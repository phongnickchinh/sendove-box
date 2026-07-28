#ifndef SD_STORAGE_PROVIDER_H
#define SD_STORAGE_PROVIDER_H

#include "IStorageProvider.h"
#include "SDCardManager.h"
#include "config.h"

/// Implementation của IStorageProvider dành cho Thẻ nhớ MicroSD (FAT32)
class SDStorageProvider : public IStorageProvider {
public:
    SDStorageProvider() = default;
    virtual ~SDStorageProvider() = default;

    bool init(SemaphoreHandle_t spiMutex = nullptr) override;

    // --- Thao tác ĐỌC ---
    bool openForRead(const char* identifier) override;
    int readData(uint8_t* buffer, uint32_t len) override;
    void seek(uint32_t offset) override;
    void closeRead() override;
    StorageItemInfo getItemInfo(const char* identifier = nullptr) const override;

    // --- Thao tác GHI ---
    bool openForWrite(const char* identifier) override;
    size_t writeChunk(const uint8_t* data, size_t len) override;
    void closeWrite() override;

    // --- Quản lý Hàng chờ & Queue ---
    bool hasUnreadMessage() const override;
    bool getNextUnreadIdentifier(char* outId, size_t maxLen) override;
    void markAsRead(const char* identifier) override;
    bool getFirstValidIdentifier(char* outId, size_t maxLen) const override;
    bool getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const override;

private:
    SDCardManager _sd;
    uint8_t _csPin = PIN_NAND_CS; // Default fallback CS pin if SD CS is shared
    char _currentReadId[32] = "";

    void buildFilePath(const char* identifier, char* outPath, size_t maxLen) const;
};

#endif // SD_STORAGE_PROVIDER_H
