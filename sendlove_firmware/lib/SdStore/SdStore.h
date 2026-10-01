#ifndef SD_STORE_H
#define SD_STORE_H

#include <Arduino.h>
#include <atomic>

class IStorageProvider;
class SDCardManager;

// ============================================================================
// SdStore — general file layer on the SD card (theme, alarm music, log)
// ============================================================================
// Messages still go through SDStorageProvider (slots + manifest). All other heavy
// data lives under this tree (SD-card design, MEMORY.md §28):
//   /sys/layout.json     {"schema":1} — version of the card layout
//   /sys/log/log0.txt    the log (SdLog); log1.txt = the previous one
//   /theme/...           theme packages (ThemeStore)
//   /alarm/...           alarm music (MusicStore)
//
// Power-loss-safe write rules:
//   - Small files: writeAtomic() = write X.tmp -> delete X -> rename. At boot: an
//     X.tmp without X is renamed; with both present, X.tmp is deleted.
//   - Large downloads: write X.part, verify size + crc32, then rename. The .part is
//     KEPT across reboots to resume with HTTP Range (NetworkManager::downloadFile).
//
// A removed card has NO dedicated handling (edge-case policy, §28): on an I/O
// error probe(); if the card doesn't answer, go ABSENT and remount on the next sync.
// ============================================================================

namespace SdStore {

enum class State : uint8_t {
    NONE,    // this build doesn't use a card (NAND)
    ABSENT,  // a card path exists but it isn't mounted / was just lost
    READY,
};

/// Call once after storage->init(): creates the directory tree, cleans up .tmp
/// files, measures free space.
void begin(IStorageProvider* storage);

State state();
/// "ok" | "absent" | "none" — reported as status.sd_state
const char* stateName();

/// The SDCardManager when the card is usable, otherwise nullptr.
SDCardManager* card();

/// Bumped on every successful remount -> theme/music know to re-check and re-download.
extern std::atomic<uint32_t> mountEpoch;

/// Try to remount while ABSENT. ONLY call while not playing a message/music and
/// with no file open. true = just remounted.
bool tryRemount();

/// Report a failed card operation -> probe(); an unresponsive card becomes ABSENT.
void noteIoError();

/// Free space (MB), measured at mount and after refreshFree() (a real measurement
/// can take a few seconds).
uint32_t freeMB();
void refreshFree();

/// Atomically write a small file (see the top of this file).
bool writeAtomic(const char* path, const uint8_t* data, size_t len);
/// Read a small file as a '\0'-terminated string. Returns bytes read, -1 if the file is missing.
int32_t readText(const char* path, char* buf, size_t maxLen);

bool exists(const char* path);
bool remove(const char* path);
/// Remove a directory and the files in it (one level — enough for a theme package /theme/t_<id>_r<rev>).
bool removeTree(const char* dir);
int32_t fileSize(const char* path);

/// Standard CRC-32 (zlib/IEEE, poly 0xEDB88320) — the web uses the same formula.
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len);
/// crc32 of the first `size` bytes of a file. ok = false on a short read.
uint32_t crc32File(const char* path, uint32_t size, bool* ok);

}  // namespace SdStore

#endif  // SD_STORE_H
