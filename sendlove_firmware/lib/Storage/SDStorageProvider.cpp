#include "SDStorageProvider.h"

void SDStorageProvider::buildFilePath(const char* identifier, char* outPath, size_t maxLen) const {
    if (!identifier || identifier[0] == '\0') {
        snprintf(outPath, maxLen, "/media/default.bin");
        return;
    }

    if (identifier[0] == '/') {
        snprintf(outPath, maxLen, "%s", identifier);
    } else if (strncmp(identifier, "slot_", 5) == 0) {
        snprintf(outPath, maxLen, "/media/slot_%s.bin", identifier + 5);
    } else {
        snprintf(outPath, maxLen, "/media/%s.bin", identifier);
    }
}

bool SDStorageProvider::init(SemaphoreHandle_t spiMutex) {
    return _sd.init(_csPin, spiMutex);
}

bool SDStorageProvider::openForRead(const char* identifier) {
    char path[64];
    buildFilePath(identifier, path, sizeof(path));
    if (identifier) {
        strncpy(_currentReadId, identifier, sizeof(_currentReadId) - 1);
    }
    return _sd.openFileForRead(path);
}

int SDStorageProvider::readData(uint8_t* buffer, uint32_t len) {
    return _sd.readBlock(buffer, len);
}

void SDStorageProvider::seek(uint32_t offset) {
    // SDCardManager hiện tại đọc tuần tự
}

void SDStorageProvider::closeRead() {
    _sd.closeReadFile();
    _currentReadId[0] = '\0';
}

StorageItemInfo SDStorageProvider::getItemInfo(const char* identifier) const {
    StorageItemInfo info;
    char path[64];
    buildFilePath(identifier ? identifier : _currentReadId, path, sizeof(path));

    info.dataSize = _sd.getFileSize(path);
    if (info.dataSize > 0) {
        info.type = StorageItemType::VIDEO; // Default type for SD binaries
        info.fps = 15;
        strncpy(info.id, identifier ? identifier : _currentReadId, sizeof(info.id) - 1);
    } else {
        info.type = StorageItemType::EMPTY;
    }
    return info;
}

bool SDStorageProvider::openForWrite(const char* identifier) {
    char path[64];
    buildFilePath(identifier, path, sizeof(path));
    return _sd.openFileForWrite(path);
}

size_t SDStorageProvider::writeChunk(const uint8_t* data, size_t len) {
    return _sd.appendChunk(data, len);
}

void SDStorageProvider::closeWrite() {
    _sd.closeWriteFile();
}

bool SDStorageProvider::hasUnreadMessage() const {
    // Phase 3A: Mặc định kiểm tra file slot_0
    char path[64];
    buildFilePath("slot_0", path, sizeof(path));
    return _sd.fileExists(path);
}

bool SDStorageProvider::getNextUnreadIdentifier(char* outId, size_t maxLen) {
    if (!outId || maxLen == 0) return false;
    snprintf(outId, maxLen, "slot_0");
    return true;
}

void SDStorageProvider::markAsRead(const char* identifier) {
}

bool SDStorageProvider::getFirstValidIdentifier(char* outId, size_t maxLen) const {
    if (!outId || maxLen == 0) return false;
    snprintf(outId, maxLen, "0");
    return true;
}

bool SDStorageProvider::getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const {
    if (!outId || maxLen == 0) return false;
    int curr = currentId ? atoi(currentId) : 0;
    snprintf(outId, maxLen, "%d", (curr + 1) % 5);
    return true;
}
