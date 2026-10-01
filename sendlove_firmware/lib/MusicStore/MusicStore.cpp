#include "MusicStore.h"

#include <ArduinoJson.h>
#include <freertos/semphr.h>

#include "SDCardManager.h"
#include "ScreenLogger.h"
#include "SdStore.h"
#include "config.h"

namespace MusicStore {

namespace {

struct Entry {
    char id[24];
    uint32_t rev, size, crc, used;
};

constexpr const char* INDEX_PATH = "/alarm/index.json";

Entry s_items[ALARM_MUSIC_MAX_TRACKS];
size_t s_count = 0;
SemaphoreHandle_t s_mutex = nullptr;

void lock() {
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
}
void unlock() { xSemaphoreGive(s_mutex); }

int findLocked(const char* id) {
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_items[i].id, id) == 0) return (int)i;
    }
    return -1;
}

bool saveLocked() {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    for (size_t i = 0; i < s_count; i++) {
        JsonObject o = root[s_items[i].id].to<JsonObject>();
        o["rev"] = s_items[i].rev;
        o["size"] = s_items[i].size;
        o["crc"] = s_items[i].crc;
        o["used"] = s_items[i].used;
    }
    String body;
    serializeJson(doc, body);
    return SdStore::writeAtomic(INDEX_PATH, (const uint8_t*)body.c_str(), body.length());
}

void removeLocked(int idx) {
    char path[48];
    pathFor(s_items[idx].id, path, sizeof(path));
    SdStore::remove(path);
    for (size_t i = (size_t)idx; i + 1 < s_count; i++) s_items[i] = s_items[i + 1];
    s_count--;
}

}  // namespace

void pathFor(const char* id, char* out, size_t maxLen) {
    snprintf(out, maxLen, "/alarm/m_%s.aud", id);
}

void load() {
    lock();
    s_count = 0;
    char* buf = (char*)malloc(2048);
    if (buf) {
        int32_t n = SdStore::readText(INDEX_PATH, buf, 2048);
        if (n > 0) {
            JsonDocument doc;
            if (!deserializeJson(doc, buf)) {
                for (JsonPair kv : doc.as<JsonObject>()) {
                    if (s_count >= ALARM_MUSIC_MAX_TRACKS) break;
                    if (strlen(kv.key().c_str()) >= sizeof(s_items[0].id)) continue;
                    Entry& e = s_items[s_count];
                    strncpy(e.id, kv.key().c_str(), sizeof(e.id) - 1);
                    e.id[sizeof(e.id) - 1] = '\0';
                    e.rev = kv.value()["rev"] | 0u;
                    e.size = kv.value()["size"] | 0u;
                    e.crc = kv.value()["crc"] | 0u;
                    e.used = kv.value()["used"] | 0u;
                    // Check the size at load (cheap), not the crc (expensive). A wrong file -> download again.
                    char path[48];
                    pathFor(e.id, path, sizeof(path));
                    if (SdStore::fileSize(path) == (int32_t)e.size) s_count++;
                }
            }
        }
        free(buf);
    }
    unlock();
    DLOG("[MUS] %u bai tren the", (unsigned)s_count);
}

bool has(const char* id, uint32_t rev) {
    if (!id || !id[0]) return false;
    lock();
    int i = findLocked(id);
    bool ok = i >= 0 && (rev == 0 || s_items[i].rev == rev);
    unlock();
    return ok;
}

bool put(const char* id, uint32_t rev, uint32_t size, uint32_t crc) {
    lock();
    int i = findLocked(id);
    if (i < 0) {
        if (s_count >= ALARM_MUSIC_MAX_TRACKS) {
            // Full: evict the least recently used track (usually one no alarm uses anymore).
            size_t oldest = 0;
            for (size_t k = 1; k < s_count; k++) {
                if (s_items[k].used < s_items[oldest].used) oldest = k;
            }
            removeLocked((int)oldest);
        }
        i = (int)s_count++;
        strncpy(s_items[i].id, id, sizeof(s_items[i].id) - 1);
        s_items[i].id[sizeof(s_items[i].id) - 1] = '\0';
    }
    s_items[i].rev = rev;
    s_items[i].size = size;
    s_items[i].crc = crc;
    s_items[i].used = (uint32_t)time(nullptr);
    bool ok = saveLocked();
    unlock();
    return ok;
}

void touch(const char* id) {
    lock();
    int i = findLocked(id);
    if (i >= 0) s_items[i].used = (uint32_t)time(nullptr);
    unlock();
}

void pruneExcept(const char (*keepIds)[24], size_t keepCount) {
    lock();
    bool changed = false;
    for (int i = (int)s_count - 1; i >= 0; i--) {
        bool keep = false;
        for (size_t k = 0; k < keepCount; k++) {
            if (strcmp(s_items[i].id, keepIds[k]) == 0) keep = true;
        }
        if (!keep) {
            DLOG("[MUS] xoa %s (khong con tren cloud)", s_items[i].id);
            removeLocked(i);
            changed = true;
        }
    }
    if (changed) saveLocked();
    unlock();

    // Orphan .part files (tracks deleted mid-download).
    SDCardManager* card = SdStore::card();
    if (!card) return;
    struct Ctx {
        const char (*keep)[24];
        size_t n;
    } ctx{keepIds, keepCount};
    card->listDir("/alarm", [](const char* name, bool isDir, void* p) {
        if (isDir) return;
        size_t len = strlen(name);
        if (len < 12 || strncmp(name, "m_", 2) != 0 || strcmp(name + len - 9, ".aud.part") != 0) return;
        Ctx* c = static_cast<Ctx*>(p);
        char id[24];
        size_t idLen = len - 2 - 9;
        if (idLen >= sizeof(id)) return;
        memcpy(id, name + 2, idLen);
        id[idLen] = '\0';
        for (size_t k = 0; k < c->n; k++) {
            if (strcmp(id, c->keep[k]) == 0) return;
        }
        char path[64];
        snprintf(path, sizeof(path), "/alarm/%s", name);
        SdStore::remove(path);
    }, &ctx);
}

size_t count() {
    lock();
    size_t n = s_count;
    unlock();
    return n;
}

}  // namespace MusicStore
