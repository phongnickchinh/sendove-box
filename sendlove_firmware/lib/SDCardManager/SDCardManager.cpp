#include "SDCardManager.h"

#include "ScreenLogger.h"
#include "config.h"

// ============================================================================
// SDCardManager Implementation
// ============================================================================
// MANDATORY RULES in this file:
//  R1: DLOG() MUST stay OUTSIDE any region holding the mutex. ScreenLogger::render()
//      takes this same _spiMutex (non-recursive, 50ms timeout) -> a stray DLOG in
//      appendChunk() costs 50ms per 2KB chunk and looks exactly like a network fault.
//  R2: Every SD.* / File.* call must sit inside acquireSPI()/releaseSPI(). The
//      ST7789 has no CS pin -> unguarded SD traffic corrupts the frame being
//      pushed to the screen.
//  R3: No File may be destructed outside the mutex. Always close() explicitly
//      inside the locked region; close() sets _f = null, so the later destructor
//      is harmless.
//  R4: Never nest acquireSPI() (the mutex is non-recursive).
// ============================================================================

bool SDCardManager::init(uint8_t csPin, SemaphoreHandle_t spiMutex) {
    _csPin = csPin;
    _spiMutex = spiMutex;
    _mounted = false;

    // A soft reset (RST button, OTA, WDT) does NOT power-cycle the card: the card
    // may have been mid-read when the chip reset, so the first SD.begin() after a
    // reset often fails although the card is fine. A failure leaves the box running
    // empty until it is unplugged: isFull() = true and unread = 0 -> the log line
    // "msg skip: het slot, unread=0" on every sync. A failed SD.begin() cleans up
    // after itself (_pdrv = 0xFF), so it can be retried right away.
    bool ok = false;
    uint32_t cardMB = 0;
    uint8_t attempt = 0;
    for (; attempt < 3 && !ok; attempt++) {
        if (attempt > 0) delay(200); // the mutex isn't held while waiting
        if (!acquireSPI()) return false;

        // SD.begin() defaults to 4MHz -> too slow for 15fps video. The library drops
        // to 400kHz during init and then uses this value. max_files = 7: 4 resident
        // handles (message write + sequential read + random read + general file
        // write) plus the temporary handle of readFile/readFileAt/listDir.
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
    if (!lastSlash || lastSlash == path) return; // File nam o thu muc goc

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
        // A small, known buffer: a full or removed card surfaces from fwrite within
        // 512B instead of at some unpredictable flush -> writeChunk's "returns a
        // short count" contract becomes trustworthy.
        // REMOVE THIS ONE LINE if download speed drops.
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

    // "r+" opens read-write WITHOUT truncating (unlike FILE_WRITE = "w" = O_TRUNC).
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
    // NO DLOG here (R1): this runs every 2KB throughout a download.
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

    // The hard limit is the PHYSICAL file size, not dataSize — the audio region
    // sits after dataSize, which is the very reason readAt() exists.
    if (offset >= _atSize) return 0;
    if ((uint64_t)offset + len > _atSize) len = _atSize - offset;

    if (!acquireSPI()) return 0;

    bool ok = true;
    // Skip the seek when already in position: AudioPlayer reads monotonically in
    // AUDIO_READ_CHUNK_SIZE steps, and seeking every time would defeat stdio's readahead.
    if (!_atPosKnown || offset != _atPos) {
        ok = _atFile.seek(offset);
        _atPosKnown = ok;
    }

    size_t n = ok ? _atFile.read(buffer, len) : 0;
    if (n > 0) {
        _atPos = offset + (uint32_t)n;
        _atPosKnown = true;
    } else {
        _atPosKnown = false; // position no longer certain -> seek again next time
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
    // The caller guarantees no handle is in use (only called in STANDBY, not playing).
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
    // Open + actually read one byte: SD.cardType()/totalBytes() only return values
    // cached at mount and still look normal after the card is removed.
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
    // Copy the names out first, then call cb OUTSIDE the mutex (R4: cb may delete/rename files).
    static constexpr size_t MAX_ENTRIES = 24;
    static constexpr size_t NAME_LEN = 40;  // NOT named NAME_MAX: that clashes with a limits.h macro
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
    // usedBytes() scans the FAT when FSINFO is invalid -> it can take seconds on a
    // large card. The caller (SdStore) only calls it at mount and after each download, then caches it.
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
    if (ok) _genFile.setBufferSize(512);  // same reason as in openFileForWrite
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
    if (_spiMutex == nullptr) return true; // no mutex → nothing to take

    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // NOP hack for the ST7789 (it has no CS pin, so it sees every byte on the
        // bus): send 2 NOP commands (0x00) with TFT_DC = LOW, then pull TFT_DC HIGH.
        // The display enters data mode and swallows all SD traffic as the NOP's
        // pixel data instead of mistaking it for commands. Mirrors
        // NandStorage::acquireSPI().
        //
        // SPI_BUS_MODE, NOT a literal SPI_MODE3: a transaction in a mode different
        // from LovyanGFX's would RECREATE the very CPOL flip this is here to avoid
        // (SCK's idle level jumps -> one spurious rising edge -> byte framing lost).
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
    // Safe: the mutex isn't held on this branch, so DLOG can't self-deadlock (R1).
    DLOG("[SD] ERR: SPI mutex timeout");
    return false;
}

void SDCardManager::releaseSPI() const {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}
