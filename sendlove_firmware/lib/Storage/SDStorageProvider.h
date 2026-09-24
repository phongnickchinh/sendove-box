#ifndef SD_STORAGE_PROVIDER_H
#define SD_STORAGE_PROVIDER_H

#include "IStorageProvider.h"
#include "SDCardManager.h"
#include "config.h"

// ============================================================================
// Bố cục dữ liệu trên thẻ
// ============================================================================
//   /media/index.bin      manifest: hàng chờ + metadata từng slot
//   /media/slot_00.bin    media — layout GIỐNG HỆT một slot NAND:
//   ...                     [4B kích thước][header container 16B][payload][AUDC + audio]
//   /media/slot_19.bin
//   /media/slot_00.txt    caption (chỉ tạo khi tin có text)
//
// Caption để file sidecar chứ KHÔNG nhét vào manifest: 256B × 20 slot = 5KB RAM
// thường trú, quá đắt trên ESP32-C3 nơi TLS handshake đang giành từng KB.
//
// Cờ `unread` 1 byte mỗi slot thay cho bitmask uint8_t của NandStorageProvider —
// đây là lý do 20 slot chạy được mà không đụng gì tới IStorageProvider.
// ============================================================================

static constexpr uint32_t SD_MANIFEST_MAGIC = 0x324D4453; // "SDM2"
static constexpr uint16_t SD_MANIFEST_VERSION = 1;

/// Metadata một slot. 24 byte, mọi field tự căn lề đúng nên không có padding ẩn.
struct SdSlotEntry {
    char magic[4];           // "VJPG" | "VIMG" | {0,0,0,0} = rỗng
    uint32_t dataSize;       // GỒM cả 4 byte tiền tố — đúng như SlotEntry.dataSize của NAND
    uint32_t audioSize;      // GỒM 10 byte header AUDC; 0 = không có audio
    uint32_t maxDisplayTime; // giây
    uint16_t fps;
    uint16_t totalFrames;
    uint8_t unread;          // 0/1
    uint8_t _pad[3];
};

struct SdManifest {
    uint32_t magic;
    uint16_t version;
    uint8_t slotCount;      // Phải khớp SD_SLOT_COUNT, chặn thẻ của bản build cũ
    int8_t writeSlotIndex;  // Slot kế tiếp nhận tin mới (= _writeSlotIndex của NAND)
    SdSlotEntry slots[SD_SLOT_COUNT];
};

static_assert(sizeof(SdSlotEntry) == 24, "layout manifest doi -> the cu doc sai");
static_assert(sizeof(SdManifest) == 8 + 24 * SD_SLOT_COUNT, "layout manifest doi -> the cu doc sai");

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
    bool openForAppend(const char* identifier = nullptr) override;
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

    SDCardManager* sdCard() override { return &_sd; }
    bool remount() override;

private:
    SDCardManager _sd;
    SdManifest _m{};
    bool _mounted = false;

    // --- Trạng thái đường GHI ---
    int8_t _activeIndex = -1;       // Slot đang mở để ghi/append
    int8_t _lastWrittenIndex = -1;  // Slot mà closeWrite() vừa chốt (gốc cho append)
    uint32_t _lastWrittenSize = 0;  // dataSize của slot đó = offset header AUDC
    uint32_t _writeSize = 0;        // Số byte đã ghi trong phiên hiện tại
    bool _writeOpen = false;
    bool _capturingHeader = false;
    uint8_t _hdrPeek[16] = {};      // 16 byte header container chụp lúc đi qua

    // --- Trạng thái đường ĐỌC tuần tự ---
    int8_t _readIndex = -1;
    uint32_t _readCursor = 0;
    uint32_t _readCeil = 0;         // = dataSize; readAt() KHÔNG bị trần này

    /// Chấp nhận cả "7" lẫn "slot_7" (giống NandStorageProvider::parseSlotId).
    /// Trả -1 nếu không hợp lệ / ngoài dải.
    int8_t parseIndex(const char* identifier) const;

    /// Mọi đường dẫn dựng từ index ĐÃ PARSE, không bao giờ từ chuỗi thô —
    /// diệt tận gốc lỗi lệch identifier ("0" và "slot_0" từng ra 2 file khác nhau).
    void buildPath(int8_t idx, char* out, size_t maxLen) const;
    void buildTextPath(int8_t idx, char* out, size_t maxLen) const;

    bool isSlotValid(int8_t idx) const;
    int8_t writeIndexSafe() const;

    bool loadManifest();
    void saveManifest();
    void resetManifest();
};

#endif // SD_STORAGE_PROVIDER_H
