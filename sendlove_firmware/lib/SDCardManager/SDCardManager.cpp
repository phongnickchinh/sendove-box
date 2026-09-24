#include "SDCardManager.h"

#include "ScreenLogger.h"
#include "config.h"

// ============================================================================
// SDCardManager Implementation
// ============================================================================
// QUY TAC BAT BUOC trong file nay:
//  R1: DLOG() PHAI nam NGOAI vung dang giu mutex. ScreenLogger::render() lay
//      chinh _spiMutex (khong de quy, timeout 50ms) -> mot DLOG dat nham trong
//      appendChunk() ton 50ms moi chunk 2KB va trong y het loi mang.
//  R2: Moi loi goi SD.* / File.* deu phai nam trong acquireSPI()/releaseSPI().
//      ST7789 khong co chan CS -> traffic SD khong duoc bao ve se pha frame
//      dang push len man hinh.
//  R3: Khong File nao duoc destruct ngoai mutex. Luon close() tuong minh trong
//      vung khoa; close() set _f = null nen destructor sau do vo hai.
//  R4: Khong acquireSPI() long nhau (mutex khong de quy).
// ============================================================================

bool SDCardManager::init(uint8_t csPin, SemaphoreHandle_t spiMutex) {
    _csPin = csPin;
    _spiMutex = spiMutex;
    _mounted = false;

    // Reset mem (nut RST, OTA, WDT) KHONG ngat dien the: the co the dang do mot
    // lenh doc thi chip reset, nen lan SD.begin() dau sau reset hay fail du the
    // van tot. Fail la hop chay rong toi luc rut dien: isFull() = true va
    // unread = 0 -> log "msg skip: het slot, unread=0" moi chu ky sync.
    // SD.begin() fail tu don dep (_pdrv = 0xFF) nen goi lai duoc ngay.
    bool ok = false;
    uint32_t cardMB = 0;
    uint8_t attempt = 0;
    for (; attempt < 3 && !ok; attempt++) {
        if (attempt > 0) delay(200); // Khong giu mutex luc cho.
        if (!acquireSPI()) return false;

        // SD.begin() mac dinh 4MHz -> qua cham cho video 15fps. Thu vien tu ha ve
        // 400kHz trong lúc init roi moi dung con so nay. max_files = 7: 4 handle
        // thuong tru (write tin + read tuan tu + read ngau nhien + ghi file tong quat)
        // cong handle tam cua readFile/readFileAt/listDir.
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
        // Buffer nho va biet truoc: loi the day / rut the lo ra tu fwrite trong
        // vong 512B thay vi o mot lan flush khong doan truoc -> hop dong "tra
        // short count" cua writeChunk moi thuc su dang tin.
        // BO 1 DONG NAY neu toc do tai tut.
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

    // "r+" mo de doc-ghi, KHONG cat file (khac FILE_WRITE = "w" = O_TRUNC).
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
    // KHONG DLOG o day (R1): ham nay chay moi 2KB trong suot qua trinh tai.
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

// --- Random Read (handle rieng cho AudioPlayer) ---

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

    // Tran cung la kich thuoc file VAT LY, khong phai dataSize — vung audio nam
    // sau dataSize, do chinh la ly do readAt() ton tai.
    if (offset >= _atSize) return 0;
    if ((uint64_t)offset + len > _atSize) len = _atSize - offset;

    if (!acquireSPI()) return 0;

    bool ok = true;
    // Bo qua seek khi da dung vi tri: AudioPlayer doc don dieu tang dan tung
    // AUDIO_READ_CHUNK_SIZE byte, seek moi lan se pha readahead cua stdio.
    if (!_atPosKnown || offset != _atPos) {
        ok = _atFile.seek(offset);
        _atPosKnown = ok;
    }

    size_t n = ok ? _atFile.read(buffer, len) : 0;
    if (n > 0) {
        _atPos = offset + (uint32_t)n;
        _atPosKnown = true;
    } else {
        _atPosKnown = false; // Vi tri khong con chac chan -> lan sau seek lai
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

// --- File tong quat (theme, nhac bao thuc, log) ---

bool SDCardManager::remount() {
    // Ben goi dam bao khong con ai dung handle nao (chi goi luc STANDBY, khong phat).
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
    // Mo + doc that mot byte: SD.cardType()/totalBytes() chi tra gia tri cache luc mount,
    // rut the roi van bao binh thuong.
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
    // Chep ten ra truoc roi moi goi cb NGOAI mutex (R4: cb co the xoa/doi ten file).
    static constexpr size_t MAX_ENTRIES = 24;
    static constexpr size_t NAME_LEN = 40;  // KHÔNG đặt NAME_MAX: trùng macro của limits.h
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
    // usedBytes() quet bang FAT khi FSINFO khong hop le -> co the ton vai giay tren the
    // lon. Ben goi (SdStore) chi goi luc mount va sau moi lan tai xong, roi cache lai.
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
    if (ok) _genFile.setBufferSize(512);  // cung ly do voi openFileForWrite
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
    if (_spiMutex == nullptr) return true; // Không có mutex → bỏ qua

    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // NOP Hack cho ST7789 (khong co chan CS nen an moi byte tren bus):
        // gui 2 lenh NOP (0x00) voi TFT_DC = LOW roi keo TFT_DC = HIGH. Man hinh
        // chuyen sang Data Mode va nuot toan bo traffic SD nhu du lieu pixel cua
        // lenh NOP thay vi hieu nham thanh lenh dieu khien. Doi xung voi
        // NandStorage::acquireSPI().
        //
        // SPI_BUS_MODE chu KHONG phai literal SPI_MODE3: transaction nay ma dung
        // mode khac voi LovyanGFX se TAI TAO dung cu lat CPOL ma ca thay doi nay
        // ton tai de khu (SCK nhay muc idle -> 1 suon len gia -> lech khung byte).
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
    // An toan: khong giu mutex o nhanh nay nen DLOG khong tu-khoa (R1).
    DLOG("[SD] ERR: SPI mutex timeout");
    return false;
}

void SDCardManager::releaseSPI() const {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}
