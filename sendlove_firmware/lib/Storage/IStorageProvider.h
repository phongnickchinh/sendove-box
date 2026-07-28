#ifndef I_STORAGE_PROVIDER_H
#define I_STORAGE_PROVIDER_H

#include <Arduino.h>

/// Trạng thái của media slot / file
enum class StorageItemType : uint8_t {
    UNKNOWN,
    VIDEO,
    IMAGE,
    EMPTY
};

/// Thông tin của một media item
struct StorageItemInfo {
    StorageItemType type = StorageItemType::UNKNOWN;
    uint32_t dataSize = 0;
    uint16_t fps = 10;
    uint16_t totalFrames = 0;
    char id[32] = "";
};

/// Interface trừu tượng cho mọi lớp bộ nhớ lưu trữ (NAND Flash / SD Card)
class IStorageProvider {
public:
    virtual ~IStorageProvider() = default;

    /// Khởi tạo phần cứng bộ nhớ với mutex chia sẻ SPI
    virtual bool init(SemaphoreHandle_t spiMutex = nullptr) = 0;

    // --- Thao tác ĐỌC (Media Player) ---
    
    /// Mở một item theo ID (hoặc slot index dạng chuỗi "0", "1"...) để đọc
    virtual bool openForRead(const char* identifier) = 0;

    /// Đọc một lượng byte dữ liệu từ item đang mở
    virtual int readData(uint8_t* buffer, uint32_t len) = 0;

    /// Di chuyển con trỏ đọc đến offset cụ thể
    virtual void seek(uint32_t offset) = 0;

    /// Đóng item đang đọc
    virtual void closeRead() = 0;

    /// Lấy thông tin metadata của item đang mở hoặc theo ID
    virtual StorageItemInfo getItemInfo(const char* identifier = nullptr) const = 0;

    // --- Thao tác GHI (File Downloader) ---

    /// Mở một item theo ID để ghi mới / ghi đè
    virtual bool openForWrite(const char* identifier) = 0;

    /// Ghi thêm một chunk dữ liệu vào item đang mở
    virtual size_t writeChunk(const uint8_t* data, size_t len) = 0;

    /// Đóng item đang ghi
    virtual void closeWrite() = 0;

    // --- Quản lý Hàng chờ & Duyệt Item ---

    /// Kiểm tra xem có tin nhắn / item nào chưa xem hay không
    virtual bool hasUnreadMessage() const = 0;

    /// Lấy ID của item chưa đọc tiếp theo (trả về false nếu không có)
    virtual bool getNextUnreadIdentifier(char* outId, size_t maxLen) = 0;

    /// Đánh dấu một item đã được xem
    virtual void markAsRead(const char* identifier) = 0;

    /// Tìm ID của item hợp lệ đầu tiên trong bộ nhớ (phục vụ fallback)
    virtual bool getFirstValidIdentifier(char* outId, size_t maxLen) const = 0;

    /// Tìm ID của item hợp lệ kế tiếp
    virtual bool getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const = 0;
};

#endif // I_STORAGE_PROVIDER_H
