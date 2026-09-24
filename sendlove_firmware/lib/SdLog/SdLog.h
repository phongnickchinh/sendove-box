#ifndef SD_LOG_H
#define SD_LOG_H

#include <Arduino.h>

// ============================================================================
// SdLog — nhật ký bền trên thẻ + đẩy đoạn cuối lên cloud khi có lỗi (đề xuất #2)
// ============================================================================
// ScreenLogger::log() gọi add() cho MỌI dòng DLOG. add() chỉ chép vào vòng RAM (không
// đụng thẻ): DLOG được gọi từ mọi task, có chỗ đang giữ spiMutex, ghi thẻ ở đó là
// deadlock (quy tắc R1 của SDCardManager).
//
// flush() ghi các dòng chưa ghi xuống /sys/log/log0.txt, quá 64KB thì đổi sang log1.txt.
// Gọi từ nơi KHÔNG giữ spiMutex: vòng STANDBY của Task_MediaPlayer và trước khi ngủ.
//
// takeTail(): đoạn cuối log để đẩy lên status/log_tail. Chỉ trả khi có dòng lỗi mới từ
// lần lấy trước, hoặc lần đầu sau boot (để thấy `[BOOT] reset=` của lần reboot vừa rồi).
// ============================================================================

namespace SdLog {

void add(const char* line);
void flush();
/// true + chép đoạn cuối (các dòng cách nhau '\n') vào out nếu có gì mới đáng đẩy.
bool takeTail(char* out, size_t maxLen);

}  // namespace SdLog

#endif  // SD_LOG_H
