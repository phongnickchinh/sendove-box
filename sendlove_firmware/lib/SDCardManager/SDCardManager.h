#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

// ============================================================================
// SDCardManager — Quản lý file system trên thẻ MicroSD + SPI Mutex
// ============================================================================
// Module chia sẻ — được gọi bởi:
// - NetworkHandler (ghi file tải từ Firebase)
// - MediaPlayer (đọc file để phát)
//
// Mọi thao tác SPI đều bọc trong xSemaphoreTake/Give(spiMutex)
// để tránh xung đột với DisplayDriver (cùng bus SPI).
// ============================================================================

class SDCardManager {
public:
    /// Khởi tạo SD card
    /// @param csPin Chân Chip Select cho SD module
    /// @param spiMutex Mutex chia sẻ bus SPI (tạo trong main.cpp)
    /// @return true nếu mount thành công
    bool init(uint8_t csPin, SemaphoreHandle_t spiMutex);

    /// Ghi dữ liệu vào file (mode: tạo mới hoặc ghi đè)
    /// @param path Đường dẫn file trên SD (VD: "/media/video.bin")
    /// @param data Con trỏ buffer dữ liệu
    /// @param len Kích thước dữ liệu (bytes)
    /// @return Số bytes đã ghi, hoặc -1 nếu lỗi
    int32_t writeFile(const char* path, const uint8_t* data, size_t len);

    /// Mở file để ghi stream (append mode)
    /// @param path Đường dẫn file
    /// @return true nếu mở thành công
    bool openFileForWrite(const char* path);

    /// Ghi thêm chunk dữ liệu vào file đang mở
    /// @return Số bytes đã ghi
    size_t appendChunk(const uint8_t* data, size_t len);

    /// Đóng file đang ghi
    void closeWriteFile();

    /// Mở file để đọc tuần tự
    /// @param path Đường dẫn file
    /// @return true nếu mở thành công
    bool openFileForRead(const char* path);

    /// Đọc một block dữ liệu từ file đang mở
    /// @param buffer Buffer nhận dữ liệu
    /// @param len Kích thước block muốn đọc
    /// @return Số bytes thực tế đã đọc (0 nếu hết file)
    size_t readBlock(uint8_t* buffer, size_t len);

    /// Đóng file đang đọc
    void closeReadFile();

    /// Kiểm tra file tồn tại
    bool fileExists(const char* path);

    /// Xóa file
    bool deleteFile(const char* path);

    /// Lấy kích thước file (bytes)
    /// @return Kích thước file, hoặc -1 nếu không tồn tại
    int32_t getFileSize(const char* path);

private:
    uint8_t _csPin = 0;
    SemaphoreHandle_t _spiMutex = nullptr;
    File _writeFile;
    File _readFile;

    /// Lấy quyền sử dụng SPI bus (blocking, timeout 1 giây)
    bool acquireSPI();

    /// Trả quyền sử dụng SPI bus
    void releaseSPI();
};

#endif // SD_CARD_MANAGER_H
