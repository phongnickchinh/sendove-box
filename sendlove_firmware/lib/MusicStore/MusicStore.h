#ifndef MUSIC_STORE_H
#define MUSIC_STORE_H

#include <Arduino.h>

// ============================================================================
// MusicStore — nhạc báo thức đã có trên thẻ SD (/alarm)
// ============================================================================
//   /alarm/index.json    {"<musicId>":{"rev":n,"size":b,"crc":c,"used":epoch}, ...}
//   /alarm/m_<id>.aud    AUDC(10) + WAV(44) + PCM 16kHz mono 16-bit (web đóng gói)
//
// Bài không còn báo thức nào dùng vẫn GIỮ trên thẻ: chọn lại thì khỏi tải (yêu cầu gốc).
// Chỉ xoá khi bài bị xoá khỏi thư viện trên cloud (pruneExcept) hoặc thẻ gần đầy.
// Bản RAM nhỏ (≤ ALARM_MUSIC_MAX_TRACKS mục) để lúc báo thức kêu tra cứu không đụng thẻ.
// Đọc từ task WakeSync (đồng bộ) và Task_MediaPlayer (báo thức kêu) -> mutex nội bộ.
// ============================================================================

namespace MusicStore {

/// Nạp index từ thẻ. Gọi sau SdStore::begin() và sau mỗi lần thẻ mount lại.
void load();

/// Có file bài này với đúng rev chưa (rev = 0: bản nào cũng được).
bool has(const char* id, uint32_t rev = 0);

/// "/alarm/m_<id>.aud"
void pathFor(const char* id, char* out, size_t maxLen);

/// Ghi nhận bài vừa tải xong (file đã đúng size + crc). Lưu index nguyên tử.
bool put(const char* id, uint32_t rev, uint32_t size, uint32_t crc);

/// Đánh dấu vừa dùng (xếp hạng xoá khi thẻ đầy). Chỉ đổi RAM; lưu ở lần put() sau.
void touch(const char* id);

/// Xoá mọi bài KHÔNG có trong danh sách cloud (bài đã bị xoá khỏi thư viện), cùng file
/// .part mồ côi của chúng. keepIds = danh sách id còn trên cloud.
void pruneExcept(const char (*keepIds)[24], size_t keepCount);

size_t count();

}  // namespace MusicStore

#endif  // MUSIC_STORE_H
