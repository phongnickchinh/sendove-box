#ifndef MUSIC_STORE_H
#define MUSIC_STORE_H

#include <Arduino.h>

// MusicStore — alarm music on the SD card:
//   /alarm/index.json    {"<musicId>":{"rev":n,"size":b,"crc":c,"used":epoch}, ...}
//   /alarm/m_<id>.aud    AUDC(10) + WAV(44) + PCM 16kHz mono 16-bit
// An unused track is KEPT (choosing it again needs no download); tracks go only
// when removed from the cloud library or when the card is nearly full. The index
// is mirrored in RAM (no card access when an alarm rings); internal mutex.

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

/// Delete every track (and orphan .part) NOT in keepIds, the ids still in the cloud.
void pruneExcept(const char (*keepIds)[24], size_t keepCount);

size_t count();

}  // namespace MusicStore

#endif  // MUSIC_STORE_H
