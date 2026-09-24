#include "ThemeStore.h"

#include <ArduinoJson.h>
#include <esp_partition.h>

#include "SDCardManager.h"
#include "ScreenLogger.h"
#include "SdStore.h"

namespace ThemeStore {

namespace {

constexpr uint8_t PART_SUBTYPE = 0x40;  // partitions_ota.csv: theme, data, 0x40
constexpr uint32_t PAYLOAD_OFF = 4096;  // sector 0 = header
constexpr uint32_t VERSION = 1;
constexpr size_t MAX_ASSETS = 4;

struct AssetEntry {
    char name[8];
    uint32_t off;  // tính từ đầu phân vùng
    uint32_t len;
};

struct Header {
    char magic[4];  // "SLT1"
    uint32_t version;
    uint32_t rev;
    char themeId[24];
    uint32_t payloadLen;
    uint32_t payloadCrc;
    uint32_t count;
    AssetEntry assets[MAX_ASSETS];
    uint32_t headerCrc;  // crc của mọi byte phía trước trường này
};

struct AssetFile {
    const char* name;
    const char* file;
    bool required;
};
constexpr AssetFile FILES[MAX_ASSETS] = {
    {"layout", "layout.json", true},
    {"bg", "bg.bin", false},
    {"f_time", "f_time.vlw", false},
    {"f_date", "f_date.vlw", false},
};

const esp_partition_t* s_part = nullptr;
spi_flash_mmap_handle_t s_map = 0;
const uint8_t* s_base = nullptr;
bool s_valid = false;
Header s_hdr{};

portMUX_TYPE s_reqMux = portMUX_INITIALIZER_UNLOCKED;
bool s_reqPending = false;
char s_reqDir[64] = "";
char s_reqId[24] = "";
uint32_t s_reqRev = 0;

void unmap() {
    if (s_map) {
        spi_flash_munmap(s_map);
        s_map = 0;
    }
    s_base = nullptr;
    s_valid = false;
}

uint32_t headerCrcOf(const Header& h) {
    return SdStore::crc32Update(0, (const uint8_t*)&h, offsetof(Header, headerCrc));
}

// mmap cả phân vùng rồi kiểm. Kiểm crc payload qua con trỏ mmap (~160KB, vài chục ms).
bool mapAndValidate() {
    unmap();
    if (!s_part) return false;
    const void* p = nullptr;
    if (esp_partition_mmap(s_part, 0, s_part->size, SPI_FLASH_MMAP_DATA, &p, &s_map) != ESP_OK) {
        s_map = 0;
        return false;
    }
    s_base = (const uint8_t*)p;
    memcpy(&s_hdr, s_base, sizeof(s_hdr));
    if (memcmp(s_hdr.magic, "SLT1", 4) != 0 || s_hdr.version != VERSION ||
        headerCrcOf(s_hdr) != s_hdr.headerCrc || s_hdr.count > MAX_ASSETS ||
        s_hdr.payloadLen > s_part->size - PAYLOAD_OFF) {
        return false;
    }
    uint32_t crc = SdStore::crc32Update(0, s_base + PAYLOAD_OFF, s_hdr.payloadLen);
    if (crc != s_hdr.payloadCrc) {
        DLOG("[THM] payload crc sai -> bo");
        return false;
    }
    s_hdr.themeId[sizeof(s_hdr.themeId) - 1] = '\0';
    s_valid = true;
    return true;
}

}  // namespace

bool begin() {
    s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)PART_SUBTYPE, "theme");
    if (!s_part) {
        DLOG("[THM] chua co phan vung theme (can nap cap)");
        return false;
    }
    bool ok = mapAndValidate();
    if (ok) DLOG("[THM] %s rev %lu", s_hdr.themeId, (unsigned long)s_hdr.rev);
    else DLOG("[THM] phan vung trong -> man du phong");
    return ok;
}

bool valid() { return s_valid; }
bool partitionPresent() { return s_part != nullptr; }
uint32_t rev() { return s_valid ? s_hdr.rev : 0; }
const char* themeId() { return s_valid ? s_hdr.themeId : ""; }

const uint8_t* asset(const char* name, uint32_t* len) {
    if (!s_valid || !name) return nullptr;
    for (uint32_t i = 0; i < s_hdr.count; i++) {
        if (strncmp(s_hdr.assets[i].name, name, sizeof(s_hdr.assets[i].name)) == 0) {
            if (len) *len = s_hdr.assets[i].len;
            return s_base + s_hdr.assets[i].off;
        }
    }
    return nullptr;
}

bool installFromSd(const char* dir, const char* id, uint32_t rev) {
    SDCardManager* card = SdStore::card();
    if (!s_part || !card || !dir || !id) return false;

    Header h{};
    memcpy(h.magic, "SLT1", 4);
    h.version = VERSION;
    h.rev = rev;
    strncpy(h.themeId, id, sizeof(h.themeId) - 1);

    // Đo trước: gói phải vừa phân vùng và đủ asset bắt buộc, không thì KHÔNG đụng flash.
    char paths[MAX_ASSETS][96];
    uint32_t off = PAYLOAD_OFF;
    for (size_t i = 0; i < MAX_ASSETS; i++) {
        snprintf(paths[i], sizeof(paths[i]), "%s/%s", dir, FILES[i].file);
        int32_t sz = card->getFileSize(paths[i]);
        if (sz <= 0) {
            if (FILES[i].required) {
                DLOG("[THM] thieu %s", FILES[i].file);
                return false;
            }
            paths[i][0] = '\0';
            continue;
        }
        AssetEntry& e = h.assets[h.count++];
        strncpy(e.name, FILES[i].name, sizeof(e.name));
        e.off = off;
        e.len = (uint32_t)sz;
        off += ((uint32_t)sz + 3u) & ~3u;  // căn 4 byte: bg đọc như mảng uint16_t
    }
    h.payloadLen = off - PAYLOAD_OFF;
    if (off > s_part->size) {
        DLOG("[THM] goi %lu B > phan vung", (unsigned long)off);
        return false;
    }

    uint8_t* buf = (uint8_t*)malloc(4096);
    if (!buf) return false;

    DLOG("[THM] cai %s rev %lu (%lu B)", id, (unsigned long)rev, (unsigned long)h.payloadLen);
    unmap();  // con trỏ asset cũ chết từ đây
    bool ok = esp_partition_erase_range(s_part, 0, (off + 4095u) & ~4095u) == ESP_OK;

    uint32_t crc = 0;
    size_t slot = 0;
    for (size_t i = 0; ok && i < MAX_ASSETS; i++) {
        if (!paths[i][0]) continue;
        const AssetEntry& e = h.assets[slot++];
        uint32_t pos = 0;
        while (ok && pos < e.len) {
            uint32_t n = e.len - pos < 4096 ? e.len - pos : 4096;
            if (card->readFileAt(paths[i], pos, buf, n) != (int32_t)n) {
                ok = false;
                break;
            }
            ok = esp_partition_write(s_part, e.off + pos, buf, n) == ESP_OK;
            crc = SdStore::crc32Update(crc, buf, n);
            pos += n;
        }
        // Byte đệm căn 4 cũng nằm trong payload (flash vừa xoá = 0xFF) -> tính vào crc.
        uint32_t pad = (((e.len + 3u) & ~3u) - e.len);
        if (ok && pad) {
            memset(buf, 0xFF, pad);
            crc = SdStore::crc32Update(crc, buf, pad);
        }
    }
    free(buf);

    if (ok) {
        h.payloadCrc = crc;
        h.headerCrc = headerCrcOf(h);
        ok = esp_partition_write(s_part, 0, &h, sizeof(h)) == ESP_OK;  // CUỐI CÙNG
    }
    bool mapped = mapAndValidate();
    if (!ok || !mapped) {
        DLOG("[THM] cai FAIL");
        return false;
    }

    // Ghi nhớ gói đang dùng: boot mà flash trống thì cài lại từ đây (restoreFromSdIfNeeded).
    JsonDocument doc;
    doc["dir"] = dir;
    doc["id"] = id;
    doc["rev"] = rev;
    String body;
    serializeJson(doc, body);
    SdStore::writeAtomic("/theme/active.json", (const uint8_t*)body.c_str(), body.length());
    DLOG("[THM] da cai %s rev %lu", id, (unsigned long)rev);
    return true;
}

void requestInstall(const char* dir, const char* id, uint32_t rev) {
    portENTER_CRITICAL(&s_reqMux);
    strncpy(s_reqDir, dir, sizeof(s_reqDir) - 1);
    s_reqDir[sizeof(s_reqDir) - 1] = '\0';
    strncpy(s_reqId, id, sizeof(s_reqId) - 1);
    s_reqId[sizeof(s_reqId) - 1] = '\0';
    s_reqRev = rev;
    s_reqPending = true;
    portEXIT_CRITICAL(&s_reqMux);
}

bool installPending() { return s_reqPending; }

bool takeInstallRequest(char* dir, size_t dirLen, char* id, size_t idLen, uint32_t* rev) {
    bool had = false;
    portENTER_CRITICAL(&s_reqMux);
    if (s_reqPending) {
        strncpy(dir, s_reqDir, dirLen - 1);
        dir[dirLen - 1] = '\0';
        strncpy(id, s_reqId, idLen - 1);
        id[idLen - 1] = '\0';
        *rev = s_reqRev;
        s_reqPending = false;
        had = true;
    }
    portEXIT_CRITICAL(&s_reqMux);
    return had;
}

bool restoreFromSdIfNeeded() {
    if (s_valid || !s_part) return false;
    char buf[160];
    if (SdStore::readText("/theme/active.json", buf, sizeof(buf)) <= 0) return false;
    JsonDocument doc;
    if (deserializeJson(doc, buf)) return false;
    const char* dir = doc["dir"] | "";
    const char* id = doc["id"] | "";
    uint32_t r = doc["rev"] | 0u;
    if (!dir[0] || !id[0]) return false;
    DLOG("[THM] flash trong, cai lai tu the");
    return installFromSd(dir, id, r);
}

}  // namespace ThemeStore
