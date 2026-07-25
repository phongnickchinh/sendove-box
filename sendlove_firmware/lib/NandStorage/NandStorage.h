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
    char     magic[4];       // "VJPG" = video, "VIMG" = ảnh tĩnh, "\0" = trống
    uint32_t dataSize;       // Kích thước dữ liệu (bytes)
    uint16_t fps;            // FPS (dùng cho video)
    uint16_t totalFrames;    // Tổng số frame
    uint32_t reserved;       // Dự trữ
};

class NandStorage {
public:
    /// Khởi tạo NAND: cấu hình CS pin, đọc Slot Table
    /// @param spiMutex Mutex chia sẻ bus SPI2 với DisplayDriver
    /// @return true nếu đọc được Slot Table hợp lệ ("NSLT")
    bool init(SemaphoreHandle_t spiMutex = nullptr);
    void readRaw(uint32_t addr, uint8_t* data, uint32_t len);

    // --- Slot Query (read-only) ---

    /// Lấy thông tin slot
    SlotEntry getSlotInfo(uint8_t slot) const;

    /// Kiểm tra slot có dữ liệu hợp lệ (VJPG hoặc VIMG)
    bool isSlotValid(uint8_t slot) const;

    /// Kiểm tra slot là video (VJPG)
    bool isSlotVideo(uint8_t slot) const;

    /// Kiểm tra slot là ảnh tĩnh (VIMG)
    bool isSlotImage(uint8_t slot) const;

    /// Tìm slot hợp lệ đầu tiên. Trả về -1 nếu không có.
    int8_t findFirstValidSlot() const;

    /// Tìm slot hợp lệ tiếp theo (vòng tròn). Trả về -1 nếu không có.
    int8_t findNextValidSlot(int8_t currentSlot) const;

    // --- Sequential Read (cho playback) ---

    /// Mở slot để đọc tuần tự
    bool openSlot(uint8_t slot);

    /// Đọc dữ liệu tuần tự từ slot đang mở
    /// @return Số bytes thực tế đã đọc
    int readData(uint8_t* buf, uint32_t len);

    /// Seek tới offset tương đối trong slot data (0 = đầu data)
    void seekSlot(uint32_t offset);

    /// Đóng slot
    void closeSlot();

    /// Lấy slot index đang mở (-1 nếu chưa mở)
    int8_t getCurrentSlot() const;

private:
    SemaphoreHandle_t _spiMutex = nullptr;
    SlotEntry _slots[NAND_SLOT_COUNT];
    bool _tableValid = false;

    // Cursor cho slot đang đọc
    int8_t   _currentSlot = -1;
    uint32_t _cursor = 0;       // Offset tương đối trong slot data
    uint32_t _slotSize = 0;     // Kích thước slot data

    // --- Low-level SPI (có mutex) ---

    /// Acquire/Release SPI bus mutex
    bool acquireSPI();
    void releaseSPI();
};

#endif // NAND_STORAGE_H
