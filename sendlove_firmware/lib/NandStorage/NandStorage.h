#ifndef NAND_STORAGE_H
#define NAND_STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// NandStorage — W25Q128 driver: NAND_SLOT_COUNT media slots on the SPI bus shared
// with the ST7789.
// Slot table in sector 0: magic "NSL3" + NAND_SLOT_COUNT × SlotEntry (278 bytes).
// The magic changes whenever SlotEntry grows, so an old table is rejected rather
// than misread — such an upgrade wipes unread messages once, by design.

/// Caption text cap stored in the slot table (about 7-8 lines on the 240x240 screen).
static constexpr uint16_t SLOT_TEXT_MAX_LEN = 256;

/// One slot's metadata (278 bytes)
struct SlotEntry {
    char     magic[4];       // "VJPG" for video, "VIMG" for static image, "\0" for empty
    uint32_t dataSize;       // Video data size in bytes (EXCLUDING the audio appended after it)
    uint16_t fps;            // Frame rate (video)
    uint16_t totalFrames;    // Total frame count
    uint32_t maxDisplayTime; // Max display time in seconds
    uint32_t audioSize;      // Audio bytes appended after the video (incl. the 10-byte AUDC header); 0 = none
    uint16_t textLen;        // Actual caption length in text[] (0 = no text)
    char     text[SLOT_TEXT_MAX_LEN]; // Raw UTF-8 caption (diacritics intact) — folded to ASCII at render time
};

/// Hardware SPI driver for W25Q128 NAND Flash storage
class NandStorage {
public:
    /// Initialize NAND storage and parse slot table
    bool init(SemaphoreHandle_t spiMutex = nullptr);

    /// Read raw data bytes from specified Flash address
    void readRaw(uint32_t addr, uint8_t* data, uint32_t len);

    /// Erase a 4KB sector. false = nothing erased — the caller MUST check: writing
    /// over unerased flash gives garbage with no error.
    bool eraseSector(uint32_t addr);

    /// Erase a range. false = at least one block could not be erased.
    bool eraseRange(uint32_t addr, uint32_t len);

    /// Write raw bytes (page programming). false = NO byte was written.
    bool writeRaw(uint32_t addr, const uint8_t* data, uint32_t len);

    /// Erase all slots & header table (Format Flash)
    void formatAll();

    /// Write / sync current slot table to Sector 0 with its magic header
    void writeSlotTable();

    /// Store the size of the audio appended after the video in the slot table (touches no other field)
    void setSlotAudioSize(uint8_t slot, uint32_t audioSize);

    /// Store caption text in the slot table (only text/textLen are touched).
    /// Truncates to SLOT_TEXT_MAX_LEN.
    void setSlotText(uint8_t slot, const char* text, uint16_t len);

    /// Read caption text from RAM (no SPI). Returns the length copied (0 if none).
    uint16_t getSlotText(uint8_t slot, char* outBuf, size_t maxLen) const;

    /// Read at an absolute offset, NOT limited by dataSize and WITHOUT moving
    /// readData()'s cursor (audio region).
    int readAtSlot(uint32_t offset, uint8_t* buf, uint32_t len);

    /// Set slot metadata in RAM table (used after writing slot data)
    void setSlotInfo(uint8_t slot, const char* magic, uint32_t dataSize, uint16_t fps, uint16_t totalFrames, uint32_t maxDisplayTime);

    /// Get slot metadata entry
    SlotEntry getSlotInfo(uint8_t slot) const;

    /// Check if slot contains valid data (VJPG or VIMG)
    bool isSlotValid(uint8_t slot) const;

    /// Check if slot is video (VJPG)
    bool isSlotVideo(uint8_t slot) const;

    /// Check if slot is static image (VIMG)
    bool isSlotImage(uint8_t slot) const;

    /// Find first valid slot index (-1 if none)
    int8_t findFirstValidSlot() const;

    /// Find next valid slot index sequentially (-1 if none)
    int8_t findNextValidSlot(int8_t currentSlot) const;

    /// Open slot for sequential reading
    bool openSlot(uint8_t slot);

    /// Read sequential data bytes from opened slot
    int readData(uint8_t* buf, uint32_t len);

    /// Seek to offset within current opened slot
    void seekSlot(uint32_t offset);

    /// Close currently opened slot
    void closeSlot();

    /// Get currently opened slot index (-1 if none)
    int8_t getCurrentSlot() const;

    /// false = init() created a fresh slot table: the layer above must reset its queue.
    bool isTableValid() const { return _tableValid; }

private:
    SemaphoreHandle_t _spiMutex = nullptr;
    SlotEntry _slots[NAND_SLOT_COUNT];
    bool _tableValid = false;

    int8_t   _currentSlot = -1;
    uint32_t _cursor = 0;
    uint32_t _slotSize = 0;

    bool acquireSPI();
    void releaseSPI();
};

#endif // NAND_STORAGE_H
