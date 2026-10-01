#ifndef NAND_STORAGE_H
#define NAND_STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

// ============================================================================
// NandStorage — driver for the W25Q128 flash chip on hardware SPI2
// ============================================================================
// Manages NAND_SLOT_COUNT video/image slots on the 16MB W25Q128, on hardware
// SPI2 (bus shared with the ST7789 DisplayDriver).
//
// The slot table sits in the first sector (0x000000):
//   magic "NSL3" + NAND_SLOT_COUNT × SlotEntry (278 bytes each)
//   The magic changes whenever SlotEntry grows (NSLT -> NSL2 for audioSize,
//   NSL2 -> NSL3 for textLen+text[256]): an old table must be rejected rather
//   than read with shifted fields. Such a firmware upgrade wipes the unread
//   messages on the device once — by design, not a bug.
// ============================================================================

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

    /// Erase a 4KB Flash sector at specified address. Returns false if the SPI
    /// mutex could NOT be taken (nothing erased) — the caller MUST check, because
    /// writing over an unerased area yields garbage with no error from the chip.
    bool eraseSector(uint32_t addr);

    /// Erase a continuous flash range using the largest supported erase granularity.
    /// false = at least one block/sector could not be erased.
    bool eraseRange(uint32_t addr, uint32_t len);

    /// Write raw data bytes to Flash address (handles page programming).
    /// false = the SPI mutex could not be taken; NO byte was written.
    bool writeRaw(uint32_t addr, const uint8_t* data, uint32_t len);

    /// Erase all slots & header table (Format Flash)
    void formatAll();

    /// Write / sync current slot table to Sector 0 with its magic header
    void writeSlotTable();

    /// Store the size of the audio appended after the video in the slot table (touches no other field)
    void setSlotAudioSize(uint8_t slot, uint32_t audioSize);

    /// Store caption text in the slot table. Sets only text/textLen — like
    /// setSlotAudioSize(), it must not memset() the struct and lose the
    /// magic/dataSize/audioSize written earlier. Truncates safely if
    /// len > SLOT_TEXT_MAX_LEN.
    void setSlotText(uint8_t slot, const char* text, uint16_t len);

    /// Read caption text from RAM (no SPI). Returns the length copied (0 if none).
    uint16_t getSlotText(uint8_t slot, char* outBuf, size_t maxLen) const;

    /// Read at an absolute offset in the open slot, NOT limited by dataSize and
    /// WITHOUT touching readData()'s sequential cursor. Used for the audio region.
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

    /// false = init() just created a fresh slot table (magic mismatch / blank chip).
    /// All old metadata is gone, so the layer above must reset its queue too.
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
