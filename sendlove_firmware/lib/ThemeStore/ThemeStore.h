#ifndef THEME_STORE_H
#define THEME_STORE_H

#include <Arduino.h>

// ============================================================================
// ThemeStore — the active theme, projected from the SD card into the `theme`
// flash partition
// ============================================================================
// Product decision "option B" (MEMORY.md §28): the SPI bus is shared with an
// ST7789 that has no CS pin, so the background/fonts can NOT be read from the
// card while drawing. A theme package is downloaded to the card
// (/theme/t_<id>_r<rev>/), then copied to a 256KB flash partition and read through
// esp_partition_mmap, just like a PROGMEM array: 0 bytes of RAM, and pushImage()
// works straight from a flash pointer.
//
// Partition layout: sector 0 = header (magic, rev, id, asset table, payload crc,
// header crc), payload from 4096. The header is written LAST: a power loss during
// the copy leaves a bad header crc -> begin() treats it as empty -> copied again
// from the card (mandatory case, MEMORY.md §28).
//
// With a faulty card the theme in flash STILL shows (product decision). The black
// screen with white text appears only when the partition is empty/corrupt (e.g. a
// new partition table was just cable-flashed).
//
// Assets: "layout" (theme JSON), "bg" (240x240 RGB565 LE, 115,200 B), "f_time",
// "f_date" (VLW fonts subset by the web). Only "layout" is required.
// ============================================================================

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

/// Copy the package in card directory `dir` to flash, then remap. Flash erase/write
/// stalls the CPU ~2s. ONLY call from the task that owns drawing (Task_MediaPlayer)
/// while it isn't drawing the theme, or in setup() — old asset pointers die the
/// moment it starts.
bool installFromSd(const char* dir, const char* themeId, uint32_t rev);

/// WakeSync finished downloading a package -> post a request; Task_MediaPlayer
/// installs it when it sees one (single owner).
void requestInstall(const char* dir, const char* themeId, uint32_t rev);
bool takeInstallRequest(char* dir, size_t dirLen, char* id, size_t idLen, uint32_t* rev);
bool installPending();

/// At boot with an empty/corrupt partition: reinstall from the package named in
/// /theme/active.json if the card has it.
bool restoreFromSdIfNeeded();

}  // namespace ThemeStore

#endif  // THEME_STORE_H
