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
    int readAt(uint32_t offset, uint8_t* buffer, uint32_t len) override;
    void closeRead() override;
    StorageItemInfo getItemInfo(const char* identifier = nullptr) const override;

    // --- Thao tác GHI ---
    bool openForWrite(const char* identifier) override;
    size_t writeChunk(const uint8_t* data, size_t len) override;
    void closeWrite(uint32_t maxDisplayTime = 60) override;
    void discardWrite() override;
    void setItemText(const char* identifier, const char* text) override;
    bool getItemText(const char* identifier, char* outBuf, size_t maxLen) const override;

    /// Ghi tiếp dữ liệu vào slot vừa đóng (dùng để append audio sau video)
    bool openForAppend(const char* identifier = nullptr) override;

    /// Chốt append: ghi audioSize vào bảng slot rồi flush ra NAND
    void closeAppend() override;

    // --- Quản lý Hàng chờ & Slot ---
    bool isFull() const override;
    bool getNextWriteSlotIdentifier(char* outId, size_t maxLen) override;
    bool hasUnreadMessage() const override;
    uint8_t getUnreadCount() const override;
    bool getNextUnreadIdentifier(char* outId, size_t maxLen) override;
    void markAsRead(const char* identifier) override;
    bool getFirstValidIdentifier(char* outId, size_t maxLen) const override;
    bool getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const override;

    bool formatStorage() override;

private:
    NandStorage _nand;
    Preferences _prefs;
    /// Mọi slot chưa đọc -> đầy. Suy ra từ NAND_SLOT_COUNT, không hardcode 0x1F.
    static constexpr uint8_t SLOT_ALL_MASK = (uint8_t)((1u << NAND_SLOT_COUNT) - 1u);

    uint8_t _unreadBitmask = 0;
    /// Con trỏ HÀNG CHỜ: slot kế tiếp sẽ nhận tin mới. openForAppend không được đụng vào.
    int8_t _writeSlotIndex = 0;
    /// Slot đang mở để ghi/append. writeChunk/closeWrite/closeAppend dùng biến này.
    int8_t _activeSlot = 0;
    uint32_t _writeOffset = 0;
    uint32_t _slotCapacity = 0;

    int8_t parseSlotId(const char* identifier) const;
    /// Số byte vật lý của một slot (slot cuối chạy tới hết chip 16MB)
    static uint32_t slotSpan(int8_t slot);
    void loadNvsState();
    void saveNvsState();

    int8_t   _lastWrittenSlot   = -1;
    uint32_t _lastWrittenOffset = 0;
};

#endif // NAND_STORAGE_PROVIDER_H
