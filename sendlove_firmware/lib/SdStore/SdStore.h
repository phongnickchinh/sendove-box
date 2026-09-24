#ifndef SD_STORE_H
#define SD_STORE_H

#include <Arduino.h>
#include <atomic>

class IStorageProvider;
class SDCardManager;

// ============================================================================
// SdStore — lớp file tổng quát trên thẻ SD (theme, nhạc báo thức, log)
// ============================================================================
// Tin nhắn vẫn đi đường SDStorageProvider (slot + manifest) như cũ. Mọi dữ liệu nặng
// khác nằm dưới cây thư mục này (thiết kế 2026-09-24, MEMORY.md §28):
//   /sys/layout.json     {"schema":1} — phiên bản cấu trúc thẻ
//   /sys/log/log0.txt    nhật ký (SdLog), log1.txt = bản cũ
//   /theme/...           gói theme (ThemeStore)
//   /alarm/...           nhạc báo thức (MusicStore)
//
// Quy tắc ghi an toàn khi mất điện:
//   - File nhỏ: writeAtomic() = ghi X.tmp -> xoá X -> đổi tên. Boot: có X.tmp mà không
//     có X thì đổi tên; có cả hai thì xoá X.tmp.
//   - File lớn tải về: ghi X.part, kiểm size + crc32 rồi mới đổi tên. .part GIỮ LẠI
//     qua reboot để tải tiếp bằng HTTP Range (NetworkManager::downloadFile).
//
// Thẻ bị rút KHÔNG có nhánh xử lý riêng (nguyên tắc edge case, §28): lỗi I/O thì
// probe(), thẻ không trả lời thì về ABSENT, remount ở lần sync kế tiếp.
// ============================================================================

namespace SdStore {

enum class State : uint8_t {
    NONE,    // bản build không dùng thẻ (NAND)
    ABSENT,  // có đường thẻ nhưng chưa mount được / vừa mất
    READY,
};

/// Gọi một lần sau storage->init(): tạo cây thư mục, dọn .tmp, đo dung lượng trống.
void begin(IStorageProvider* storage);

State state();
/// "ok" | "absent" | "none" — gửi lên status.sd_state
const char* stateName();

/// Thẻ đang dùng được thì trả SDCardManager, không thì nullptr.
SDCardManager* card();

/// Tăng mỗi lần thẻ mount lại thành công -> theme/nhạc biết để kiểm và tải lại.
extern std::atomic<uint32_t> mountEpoch;

/// Thử mount lại khi đang ABSENT. CHỈ gọi lúc không phát tin/nhạc và không giữ file
/// nào mở. true = vừa mount lại được.
bool tryRemount();

/// Báo một thao tác thẻ vừa lỗi -> probe(); thẻ không trả lời thì chuyển ABSENT.
void noteIoError();

/// Dung lượng trống (MB), đo lúc mount và sau refreshFree() (đo thật có thể mất vài giây).
uint32_t freeMB();
void refreshFree();

/// Ghi nguyên tử một file nhỏ (xem đầu file).
bool writeAtomic(const char* path, const uint8_t* data, size_t len);
/// Đọc file nhỏ thành chuỗi kết thúc '\0'. Trả số byte đọc được, -1 nếu không có file.
int32_t readText(const char* path, char* buf, size_t maxLen);

bool exists(const char* path);
bool remove(const char* path);
/// Xoá thư mục cùng các file bên trong (một cấp, đủ cho gói theme /theme/t_<id>_r<rev>).
bool removeTree(const char* dir);
int32_t fileSize(const char* path);

/// CRC-32 chuẩn (zlib/IEEE, poly 0xEDB88320) — web tính cùng công thức.
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len);
/// crc32 của `size` byte đầu file. ok = false nếu đọc hụt.
uint32_t crc32File(const char* path, uint32_t size, bool* ok);

}  // namespace SdStore

#endif  // SD_STORE_H
