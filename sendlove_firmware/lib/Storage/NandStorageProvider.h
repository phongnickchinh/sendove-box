#ifndef NAND_STORAGE_PROVIDER_H
#define NAND_STORAGE_PROVIDER_H

#include "IStorageProvider.h"
#include "NandStorage.h"
#include "Preferences.h"

/// Implementation của IStorageProvider dành cho chip W25Q128 NAND Flash 16MB
class NandStorageProvider : public IStorageProvider {
public:
    NandStorageProvider() = default;
    virtual ~NandStorageProvider() = default;

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

    // --- Quản lý Hàng chờ & Slot ---
    bool isFull() const override;
    bool getNextWriteSlotIdentifier(char* outId, size_t maxLen) override;
    bool hasUnreadMessage() const override;
    bool getNextUnreadIdentifier(char* outId, size_t maxLen) override;
    void markAsRead(const char* identifier) override;
    bool getFirstValidIdentifier(char* outId, size_t maxLen) const override;
    bool getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const override;

    bool formatStorage() override;

private:
    NandStorage _nand;
    Preferences _prefs;
    uint8_t _unreadBitmask = 0;
    int8_t _writeSlotIndex = 0;
    uint32_t _writeOffset = 0;
    uint32_t _lastErasedSectorAddr = 0xFFFFFFFF;

    int8_t parseSlotId(const char* identifier) const;
    void loadNvsState();
    void saveNvsState();
};

#endif // NAND_STORAGE_PROVIDER_H
