#include "SDCardManager.h"

#include "ScreenLogger.h"
#include "config.h"

// MANDATORY RULES in this file:
//  R1: No DLOG() while holding the mutex: ScreenLogger::render() takes the same
//      non-recursive _spiMutex (50ms timeout per call).
//  R2: Every SD.* / File.* call sits inside acquireSPI()/releaseSPI(): the CS-less
//      ST7789 sees unguarded SD traffic as display data.
//  R3: close() every File explicitly inside the locked region, never by destructor.
//  R4: Never nest acquireSPI().

bool SDCardManager::init(uint8_t csPin, SemaphoreHandle_t spiMutex) {
    _csPin = csPin;
    _spiMutex = spiMutex;
    _mounted = false;

    // A soft reset doesn't power-cycle the card, so the first SD.begin() often
    // fails although the card is fine (MEMORY.md §26): retry.
    bool ok = false;
    uint32_t cardMB = 0;
    uint8_t attempt = 0;
    for (; attempt < 3 && !ok; attempt++) {
        if (attempt > 0) delay(200); // mutex not held here
        if (!acquireSPI()) return false;

        // max_files = 7: 4 resident handles (message write, sequential read, random
        // read, general write) plus temporaries.
        ok = SD.begin(_csPin, SPI, SD_SPI_FREQ_HZ, "/sd", 7, false);
        if (ok) cardMB = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));

        releaseSPI();
    }

    _mounted = ok;
    if (!ok) {
        DLOG("[SD] mount FAIL (%u lan)", (unsigned)attempt);
        return false;
    }
    DLOG("[SD] mounted %lu MB (lan %u)", (unsigned long)cardMB, (unsigned)attempt);
    return true;
}

// --- Write Operations ---

void SDCardManager::mkParentDirLocked(const char* path) {
    const char* lastSlash = strrchr(path, '/');
    if (!lastSlash || lastSlash == path) return; // file in the root directory

    char dir[64];
    size_t len = (size_t)(lastSlash - path);
    if (len >= sizeof(dir)) return;
    memcpy(dir, path, len);
    dir[len] = '\0';

    if (!SD.exists(dir)) SD.mkdir(dir);
}

int32_t SDCardManager::writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!_mounted || !data) return -1;
    if (!acquireSPI()) return -1;

    mkParentDirLocked(path);

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        releaseSPI();
        DLOG("[SD] open W fail");
        return -1;
    }

    size_t written = file.write(data, len);
    file.close();
    releaseSPI();

    return (int32_t)written;
}

int32_t SDCardManager::appendFile(const char* path, const uint8_t* data, size_t len) {
    if (!_mounted || !data) return -1;
    if (!acquireSPI()) return -1;

    mkParentDirLocked(path);

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        releaseSPI();
        DLOG("[SD] open A fail");
        return -1;
    }

    size_t written = file.write(data, len);
    file.close();
    releaseSPI();

    return (int32_t)written;
}

bool SDCardManager::openFileForWrite(const char* path) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;

    if (_writeFile) _writeFile.close();
    mkParentDirLocked(path);

    _writeFile = SD.open(path, FILE_WRITE);
    bool ok = (bool)_writeFile;
    if (ok) {
        // Small buffer: a full or removed card shows up within 512B, so a short
        // write count is trustworthy. Remove this line if download speed drops.
        _writeFile.setBufferSize(512);
    }

    releaseSPI();

    if (!ok) DLOG("[SD] open W fail");
    return ok;
}

bool SDCardManager::openFileForAppend(const char* path, uint32_t atOffset) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;

    if (_writeFile) _writeFile.close();

    // "r+": read-write WITHOUT truncating.
    _writeFile = SD.open(path, "r+");
    bool ok = (bool)_writeFile;
    if (ok) {
        _writeFile.setBufferSize(512);
        ok = _writeFile.seek(atOffset);
        if (!ok) _writeFile.close();
    }

    releaseSPI();

    if (!ok) DLOG("[SD] open A fail @%lu", (unsigned long)atOffset);
    return ok;
}

size_t SDCardManager::appendChunk(const uint8_t* data, size_t len) {
    if (!_writeFile || !data || len == 0) return 0;
    if (!acquireSPI()) return 0;

    size_t written = _writeFile.write(data, len);

    releaseSPI();
    // NO DLOG here (R1).
    return written;
}

bool SDCardManager::patchWriteFileAt0(const uint8_t* buf4) {
    if (!_writeFile || !buf4) return false;
    if (!acquireSPI()) return false;

    bool ok = _writeFile.seek(0) && (_writeFile.write(buf4, 4) == 4);

    releaseSPI();

    if (!ok) DLOG("[SD] patch size FAIL");
    return ok;
}

void SDCardManager::closeWriteFile() {
    if (!_writeFile) return;
    if (acquireSPI()) {
        _writeFile.close();
        releaseSPI();
    }
}

// --- Sequential Read ---

bool SDCardManager::openFileForRead(const char* path) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;

    if (_readFile) _readFile.close();
    _readFile = SD.open(path, FILE_READ);
    bool ok = (bool)_readFile;

    releaseSPI();

    if (!ok) DLOG("[SD] open R fail");
    return ok;
}

bool SDCardManager::seekReadFile(uint32_t offset) {
    if (!_readFile) return false;
    if (!acquireSPI()) return false;

    bool ok = _readFile.seek(offset);

    releaseSPI();
    return ok;
}

size_t SDCardManager::readBlock(uint8_t* buffer, size_t len) {
    if (!_readFile || !buffer || len == 0) return 0;
    if (!acquireSPI()) return 0;

    size_t bytesRead = _readFile.read(buffer, len);

    releaseSPI();
    return bytesRead;
}

void SDCardManager::closeReadFile() {
    if (!_readFile) return;
    if (acquireSPI()) {
        _readFile.close();
        releaseSPI();
    }
}

// --- Random Read (separate handle for AudioPlayer) ---

bool SDCardManager::openAtFile(const char* path) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;

    if (_atFile) _atFile.close();
    _atFile = SD.open(path, FILE_READ);
    bool ok = (bool)_atFile;
    _atSize = ok ? (uint32_t)_atFile.size() : 0;
    _atPos = 0;
    _atPosKnown = ok;

    releaseSPI();

    if (!ok) DLOG("[SD] open AT fail");
    return ok;
}

int SDCardManager::readAtFile(uint32_t offset, uint8_t* buffer, uint32_t len) {
    if (!_atFile || !buffer || len == 0) return 0;

    // Limit = the physical file size, not dataSize (the audio sits after it).
    if (offset >= _atSize) return 0;
    if ((uint64_t)offset + len > _atSize) len = _atSize - offset;

    if (!acquireSPI()) return 0;

    bool ok = true;
    // Skip the seek when already in position (keeps stdio's readahead).
    if (!_atPosKnown || offset != _atPos) {
        ok = _atFile.seek(offset);
        _atPosKnown = ok;
    }

    size_t n = ok ? _atFile.read(buffer, len) : 0;
    if (n > 0) {
        _atPos = offset + (uint32_t)n;
        _atPosKnown = true;
    } else {
        _atPosKnown = false; // seek again next time
    }

    releaseSPI();
    return (int)n;
}

void SDCardManager::closeAtFile() {
    _atSize = 0;
    _atPos = 0;
    _atPosKnown = false;
    if (!_atFile) return;
    if (acquireSPI()) {
        _atFile.close();
        releaseSPI();
    }
}

// --- Utility ---

bool SDCardManager::fileExists(const char* path) const {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    bool exists = SD.exists(path);
    releaseSPI();
    return exists;
}

bool SDCardManager::deleteFile(const char* path) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    bool ok = SD.exists(path) && SD.remove(path);
    releaseSPI();
    return ok;
}

int32_t SDCardManager::getFileSize(const char* path) const {
    if (!_mounted) return -1;
    if (!acquireSPI()) return -1;

    File f = SD.open(path, FILE_READ);
    if (!f) {
        releaseSPI();
        return -1;
    }
    int32_t size = (int32_t)f.size();
    f.close();
    releaseSPI();
    return size;
}

int32_t SDCardManager::readFile(const char* path, uint8_t* buf, size_t maxLen) const {
    if (!_mounted || !buf || maxLen == 0) return -1;
    if (!acquireSPI()) return -1;

    File f = SD.open(path, FILE_READ);
    if (!f) {
        releaseSPI();
        return -1;
    }
    size_t n = f.read(buf, maxLen);
    f.close();
    releaseSPI();
    return (int32_t)n;
}

// --- General files (theme, alarm music, log) ---

bool SDCardManager::remount() {
    // Caller guarantees no handle is in use (STANDBY only).
    if (acquireSPI()) {
        if (_writeFile) _writeFile.close();
        if (_readFile) _readFile.close();
        if (_atFile) _atFile.close();
        if (_genFile) _genFile.close();
        if (_mounted) SD.end();
        releaseSPI();
    }
    _atSize = 0;
    _atPos = 0;
    _atPosKnown = false;
    _mounted = false;
    return init(_csPin, _spiMutex);
}

bool SDCardManager::probe() {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    // Really read one byte: SD.cardType()/totalBytes() are cached at mount.
    bool ok = false;
    File f = SD.open("/sys/layout.json", FILE_READ);
    if (f) {
        uint8_t b;
        ok = (f.read(&b, 1) == 1);
        f.close();
    } else {
        File root = SD.open("/");
        ok = (bool)root;
        if (root) root.close();
    }
    releaseSPI();
    if (!ok) {
        _mounted = false;
        DLOG("[SD] probe fail -> coi nhu rut the");
    }
    return ok;
}

int32_t SDCardManager::readFileAt(const char* path, uint32_t offset, uint8_t* buf, size_t len) const {
    if (!_mounted || !buf || len == 0) return -1;
    if (!acquireSPI()) return -1;
    File f = SD.open(path, FILE_READ);
    if (!f) {
        releaseSPI();
        return -1;
    }
    int32_t n = -1;
    if (f.seek(offset)) n = (int32_t)f.read(buf, len);
    f.close();
    releaseSPI();
    return n;
}

bool SDCardManager::renameFile(const char* from, const char* to) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    bool ok = SD.exists(from) && !SD.exists(to) && SD.rename(from, to);
    releaseSPI();
    return ok;
}

bool SDCardManager::makeDir(const char* path) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    mkParentDirLocked(path);
    bool ok = SD.exists(path) || SD.mkdir(path);
    releaseSPI();
    return ok;
}

bool SDCardManager::removeDir(const char* path) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    bool ok = SD.rmdir(path);
    releaseSPI();
    return ok;
}

size_t SDCardManager::listDir(const char* dir, void (*cb)(const char*, bool, void*), void* ctx) {
    if (!_mounted || !cb) return 0;
    // Collect names first, then call cb OUTSIDE the mutex (R4).
    static constexpr size_t MAX_ENTRIES = 24;
    static constexpr size_t NAME_LEN = 40;  // NAME_MAX would clash with limits.h
    char names[MAX_ENTRIES][NAME_LEN];
    bool dirs[MAX_ENTRIES];
    size_t n = 0;

    if (!acquireSPI()) return 0;
    File d = SD.open(dir);
    if (d && d.isDirectory()) {
        File e = d.openNextFile();
        while (e && n < MAX_ENTRIES) {
            const char* full = e.name();
            const char* base = strrchr(full, '/');
            base = base ? base + 1 : full;
            strncpy(names[n], base, NAME_LEN - 1);
            names[n][NAME_LEN - 1] = '\0';
            dirs[n] = e.isDirectory();
            n++;
            e.close();
            e = d.openNextFile();
        }
        if (e) e.close();
    }
    if (d) d.close();
    releaseSPI();

    for (size_t i = 0; i < n; i++) cb(names[i], dirs[i], ctx);
    return n;
}

uint32_t SDCardManager::freeMB() const {
    if (!_mounted) return 0;
    if (!acquireSPI()) return 0;
    // usedBytes() may scan the FAT (seconds on a large card): SdStore caches the result.
    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    releaseSPI();
    return (uint32_t)((total > used ? total - used : 0) / (1024ULL * 1024ULL));
}

bool SDCardManager::openGenWrite(const char* path, bool append) {
    if (!_mounted) return false;
    if (!acquireSPI()) return false;
    if (_genFile) _genFile.close();
    mkParentDirLocked(path);
    _genFile = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
    bool ok = (bool)_genFile;
    if (ok) _genFile.setBufferSize(512);  // see openFileForWrite
    releaseSPI();
    if (!ok) DLOG("[SD] open G fail");
    return ok;
}

size_t SDCardManager::genWrite(const uint8_t* data, size_t len) {
    if (!_genFile || !data || len == 0) return 0;
    if (!acquireSPI()) return 0;
    size_t w = _genFile.write(data, len);
    releaseSPI();
    return w;
}

void SDCardManager::closeGenWrite() {
    if (!_genFile) return;
    if (acquireSPI()) {
        _genFile.close();
        releaseSPI();
    }
}

// --- SPI Mutex + NOP Hack ---

bool SDCardManager::acquireSPI() const {
    if (_spiMutex == nullptr) return true;

    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // NOP hack for the CS-less ST7789: 2 NOPs with TFT_DC low, then DC high, so
        // the display swallows SD traffic as data (mirrors NandStorage::acquireSPI()).
        // Use SPI_BUS_MODE, the mode LovyanGFX uses: a CPOL flip breaks byte framing.
        SPI.beginTransaction(SPISettings(SD_SPI_FREQ_HZ, MSBFIRST, SPI_BUS_MODE));
        digitalWrite(PIN_TFT_DC, LOW);
        delayMicroseconds(2);
        SPI.transfer(0x00);
        SPI.transfer(0x00);
        delayMicroseconds(2);
        digitalWrite(PIN_TFT_DC, HIGH);
        SPI.endTransaction();

        return true;
    }
    // Mutex not held here, so DLOG is safe (R1).
    DLOG("[SD] ERR: SPI mutex timeout");
    return false;
}

void SDCardManager::releaseSPI() const {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}
