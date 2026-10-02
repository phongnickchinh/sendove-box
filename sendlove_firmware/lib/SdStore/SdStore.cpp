#include "SdStore.h"

#include "IStorageProvider.h"
#include "SDCardManager.h"
#include "ScreenLogger.h"

namespace SdStore {

std::atomic<uint32_t> mountEpoch{0};

namespace {

IStorageProvider* s_storage = nullptr;
SDCardManager* s_card = nullptr;
std::atomic<uint32_t> s_freeMB{0};

constexpr const char* LAYOUT_PATH = "/sys/layout.json";
constexpr const char* LAYOUT_JSON = "{\"schema\":1}";

bool ready() { return s_card != nullptr && s_card->isMounted(); }

// --- Clean up leftover .tmp files (power lost during writeAtomic) ---

struct CleanCtx {
    const char* dir;
};

void cleanEntry(const char* name, bool isDir, void* ctx) {
    const char* dir = static_cast<CleanCtx*>(ctx)->dir;
    char path[96];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (isDir) {
        // One level down (theme packages /theme/t_<id>/) is enough; the tree goes no deeper.
        CleanCtx sub{path};
        s_card->listDir(path, [](const char* n, bool d, void* c) {
            if (!d) cleanEntry(n, false, c);
        }, &sub);
        return;
    }
    size_t len = strlen(name);
    if (len < 5 || strcmp(name + len - 4, ".tmp") != 0) return;
    char base[96];
    snprintf(base, sizeof(base), "%s/%.*s", dir, (int)(len - 4), name);
    if (s_card->fileExists(base)) {
        s_card->deleteFile(path);  // the .tmp write didn't finish -> the old file is still correct
    } else {
        s_card->renameFile(path, base);  // died after deleting the old file -> the .tmp is the correct one
    }
}

void bootCleanup() {
    static const char* const DIRS[] = {"/sys", "/theme", "/alarm"};
    for (const char* d : DIRS) {
        CleanCtx ctx{d};
        s_card->listDir(d, cleanEntry, &ctx);
    }
}

void ensureLayout() {
    s_card->makeDir("/sys");
    s_card->makeDir("/sys/log");
    s_card->makeDir("/theme");
    s_card->makeDir("/alarm");
    // New / empty card: create the layout, do NOT format, leave any unknown files alone.
    if (!s_card->fileExists(LAYOUT_PATH)) {
        writeAtomic(LAYOUT_PATH, (const uint8_t*)LAYOUT_JSON, strlen(LAYOUT_JSON));
    }
}

void onMounted() {
    bootCleanup();
    ensureLayout();
    refreshFree();
    DLOG("[SDS] san sang, trong %lu MB", (unsigned long)s_freeMB.load());
}

}  // namespace

void begin(IStorageProvider* storage) {
    s_storage = storage;
    s_card = storage ? storage->sdCard() : nullptr;
    if (!s_card) return;
    if (s_card->isMounted()) onMounted();
}

State state() {
    if (!s_card) return State::NONE;
    return s_card->isMounted() ? State::READY : State::ABSENT;
}

const char* stateName() {
    switch (state()) {
        case State::READY: return "ok";
        case State::ABSENT: return "absent";
        default: return "none";
    }
}

SDCardManager* card() { return ready() ? s_card : nullptr; }

bool tryRemount() {
    if (!s_card || !s_storage || s_card->isMounted()) return false;
    if (!s_storage->remount()) return false;
    onMounted();
    mountEpoch++;
    DLOG("[SDS] the da cam lai");
    return true;
}

void noteIoError() {
    if (ready()) s_card->probe();
}

uint32_t freeMB() { return s_freeMB.load(); }

void refreshFree() { s_freeMB = ready() ? s_card->freeMB() : 0; }

bool writeAtomic(const char* path, const uint8_t* data, size_t len) {
    if (!ready()) return false;
    char tmp[96];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (s_card->writeFile(tmp, data, len) != (int32_t)len) {
        s_card->deleteFile(tmp);
        noteIoError();
        return false;
    }
    s_card->deleteFile(path);
    return s_card->renameFile(tmp, path);
}

int32_t readText(const char* path, char* buf, size_t maxLen) {
    if (!ready() || !buf || maxLen < 2) return -1;
    int32_t n = s_card->readFile(path, (uint8_t*)buf, maxLen - 1);
    if (n < 0) return -1;
    buf[n] = '\0';
    return n;
}

bool exists(const char* path) { return ready() && s_card->fileExists(path); }

bool remove(const char* path) { return ready() && s_card->deleteFile(path); }

bool removeTree(const char* dir) {
    if (!ready()) return false;
    s_card->listDir(dir, [](const char* name, bool isDir, void* ctx) {
        if (isDir) return;
        char path[96];
        snprintf(path, sizeof(path), "%s/%s", static_cast<const char*>(ctx), name);
        s_card->deleteFile(path);
    }, (void*)dir);
    return s_card->removeDir(dir);
}

int32_t fileSize(const char* path) { return ready() ? s_card->getFileSize(path) : -1; }

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
    // No lookup table (saves 1KB of resident RAM). 2MB of music takes ~0.5s and only runs after a download.
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c ^= data[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c ^ 0xFFFFFFFFu;
}

uint32_t crc32File(const char* path, uint32_t size, bool* ok) {
    if (ok) *ok = false;
    if (!ready()) return 0;
    // Do NOT use the random-read handle: a ringing alarm holds it. Open-read-close
    // per 4KB block is slower but disturbs nobody.
    static constexpr size_t BUF = 4096;
    uint8_t* buf = (uint8_t*)malloc(BUF);
    if (!buf) return 0;
    uint32_t crc = 0, off = 0;
    bool good = true;
    while (off < size) {
        uint32_t want = (size - off < BUF) ? (size - off) : BUF;
        int32_t n = s_card->readFileAt(path, off, buf, want);
        if (n <= 0) {
            good = false;
            break;
        }
        crc = crc32Update(crc, buf, (size_t)n);
        off += (uint32_t)n;
    }
    free(buf);
    if (ok) *ok = good;
    return crc;
}

}  // namespace SdStore
