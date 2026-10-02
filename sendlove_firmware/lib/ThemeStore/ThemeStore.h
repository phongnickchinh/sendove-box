#ifndef THEME_STORE_H
#define THEME_STORE_H

#include <Arduino.h>

// ThemeStore — the active theme, copied from the SD card into the `theme` flash
// partition (product decision "option B", MEMORY.md §28). The card can NOT be read
// while drawing (shared bus, CS-less ST7789), so the package is copied to flash
// and read through esp_partition_mmap: 0 bytes of RAM.
//
// Layout: sector 0 = header, payload from 4096. The header is written LAST, so a
// power loss mid-copy leaves a bad crc and the copy is redone from the card
// (mandatory case, MEMORY.md §28). With a faulty card the theme in flash STILL shows.
//
// Assets: "layout" (JSON, required), "bg" (240x240 RGB565 LE), "f_time", "f_date" (VLW).

namespace ThemeStore {

/// Find the partition, mmap it, check header + crc. Call in setup(). false = no theme yet.
bool begin();

bool valid();
/// Whether the partition table has a `theme` partition (images flashed before the SD-card design don't).
bool partitionPresent();
uint32_t rev();
const char* themeId();

/// Pointer to an asset in flash (through the mmap), nullptr if absent. Valid until the next install.
const uint8_t* asset(const char* name, uint32_t* len);

/// Copy the package in `dir` to flash and remap (stalls the CPU ~2s). ONLY from
/// the drawing task or setup(): old asset pointers die the moment it starts.
bool installFromSd(const char* dir, const char* themeId, uint32_t rev);

/// Posted by WakeSync after a download; Task_MediaPlayer performs the install.
void requestInstall(const char* dir, const char* themeId, uint32_t rev);
bool takeInstallRequest(char* dir, size_t dirLen, char* id, size_t idLen, uint32_t* rev);
bool installPending();

/// At boot with an empty partition: reinstall the package named in /theme/active.json.
bool restoreFromSdIfNeeded();

}  // namespace ThemeStore

#endif  // THEME_STORE_H
