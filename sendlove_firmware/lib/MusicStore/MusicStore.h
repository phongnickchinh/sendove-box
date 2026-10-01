#ifndef MUSIC_STORE_H
#define MUSIC_STORE_H

#include <Arduino.h>

// ============================================================================
// MusicStore — alarm music already on the SD card (/alarm)
// ============================================================================
//   /alarm/index.json    {"<musicId>":{"rev":n,"size":b,"crc":c,"used":epoch}, ...}
//   /alarm/m_<id>.aud    AUDC(10) + WAV(44) + PCM 16kHz mono 16-bit (packaged by the web)
//
// A track no alarm uses anymore is KEPT on the card: choosing it again needs no
// download (an original requirement). Tracks are deleted only when removed from the
// cloud library (pruneExcept) or when the card is nearly full.
// The RAM copy is small (≤ ALARM_MUSIC_MAX_TRACKS entries), so the lookup when an
// alarm rings doesn't touch the card. Read from the WakeSync task (sync) and
// Task_MediaPlayer (ringing alarm) -> internal mutex.
// ============================================================================

namespace MusicStore {

/// Load the index from the card. Call after SdStore::begin() and after every remount.
void load();

/// Whether this track's file exists at this rev (rev = 0: any revision).
bool has(const char* id, uint32_t rev = 0);

/// "/alarm/m_<id>.aud"
void pathFor(const char* id, char* out, size_t maxLen);

/// Record a track that just finished downloading (size + crc already verified). Saves the index atomically.
bool put(const char* id, uint32_t rev, uint32_t size, uint32_t crc);

/// Mark as just used (eviction ranking when the card is full). RAM only; saved by the next put().
void touch(const char* id);

/// Delete every track NOT in the cloud list (tracks removed from the library), along
/// with their orphan .part files. keepIds = the ids still in the cloud.
void pruneExcept(const char (*keepIds)[24], size_t keepCount);

size_t count();

}  // namespace MusicStore

#endif  // MUSIC_STORE_H
