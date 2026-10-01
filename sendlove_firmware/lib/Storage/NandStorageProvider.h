#ifndef NAND_STORAGE_PROVIDER_H
#define NAND_STORAGE_PROVIDER_H

#include "IStorageProvider.h"
#include "NandStorage.h"
#include "Preferences.h"

/// IStorageProvider implementation for the 16MB W25Q128 flash chip
class NandStorageProvider : public IStorageProvider {
public:
    NandStorageProvider() = default;
    virtual ~NandStorageProvider() = default;

    bool init(SemaphoreHandle_t spiMutex = nullptr) override;

    // --- READ operations ---
    bool openForRead(const char* identifier) override;
    int readData(uint8_t* buffer, uint32_t len) override;
    void seek(uint32_t offset) override;
    int readAt(uint32_t offset, uint8_t* buffer, uint32_t len) override;
    void closeRead() override;
    StorageItemInfo getItemInfo(const char* identifier = nullptr) const override;

    // --- WRITE operations ---
    bool openForWrite(const char* identifier) override;
    size_t writeChunk(const uint8_t* data, size_t len) override;
    void closeWrite(uint32_t maxDisplayTime = 60) override;
    void discardWrite() override;
    void setItemText(const char* identifier, const char* text) override;
    bool getItemText(const char* identifier, char* outBuf, size_t maxLen) const override;

    /// Keep writing into the slot just closed (appends audio after the video)
    bool openForAppend(const char* identifier = nullptr) override;

    /// Commit the append: store audioSize in the slot table and flush it to flash
    void closeAppend() override;

    // --- Queue & slot management ---
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
    /// All slots unread -> full. Derived from NAND_SLOT_COUNT, not a hardcoded 0x1F.
    static constexpr uint8_t SLOT_ALL_MASK = (uint8_t)((1u << NAND_SLOT_COUNT) - 1u);

    uint8_t _unreadBitmask = 0;
    /// The QUEUE cursor: the next slot to receive a message. openForAppend must not touch it.
    int8_t _writeSlotIndex = 0;
    /// The slot open for write/append. Used by writeChunk/closeWrite/closeAppend.
    int8_t _activeSlot = 0;
    uint32_t _writeOffset = 0;
    uint32_t _slotCapacity = 0;
    /// Erase-as-you-write: the absolute address erased so far in the slot being
    /// written. openForWrite() erases only the first 64KB block (erasing a whole
    /// ~5.3MB slot blocks synchronously for 15-25s, long enough for the HTTP socket
    /// to hit TCP zero-window/timeout). writeChunk() erases further blocks as the
    /// write cursor approaches them.
    uint32_t _erasedUpToAddr = 0;

    int8_t parseSlotId(const char* identifier) const;
    /// Physical size of a slot in bytes (the last slot runs to the end of the 16MB chip)
    static uint32_t slotSpan(int8_t slot);
    void loadNvsState();
    void saveNvsState();

    int8_t   _lastWrittenSlot   = -1;
    uint32_t _lastWrittenOffset = 0;
};

#endif // NAND_STORAGE_PROVIDER_H
