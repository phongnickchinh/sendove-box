#ifndef NAND_STORAGE_H
#define NAND_STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// ============================================================================
// NandStorage — Driver cho W25Q128 NAND Flash trên Hardware SPI2
// ============================================================================
// Quản lý NAND_SLOT_COUNT slot video/ảnh lưu trên flash NAND W25Q128 (16MB).
// Sử dụng Hardware SPI2 (chia sẻ bus với DisplayDriver ST7789).
//
// Slot Table nằm ở sector đầu tiên (0x000000):
//   Magic "NSL3" + NAND_SLOT_COUNT × SlotEntry (278 bytes mỗi entry)
//   Magic đổi NSL2 -> NSL3 vì SlotEntry dài thêm textLen+text[256] (caption tin
//   nhắn tĩnh): bảng cũ phải bị từ chối chứ không được đọc lệch trường. Giống
//   tiền lệ NSLT -> NSL2 khi thêm audioSize — nâng cấp firmware này xoá sạch
//   tin nhắn unread cũ trên máy 1 lần duy nhất, không phải bug.
//
// Phase 1: Chế độ READ-ONLY — không erase/write để bảo toàn dữ liệu.
// ============================================================================

/// Giới hạn caption text lưu trong bảng slot (đủ ~7-8 dòng trên màn 240x240).
static constexpr uint16_t SLOT_TEXT_MAX_LEN = 256;

/// Thông tin 1 slot (278 bytes)
struct SlotEntry {
    char     magic[4];       // "VJPG" for video, "VIMG" for static image, "\0" for empty
    uint32_t dataSize;       // Video data size in bytes (KHÔNG gồm audio nối phía sau)
    uint16_t fps;            // Frame rate (video)
    uint16_t totalFrames;    // Total frame count
    uint32_t maxDisplayTime; // Max display time in seconds
    uint32_t audioSize;      // Byte audio nối sau video (gồm header AUDC 10 byte); 0 = không có
    uint16_t textLen;        // Độ dài caption thực tế trong text[] (0 = không có text)
    char     text[SLOT_TEXT_MAX_LEN]; // Caption UTF-8 thô (chưa bỏ dấu) — bỏ dấu lúc render
};

/// Hardware SPI driver for W25Q128 NAND Flash storage
class NandStorage {
public:
    /// Initialize NAND storage and parse slot table
    bool init(SemaphoreHandle_t spiMutex = nullptr);

    /// Read raw data bytes from specified Flash address
    void readRaw(uint32_t addr, uint8_t* data, uint32_t len);

    /// Erase a 4KB Flash sector at specified address. Trả về false nếu KHÔNG lấy
    /// được SPI mutex (không erase được) — caller PHẢI kiểm, vì ghi đè lên vùng
    /// chưa erase cho ra dữ liệu rác mà chip không hề báo lỗi.
    bool eraseSector(uint32_t addr);

    /// Erase a continuous flash range using the largest supported erase granularity.
    /// false = có ít nhất một block/sector không erase được.
    bool eraseRange(uint32_t addr, uint32_t len);

    /// Write raw data bytes to Flash address (handles page programming).
    /// false = không lấy được SPI mutex, KHÔNG có byte nào được ghi.
    bool writeRaw(uint32_t addr, const uint8_t* data, uint32_t len);

    /// Erase all slots & header table (Format Flash)
    void formatAll();

    /// Write / sync current slot table to Sector 0 with "NSLT" magic header
    void writeSlotTable();

    /// Ghi kích thước phần audio nối sau video vào bảng slot (không đụng trường khác)
    void setSlotAudioSize(uint8_t slot, uint32_t audioSize);

    /// Ghi caption text vào bảng slot (không memset() cả struct, chỉ set field
    /// text/textLen — theo đúng pattern setSlotAudioSize(), không được xoá mất
    /// magic/dataSize/audioSize đã ghi trước đó). Cắt bớt an toàn nếu len > SLOT_TEXT_MAX_LEN.
    void setSlotText(uint8_t slot, const char* text, uint16_t len);

    /// Đọc caption text từ RAM (không cần SPI). Trả về độ dài đã copy (0 nếu không có).
    uint16_t getSlotText(uint8_t slot, char* outBuf, size_t maxLen) const;

    /// Đọc tại offset tuyệt đối trong slot đang mở, KHÔNG bị chặn bởi dataSize và
    /// KHÔNG đụng con trỏ đọc tuần tự của readData(). Dùng cho vùng audio.
    int readAtSlot(uint32_t offset, uint8_t* buf, uint32_t len);

    /// Set slot metadata in RAM table (used after writing slot data)
    void setSlotInfo(uint8_t slot, const char* magic, uint32_t dataSize, uint16_t fps, uint16_t totalFrames, uint32_t maxDisplayTime);

    /// Get slot metadata entry
    SlotEntry getSlotInfo(uint8_t slot) const;

    /// Check if slot contains valid data (VJPG or VIMG)
    bool isSlotValid(uint8_t slot) const;

    /// Check if slot is video (VJPG)
    bool isSlotVideo(uint8_t slot) const;

    /// Check if slot is static image (VIMG)
    bool isSlotImage(uint8_t slot) const;

    /// Find first valid slot index (-1 if none)
    int8_t findFirstValidSlot() const;

    /// Find next valid slot index sequentially (-1 if none)
    int8_t findNextValidSlot(int8_t currentSlot) const;

    /// Open slot for sequential reading
    bool openSlot(uint8_t slot);

    /// Read sequential data bytes from opened slot
    int readData(uint8_t* buf, uint32_t len);

    /// Seek to offset within current opened slot
    void seekSlot(uint32_t offset);

    /// Close currently opened slot
    void closeSlot();

    /// Get currently opened slot index (-1 if none)
    int8_t getCurrentSlot() const;

    /// false = init() vua tao bang slot moi (magic khong khop / chip trong).
    /// Moi metadata cu da bi xoa, phia tren phai reset hang cho theo.
    bool isTableValid() const { return _tableValid; }

private:
    SemaphoreHandle_t _spiMutex = nullptr;
    SlotEntry _slots[NAND_SLOT_COUNT];
    bool _tableValid = false;

    int8_t   _currentSlot = -1;
    uint32_t _cursor = 0;
    uint32_t _slotSize = 0;

    bool acquireSPI();
    void releaseSPI();
};

#endif // NAND_STORAGE_H
