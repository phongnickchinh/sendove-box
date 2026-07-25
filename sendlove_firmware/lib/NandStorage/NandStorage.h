#ifndef NAND_STORAGE_H
#define NAND_STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// ============================================================================
// NandStorage — Driver cho W25Q128 NAND Flash trên Hardware SPI2
// ============================================================================
// Quản lý 5 slot video/ảnh lưu trên flash NAND W25Q128 (16MB).
// Sử dụng Hardware SPI2 (chia sẻ bus với DisplayDriver ST7789).
//
// Slot Table nằm ở sector đầu tiên (0x000000):
//   Magic "NSLT" + 5 × SlotEntry (16 bytes mỗi entry)
//
// Phase 1: Chế độ READ-ONLY — không erase/write để bảo toàn dữ liệu.
// ============================================================================

/// Thông tin 1 slot (16 bytes, giữ nguyên binary format từ test project)
struct SlotEntry {
    char     magic[4];       // "VJPG" for video, "VIMG" for static image, "\0" for empty
    uint32_t dataSize;       // Data size in bytes
    uint16_t fps;            // Frame rate (video)
    uint16_t totalFrames;    // Total frame count
    uint32_t reserved;
};

/// Hardware SPI driver for W25Q128 NAND Flash storage
class NandStorage {
public:
    /// Initialize NAND storage and parse slot table
    bool init(SemaphoreHandle_t spiMutex = nullptr);

    /// Read raw data bytes from specified Flash address
    void readRaw(uint32_t addr, uint8_t* data, uint32_t len);

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
