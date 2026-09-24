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
//
// RANH GIỚI MODULE: file này chỉ biết tới đường dẫn, file handle và bus SPI.
// Toàn bộ ngữ nghĩa slot / manifest / hàng chờ nằm ở SDStorageProvider —
// đúng cách chia NandStorage / NandStorageProvider.
//
// BỐN FILE HANDLE thường trú + handle tạm của readFile/readFileAt (SD.begin dành sẵn
// max_files = 7 từ 2026-09-24; trước đó 5 cho 3 handle):
//   _genFile   : ghi file tổng quát (nhạc, theme) của WakeSync, không đụng đường tải tin.
//                CHỈ Task_WakeSync dùng: openGenWrite() đóng handle đang mở, nên task khác
//                dùng chung sẽ cướp file đang tải (log dùng appendFile(), 2026-09-24).
//   _writeFile : đường ghi (download)
//   _readFile  : đọc tuần tự cho MediaPlayer (con trỏ do provider quản)
//   _atFile    : đọc ngẫu nhiên cho AudioPlayer — PHẢI là handle RIÊNG, vì
//                readAt() không được phép đụng con trỏ tuần tự (xem
//                IStorageProvider.h:44-47). Seek-rồi-seek-lại trên một handle
//                chung chính là bug mà comment đó tồn tại để chặn.
// ============================================================================

class SDCardManager {
public:
    /// Khởi tạo SD card
    /// @param csPin Chân Chip Select cho SD module
    /// @param spiMutex Mutex chia sẻ bus SPI (tạo trong main.cpp)
    /// @return true nếu mount thành công
    bool init(uint8_t csPin, SemaphoreHandle_t spiMutex);

    /// Thẻ đã mount được hay chưa. Không mount được KHÔNG phải lỗi chí mạng:
    /// thẻ rút ra được, provider phải chạy tiếp ở chế độ rỗng.
    bool isMounted() const { return _mounted; }

    // --- Write Operations ---

    /// Ghi dữ liệu vào file (tạo mới hoặc ghi đè)
    /// @return Số bytes đã ghi, hoặc -1 nếu lỗi
    int32_t writeFile(const char* path, const uint8_t* data, size_t len);

    /// Ghi tiếp vào cuối file (tạo nếu chưa có), mở-ghi-đóng trong một lần giữ mutex
    /// @return Số bytes đã ghi, hoặc -1 nếu lỗi
    int32_t appendFile(const char* path, const uint8_t* data, size_t len);

    /// Mở file để ghi stream (cắt sạch nội dung cũ)
    bool openFileForWrite(const char* path);

    /// Mở lại file đã có để ghi tiếp tại offset chỉ định (mode "r+", không cắt file).
    /// Dùng cho đường append audio nối sau video.
    bool openFileForAppend(const char* path, uint32_t atOffset);

    /// Ghi thêm chunk dữ liệu vào file đang mở
    /// @return Số bytes đã ghi. Trả về ÍT HƠN len khi lỗi — đây là tín hiệu
    ///         mà NetworkManager dùng để phát hiện writeError.
    size_t appendChunk(const uint8_t* data, size_t len);

    /// Vá 4 byte tại offset 0 của file đang mở (tiền tố kích thước payload).
    /// Mode "w" là O_TRUNC — cắt file xảy ra lúc OPEN chứ không phải lúc write,
    /// nên ghi đè 4 byte tại đầu file không thể làm ngắn file.
    bool patchWriteFileAt0(const uint8_t* buf4);

    /// Đóng file đang ghi
    void closeWriteFile();

    // --- Sequential Read (MediaPlayer) ---

    /// Mở file để đọc tuần tự
    bool openFileForRead(const char* path);

    /// Dời con trỏ đọc tuần tự
    bool seekReadFile(uint32_t offset);

    /// Đọc một block từ con trỏ tuần tự
    /// @return Số bytes thực tế đã đọc (0 nếu hết file / lỗi)
    size_t readBlock(uint8_t* buffer, size_t len);

    /// Đóng file đang đọc tuần tự
    void closeReadFile();

    // --- Random Read (AudioPlayer) — handle riêng, KHÔNG đụng con trỏ tuần tự ---

    /// Mở handle đọc ngẫu nhiên trên cùng đường dẫn (cache sẵn kích thước file)
    bool openAtFile(const char* path);

    /// Đọc tại offset tuyệt đối. Chặn theo kích thước file vật lý.
    /// Bỏ qua seek khi offset trùng vị trí hiện tại — AudioPlayer đọc đơn điệu
    /// tăng dần từng AUDIO_READ_CHUNK_SIZE byte, seek mỗi lần sẽ phá readahead.
    int readAtFile(uint32_t offset, uint8_t* buffer, uint32_t len);

    /// Đóng handle đọc ngẫu nhiên
    void closeAtFile();

    /// Kích thước file đang mở ở handle đọc ngẫu nhiên (0 nếu chưa mở)
    uint32_t atFileSize() const { return _atSize; }

    // --- Utility ---

    /// Kiểm tra file tồn tại
    bool fileExists(const char* path) const;

    /// Xóa file. Trả false nếu file không tồn tại hoặc xoá thất bại.
    bool deleteFile(const char* path);

    /// Lấy kích thước file (bytes), hoặc -1 nếu không tồn tại
    int32_t getFileSize(const char* path) const;

    /// Đọc trọn một file nhỏ (manifest / caption) vào buffer
    /// @return Số bytes đọc được, hoặc -1 nếu không mở được
    int32_t readFile(const char* path, uint8_t* buf, size_t maxLen) const;

    // --- File tổng quát (theme, nhạc báo thức, log) — thêm 2026-09-24 ---

    /// Tháo rồi mount lại thẻ (thẻ vừa cắm lại). Gọi khi KHÔNG có handle nào đang mở.
    bool remount();

    /// Thẻ còn trả lời không (SD.cardType() != CARD_NONE). Sai -> đánh dấu chưa mount.
    bool probe();

    /// Đọc `len` byte tại `offset` của một file (mở-đọc-đóng). -1 nếu không mở được.
    int32_t readFileAt(const char* path, uint32_t offset, uint8_t* buf, size_t len) const;

    /// Đổi tên. FAT không ghi đè: `to` đã có thì trả false (bên gọi xoá trước).
    bool renameFile(const char* from, const char* to);

    /// Tạo thư mục (và thư mục cha một cấp). true nếu đã có sẵn hoặc tạo được.
    bool makeDir(const char* path);

    /// Xoá thư mục RỖNG (SD.rmdir). Xoá cả cây: SdStore::removeTree().
    bool removeDir(const char* path);

    /// Duyệt tên các mục trong thư mục (không đệ quy). cb nhận tên KHÔNG kèm đường dẫn
    /// và cờ isDir. Tên được chép ra rồi mới gọi cb sau khi nhả mutex, nên cb được phép
    /// gọi lại các hàm của lớp này (xoá, đổi tên).
    size_t listDir(const char* dir, void (*cb)(const char* name, bool isDir, void* ctx), void* ctx);

    /// Dung lượng còn trống (MB). 0 nếu chưa mount.
    uint32_t freeMB() const;

    /// Handle ghi THỨ HAI cho file tổng quát (tải nhạc/theme) — tách khỏi _writeFile
    /// của đường tải tin nhắn. append = mở "a" (ghi tiếp cuối file, cho tải tiếp bằng Range).
    bool openGenWrite(const char* path, bool append);
    size_t genWrite(const uint8_t* data, size_t len);
    void closeGenWrite();

private:
    uint8_t _csPin = 0;
    mutable SemaphoreHandle_t _spiMutex = nullptr;
    bool _mounted = false;

    File _writeFile;
    File _readFile;
    File _atFile;
    File _genFile;  // ghi file tổng quát (xem openGenWrite)

    uint32_t _atSize = 0;
    uint32_t _atPos = 0;
    bool _atPosKnown = false;

    /// Lấy quyền sử dụng SPI bus (blocking, timeout 1 giây) + NOP Hack cho ST7789
    bool acquireSPI() const;

    /// Trả quyền sử dụng SPI bus
    void releaseSPI() const;

    /// Tạo thư mục cha của path nếu chưa có. GIẢ ĐỊNH ĐANG GIỮ MUTEX
    /// (mutex không đệ quy — không được acquire lồng nhau).
    void mkParentDirLocked(const char* path);
};

#endif // SD_CARD_MANAGER_H
