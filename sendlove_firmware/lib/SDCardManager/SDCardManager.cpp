#include "SDCardManager.h"

// ============================================================================
// SDCardManager Implementation
// ============================================================================

bool SDCardManager::init(uint8_t csPin, SemaphoreHandle_t spiMutex) {
    _csPin = csPin;
    _spiMutex = spiMutex;

    if (!acquireSPI()) return false;

    bool ok = SD.begin(_csPin);

    releaseSPI();

    if (!ok) {
        Serial.println(F("[SDCard] ERROR: Failed to mount SD card"));
        return false;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SDCard] Mounted. Card size: %llu MB\n", cardSize);
    return true;
}

// --- Write Operations ---

int32_t SDCardManager::writeFile(const char* path, const uint8_t* data, size_t len) {
    if (!acquireSPI()) return -1;

    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[SDCard] ERROR: Cannot open %s for writing\n", path);
        releaseSPI();
        return -1;
    }

    size_t written = file.write(data, len);
    file.close();
    releaseSPI();

    Serial.printf("[SDCard] Wrote %u bytes to %s\n", written, path);
    return (int32_t)written;
}

bool SDCardManager::openFileForWrite(const char* path) {
    if (!acquireSPI()) return false;

    // Tạo thư mục cha nếu chưa tồn tại
    String dirPath = String(path);
    int lastSlash = dirPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = dirPath.substring(0, lastSlash);
        if (!SD.exists(dir.c_str())) {
            SD.mkdir(dir.c_str());
        }
    }

    _writeFile = SD.open(path, FILE_WRITE);
    releaseSPI();

    if (!_writeFile) {
        Serial.printf("[SDCard] ERROR: Cannot open %s for stream write\n", path);
        return false;
    }

    Serial.printf("[SDCard] Opened %s for stream write\n", path);
    return true;
}

size_t SDCardManager::appendChunk(const uint8_t* data, size_t len) {
    if (!_writeFile) return 0;
    if (!acquireSPI()) return 0;

    size_t written = _writeFile.write(data, len);

    releaseSPI();
    return written;
}

void SDCardManager::closeWriteFile() {
    if (_writeFile) {
        if (acquireSPI()) {
            _writeFile.close();
            releaseSPI();
        }
        Serial.println(F("[SDCard] Write file closed"));
    }
}

// --- Read Operations ---

bool SDCardManager::openFileForRead(const char* path) {
    if (!acquireSPI()) return false;

    _readFile = SD.open(path, FILE_READ);

    releaseSPI();

    if (!_readFile) {
        Serial.printf("[SDCard] ERROR: Cannot open %s for reading\n", path);
        return false;
    }

    Serial.printf("[SDCard] Opened %s for reading (%u bytes)\n",
                  path, _readFile.size());
    return true;
}

size_t SDCardManager::readBlock(uint8_t* buffer, size_t len) {
    if (!_readFile) return 0;
    if (!acquireSPI()) return 0;

    size_t bytesRead = _readFile.read(buffer, len);

    releaseSPI();
    return bytesRead;
}

void SDCardManager::closeReadFile() {
    if (_readFile) {
        if (acquireSPI()) {
            _readFile.close();
            releaseSPI();
        }
        Serial.println(F("[SDCard] Read file closed"));
    }
}

// --- Utility ---

bool SDCardManager::fileExists(const char* path) const {
    if (!acquireSPI()) return false;
    bool exists = SD.exists(path);
    releaseSPI();
    return exists;
}

bool SDCardManager::deleteFile(const char* path) {
    if (!acquireSPI()) return false;
    bool ok = SD.remove(path);
    releaseSPI();

    if (ok) {
        Serial.printf("[SDCard] Deleted %s\n", path);
    }
    return ok;
}

int32_t SDCardManager::getFileSize(const char* path) const {
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

// --- SPI Mutex ---

bool SDCardManager::acquireSPI() const {
    if (_spiMutex == nullptr) return true; // Không có mutex → bỏ qua

    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        return true;
    }
    Serial.println(F("[SDCard] ERROR: SPI mutex timeout"));
    return false;
}

void SDCardManager::releaseSPI() const {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}

