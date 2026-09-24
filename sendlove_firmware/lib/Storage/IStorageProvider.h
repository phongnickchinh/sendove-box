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
    uint32_t dataSize = 0;      // Chỉ phần video/ảnh
    uint32_t audioSize = 0;     // Phần audio nối sau video (gồm header AUDC); 0 = không có
    uint16_t fps = 10;
    uint16_t totalFrames = 0;
    char id[32] = "";
    uint32_t maxDisplayTime = 60;
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

    /// Đọc tại offset tuyệt đối trong item đang mở, KHÔNG giới hạn bởi dataSize và
    /// KHÔNG đụng con trỏ đọc tuần tự. Cần cho vùng audio nối sau video —
    /// nếu dùng seek()+readData() thì AudioPlayer và MediaPlayer giẫm lên nhau.
    /// Mặc định trả 0 (provider chưa hỗ trợ).
    virtual int readAt(uint32_t offset, uint8_t* buffer, uint32_t len) {
        (void)offset; (void)buffer; (void)len; return 0;
    }

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
    virtual void closeWrite(uint32_t maxDisplayTime = 60) = 0;

    /// Huỷ bỏ phiên ghi dở dang (download lỗi / stall giữa chừng): KHÔNG commit
    /// slot table, KHÔNG đánh dấu unread — khác với closeWrite(). Mặc định no-op.
    virtual void discardWrite() { }

    /// Ghi caption text (đã cắt bớt theo độ dài buffer nội bộ) vào item đã ghi
    /// xong (sau closeWrite()/closeAppend()). Mặc định no-op (SD Card chưa hỗ trợ).
    virtual void setItemText(const char* identifier, const char* text) { (void)identifier; (void)text; }

    /// Đọc caption text của item. Trả về true nếu có text, false nếu không (mặc định).
    virtual bool getItemText(const char* identifier, char* outBuf, size_t maxLen) const {
        (void)identifier; (void)outBuf; (void)maxLen; return false;
    }

    /// Ghi tiếp dữ liệu vào slot vừa đóng mà không erase (dùng để append audio sau video)
    /// Mặc định: no-op (chỉ NAND storage hỗ trợ)
    virtual bool openForAppend(const char* identifier = nullptr) { (void)identifier; return false; }

    /// Chốt phần vừa append: ghi kích thước audio vào bảng slot. Không gọi thì
    /// dữ liệu audio nằm trên flash nhưng không ai biết nó dài bao nhiêu.
    virtual void closeAppend() {}

    // --- Quản lý Hàng chờ & Duyệt Item ---

    /// Kiểm tra bộ nhớ đã đầy tin chưa đọc hay chưa
    virtual bool isFull() const = 0;

    /// Lấy ID của Slot tiếp theo cho phép ghi (trả về false nếu bộ nhớ đầy)
    virtual bool getNextWriteSlotIdentifier(char* outId, size_t maxLen) = 0;

    /// Kiểm tra xem có tin nhắn / item nào chưa xem hay không
    virtual bool hasUnreadMessage() const = 0;

    /// Trả về số lượng tin chưa đọc
    virtual uint8_t getUnreadCount() const = 0;

    /// Lấy ID của item chưa đọc tiếp theo (ưu tiên tin cũ nhất)
    virtual bool getNextUnreadIdentifier(char* outId, size_t maxLen) = 0;

    /// Đánh dấu một item đã được xem
    virtual void markAsRead(const char* identifier) = 0;

    /// Tìm ID của item hợp lệ đầu tiên trong bộ nhớ (phục vụ fallback)
    virtual bool getFirstValidIdentifier(char* outId, size_t maxLen) const = 0;

    /// Tìm ID của item hợp lệ kế tiếp
    virtual bool getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const = 0;

    /// Xóa toàn bộ dữ liệu storage (Factory reset / Clear NAND)
    virtual bool formatStorage() { return false; }

    // --- File tổng quát trên thẻ (theme, nhạc báo thức, log) — 2026-09-24 ---

    /// Thẻ SD bên dưới, để SdStore đọc/ghi file tuỳ ý. nullptr = bộ nhớ không phải thẻ
    /// (bản NAND): mọi tính năng dựa trên thẻ tự tắt.
    virtual class SDCardManager* sdCard() { return nullptr; }

    /// Mount lại thẻ + nạp lại manifest (thẻ vừa cắm lại). Chỉ gọi khi không phát tin.
    virtual bool remount() { return false; }
};

#endif // I_STORAGE_PROVIDER_H
