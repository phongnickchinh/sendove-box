#ifndef SD_STORAGE_PROVIDER_H
#define SD_STORAGE_PROVIDER_H

#include "IStorageProvider.h"
#include "SDCardManager.h"
#include "config.h"

// On-card layout:
//   /media/index.bin      manifest: the queue + per-slot metadata (one unread byte per slot)
//   /media/slot_NN.bin    media, laid out EXACTLY like a NAND slot:
//                           [4B size][16B container header][payload][AUDC + audio]
//   /media/slot_NN.txt    caption sidecar (only when the message has text); NOT in
//                         the manifest, which would cost 5KB of resident RAM

static constexpr uint32_t SD_MANIFEST_MAGIC = 0x324D4453; // "SDM2"
static constexpr uint16_t SD_MANIFEST_VERSION = 1;

/// One slot's metadata. 24 bytes; every field is naturally aligned, so no hidden padding.
struct SdSlotEntry {
    char magic[4];           // "VJPG" | "VIMG" | {0,0,0,0} = empty
    uint32_t dataSize;       // INCLUDES the 4-byte prefix — same as NAND's SlotEntry.dataSize
    uint32_t audioSize;      // INCLUDES the 10-byte AUDC header; 0 = no audio
    uint32_t maxDisplayTime; // seconds
    uint16_t fps;
    uint16_t totalFrames;
    uint8_t unread;          // 0/1
    uint8_t _pad[3];
};

struct SdManifest {
    uint32_t magic;
    uint16_t version;
    uint8_t slotCount;      // must match SD_SLOT_COUNT; rejects cards from an older build
    int8_t writeSlotIndex;  // next slot to receive a message (= NAND's _writeSlotIndex)
    SdSlotEntry slots[SD_SLOT_COUNT];
};

static_assert(sizeof(SdSlotEntry) == 24, "layout manifest doi -> the cu doc sai");
static_assert(sizeof(SdManifest) == 8 + 24 * SD_SLOT_COUNT, "layout manifest doi -> the cu doc sai");

/// IStorageProvider implementation for a MicroSD card (FAT32)
class SDStorageProvider : public IStorageProvider {
public:
    SDStorageProvider() = default;
    virtual ~SDStorageProvider() = default;

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
    bool openForAppend(const char* identifier = nullptr) override;
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

    SDCardManager* sdCard() override { return &_sd; }
    bool remount() override;

private:
    SDCardManager _sd;
    SdManifest _m{};
    bool _mounted = false;

    // --- WRITE path state ---
    int8_t _activeIndex = -1;       // slot open for write/append
    int8_t _lastWrittenIndex = -1;  // slot closeWrite() just committed (base for append)
    uint32_t _lastWrittenSize = 0;  // that slot's dataSize = offset of the AUDC header
    uint32_t _writeSize = 0;        // bytes written in the current session
    bool _writeOpen = false;
    bool _capturingHeader = false;
    uint8_t _hdrPeek[16] = {};      // the 16-byte container header, captured as it passes

    // --- Sequential READ path state ---
    int8_t _readIndex = -1;
    uint32_t _readCursor = 0;
    uint32_t _readCeil = 0;         // = dataSize; readAt() is NOT bound by it

    /// Accepts "7" and "slot_7". -1 = invalid / out of range.
    int8_t parseIndex(const char* identifier) const;

    /// Paths are built from the PARSED index, never from the raw identifier.
    void buildPath(int8_t idx, char* out, size_t maxLen) const;
    void buildTextPath(int8_t idx, char* out, size_t maxLen) const;

    bool isSlotValid(int8_t idx) const;
    int8_t writeIndexSafe() const;

    bool loadManifest();
    void saveManifest();
    void resetManifest();
};

#endif // SD_STORAGE_PROVIDER_H
