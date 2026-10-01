#ifndef I_STORAGE_PROVIDER_H
#define I_STORAGE_PROVIDER_H

#include <Arduino.h>

/// Kind of a media slot / file
enum class StorageItemType : uint8_t {
    UNKNOWN,
    VIDEO,
    IMAGE,
    EMPTY
};

/// Metadata of one media item
struct StorageItemInfo {
    StorageItemType type = StorageItemType::UNKNOWN;
    uint32_t dataSize = 0;      // video/image part only
    uint32_t audioSize = 0;     // audio appended after the video (incl. the AUDC header); 0 = none
    uint16_t fps = 10;
    uint16_t totalFrames = 0;
    char id[32] = "";
    uint32_t maxDisplayTime = 60;
};

/// Abstract interface for every storage backend (NAND flash / SD card)
class IStorageProvider {
public:
    virtual ~IStorageProvider() = default;

    /// Initialize the storage hardware with the shared SPI mutex
    virtual bool init(SemaphoreHandle_t spiMutex = nullptr) = 0;

    // --- READ operations (media player) ---

    /// Open an item by ID (or a slot index as a string: "0", "1"...) for reading
    virtual bool openForRead(const char* identifier) = 0;

    /// Read bytes from the open item
    virtual int readData(uint8_t* buffer, uint32_t len) = 0;

    /// Move the read cursor to an offset
    virtual void seek(uint32_t offset) = 0;

    /// Read at an absolute offset in the open item, NOT limited by dataSize and
    /// WITHOUT touching the sequential read cursor. Needed for the audio region
    /// appended after the video — with seek()+readData(), AudioPlayer and
    /// MediaPlayer would trample each other. Default: returns 0 (unsupported).
    virtual int readAt(uint32_t offset, uint8_t* buffer, uint32_t len) {
        (void)offset; (void)buffer; (void)len; return 0;
    }

    /// Close the item being read
    virtual void closeRead() = 0;

    /// Metadata of the open item, or of the item with the given ID
    virtual StorageItemInfo getItemInfo(const char* identifier = nullptr) const = 0;

    // --- WRITE operations (file downloader) ---

    /// Open an item by ID to write / overwrite it
    virtual bool openForWrite(const char* identifier) = 0;

    /// Append a chunk to the open item
    virtual size_t writeChunk(const uint8_t* data, size_t len) = 0;

    /// Close the item being written
    virtual void closeWrite(uint32_t maxDisplayTime = 60) = 0;

    /// Abandon an unfinished write (download error / stall): does NOT commit the
    /// slot table and does NOT mark unread — unlike closeWrite(). Default: no-op.
    virtual void discardWrite() { }

    /// Store caption text (truncated to the internal buffer length) on a finished
    /// item (after closeWrite()/closeAppend()). Default: no-op.
    virtual void setItemText(const char* identifier, const char* text) { (void)identifier; (void)text; }

    /// Read an item's caption text. Returns true if there is text; false by default.
    virtual bool getItemText(const char* identifier, char* outBuf, size_t maxLen) const {
        (void)identifier; (void)outBuf; (void)maxLen; return false;
    }

    /// Keep writing into the slot just closed, without erasing (appends audio after
    /// the video). Default: no-op.
    virtual bool openForAppend(const char* identifier = nullptr) { (void)identifier; return false; }

    /// Commit the appended part: records the audio size in the slot table. Without
    /// this call the audio data is on storage but nobody knows how long it is.
    virtual void closeAppend() {}

    // --- Queue management & item iteration ---

    /// Whether storage is full of unread messages
    virtual bool isFull() const = 0;

    /// ID of the next slot allowed for writing (returns false if storage is full)
    virtual bool getNextWriteSlotIdentifier(char* outId, size_t maxLen) = 0;

    /// Whether any message / item is unread
    virtual bool hasUnreadMessage() const = 0;

    /// Number of unread messages
    virtual uint8_t getUnreadCount() const = 0;

    /// ID of the next unread item (oldest first)
    virtual bool getNextUnreadIdentifier(char* outId, size_t maxLen) = 0;

    /// Mark an item as read
    virtual void markAsRead(const char* identifier) = 0;

    /// ID of the first valid item in storage (for fallback)
    virtual bool getFirstValidIdentifier(char* outId, size_t maxLen) const = 0;

    /// ID of the next valid item
    virtual bool getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const = 0;

    /// Erase all storage data (factory reset / clear NAND)
    virtual bool formatStorage() { return false; }

    // --- General files on the card (theme, alarm music, log) ---

    /// The underlying SD card, so SdStore can read/write arbitrary files. nullptr =
    /// storage isn't a card (NAND build): every card-based feature turns itself off.
    virtual class SDCardManager* sdCard() { return nullptr; }

    /// Remount the card + reload the manifest (card just reinserted). Only call while not playing.
    virtual bool remount() { return false; }
};

#endif // I_STORAGE_PROVIDER_H
