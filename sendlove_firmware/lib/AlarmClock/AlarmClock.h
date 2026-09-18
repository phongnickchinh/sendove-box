#ifndef ALARM_CLOCK_H
#define ALARM_CLOCK_H

#include <Arduino.h>
#include "ConfigManager.h"
#include "config.h"

// ============================================================================
// AlarmClock — danh sách báo thức trong hộp + quyết định lúc nào kêu
// ============================================================================
// Ba nơi cùng đụng vào danh sách, ở ba task khác nhau:
//   - NetworkManager (task WakeSync): tải từ cloud / đẩy lên cloud
//   - Captive portal (task NetworkController): thêm / sửa / xoá khi hộp ở AP mode
//   - Task_MediaPlayer + vòng ngủ (UIController): hỏi "tới giờ kêu chưa"
// nên mọi hàm public đều lấy mutex nội bộ. Bản RAM là nguồn đọc; NVS chỉ ghi khi đổi.
//
// Luật đồng bộ hai chiều (user chốt 2026-09-18, prototype):
//   - Sửa TRONG HỘP (portal, hoặc báo thức một lần tự tắt sau khi kêu) -> bật cờ
//     dirty (NVS). Lần sync kế tiếp ĐẨY CẢ DANH SÁCH lên cloud, ghi đè bản cloud.
//   - Còn dirty thì KHÔNG nhận danh sách từ cloud (hộp thắng). Hết dirty thì cloud
//     là nguồn chuẩn, a_flag bật -> tải về thay toàn bộ.
//   Không merge từng báo thức: không có đồng hồ tin cậy ở AP mode để so updated_at.
// ============================================================================

class AlarmClock {
public:
    static AlarmClock& instance();

    /// Tạo mutex + nạp danh sách từ NVS. Gọi một lần trong setup(), trước khi tạo task.
    void begin();

    /// Chép danh sách hiện tại ra ngoài. Trả số phần tử.
    size_t list(AlarmItem* out, size_t maxCount);

    /// Cloud -> hộp. Trả false và KHÔNG ghi gì nếu hộp đang có sửa đổi chưa đẩy.
    bool replaceFromCloud(const AlarmItem* items, size_t count);

    /// Portal: id rỗng = thêm mới (id sinh trong outId). Trả false nếu giờ sai định
    /// dạng, id không tồn tại, hoặc đã đủ MAX_ALARMS.
    bool upsert(const char* id, const char* time, bool enable, bool repeatable,
                char* outId = nullptr, size_t outLen = 0);
    bool remove(const char* id);

    /// Có sửa đổi chưa đẩy lên cloud không. `rev` dùng cho markPushed().
    bool isDirty(uint32_t* rev);
    /// Gọi sau khi PUT lên cloud thành công với snapshot lấy ở `rev`. Nếu trong lúc
    /// đẩy lại có sửa đổi mới thì giữ nguyên dirty để lần sau đẩy tiếp.
    void markPushed(uint32_t rev);

    /// Gọi định kỳ (~500ms). true = bắt đầu kêu ngay, outTime nhận "HH:MM".
    /// Báo thức một lần bị tắt (và đánh dấu dirty) NGAY lúc bắt đầu kêu.
    bool pollDue(time_t now, char* outTime, size_t len);

    /// Người dùng chạm ngắn khi đang kêu: kêu lại sau ALARM_SNOOZE_SEC.
    void snooze(time_t now);
    /// Chạm giữ / hết ALARM_RING_MAX_MS: tắt hẳn, huỷ snooze đang chờ.
    void dismiss();

    /// Số giây tới lần kêu kế tiếp (tính cả snooze). 0 = đang tới hạn mà chưa kêu.
    /// 0xFFFFFFFF = không có gì để kêu, hoặc đồng hồ chưa hợp lệ.
    uint32_t secondsToNext(time_t now);

    /// "HH:MM" đúng 24h. Dùng chung cho portal và cloud.
    static bool isValidTime(const char* t);

private:
    AlarmClock() = default;

    SemaphoreHandle_t _mutex = nullptr;
    AlarmItem _items[MAX_ALARMS];
    size_t    _count = 0;

    uint32_t _rev = 0;        // tăng mỗi lần sửa trong hộp
    uint32_t _pushedRev = 0;  // rev đã đẩy xong
    bool     _dirty = false;

    time_t   _lastFiredMinute = 0;  // chống kêu lại trong cùng một phút
    time_t   _snoozeUntil = 0;
    char     _snoozeTime[6] = "";

    void lock();
    void unlock();
    void saveLocked();          // ghi danh sách + cờ dirty xuống NVS
    void markDirtyLocked();
    int  findLocked(const char* id);
};

#endif // ALARM_CLOCK_H
