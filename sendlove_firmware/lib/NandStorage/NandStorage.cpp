#include "NandStorage.h"
#include "ScreenLogger.h"

// W25Q128 SPI commands
static constexpr uint8_t W25Q_READ_DATA      = 0x03;
static constexpr uint8_t W25Q_READ_STATUS_1  = 0x05;

// SPI settings for the NAND: three constants for three paths — don't merge them.
// All run at 4MHz: at 20MHz the breadboard WIRING corrupts data intermittently
// (stutter, Bad jpegSize), see MEMORY.md §14. Don't raise them on the breadboard;
// on a PCB, re-test with a NEWLY downloaded message played many times.

// ERASE: one corrupted address byte erases the wrong sector.
static const SPISettings NAND_SPI_SETTINGS(4000000, MSBFIRST, SPI_MODE3);
static const SPISettings NAND_READ_SPI_SETTINGS(4000000, MSBFIRST, SPI_MODE3);
// WRITE: bulk writes still give ~500KB/s, faster than the Wi-Fi download.
static const SPISettings NAND_WRITE_SPI_SETTINGS(4000000, MSBFIRST, SPI_MODE3);

bool NandStorage::init(SemaphoreHandle_t spiMutex) {
    _spiMutex = spiMutex;

    pinMode(PIN_NAND_CS, OUTPUT);
    digitalWrite(PIN_NAND_CS, HIGH);

    uint8_t header[4 + NAND_SLOT_COUNT * sizeof(SlotEntry)];
    readRaw(0, header, sizeof(header));

    if (memcmp(header, "NSL3", 4) != 0) {
        DLOG("[NAND] no table -> init clean");
        memset(_slots, 0, sizeof(_slots));
        writeSlotTable();
        return true;
    }

    for (uint8_t i = 0; i < NAND_SLOT_COUNT; i++) {
        memcpy(&_slots[i], header + 4 + i * sizeof(SlotEntry), sizeof(SlotEntry));
    }

    _tableValid = true;
    return true;
}

void NandStorage::setSlotInfo(uint8_t slot, const char* magic, uint32_t dataSize, uint16_t fps, uint16_t totalFrames, uint32_t maxDisplayTime) {
    if (slot >= NAND_SLOT_COUNT) return;
    memset(&_slots[slot], 0, sizeof(SlotEntry));
    if (magic) memcpy(_slots[slot].magic, magic, 4);
    _slots[slot].dataSize = dataSize;
    _slots[slot].fps = fps;
    _slots[slot].totalFrames = totalFrames;
    _slots[slot].maxDisplayTime = maxDisplayTime;
}

void NandStorage::setSlotAudioSize(uint8_t slot, uint32_t audioSize) {
    if (slot >= NAND_SLOT_COUNT) return;
    _slots[slot].audioSize = audioSize;
}

void NandStorage::setSlotText(uint8_t slot, const char* text, uint16_t len) {
    if (slot >= NAND_SLOT_COUNT) return;
    if (!text || len == 0) {
        _slots[slot].textLen = 0;
        _slots[slot].text[0] = '\0';
        return;
    }
    // Truncate at a UTF-8 boundary (continuation bytes match (b & 0xC0) == 0x80).
    uint16_t copyLen = (len < SLOT_TEXT_MAX_LEN - 1) ? len : (SLOT_TEXT_MAX_LEN - 1);
    while (copyLen > 0 && (((uint8_t)text[copyLen]) & 0xC0) == 0x80) {
        copyLen--;
    }
    memcpy(_slots[slot].text, text, copyLen);
    _slots[slot].text[copyLen] = '\0';
    _slots[slot].textLen = copyLen;
}

uint16_t NandStorage::getSlotText(uint8_t slot, char* outBuf, size_t maxLen) const {
    if (slot >= NAND_SLOT_COUNT || !outBuf || maxLen == 0) return 0;
    uint16_t len = _slots[slot].textLen;
    if (len == 0) {
        outBuf[0] = '\0';
        return 0;
    }
    uint16_t copyLen = (len < maxLen - 1) ? len : (uint16_t)(maxLen - 1);
    memcpy(outBuf, _slots[slot].text, copyLen);
    outBuf[copyLen] = '\0';
    return copyLen;
}

int NandStorage::readAtSlot(uint32_t offset, uint8_t* buf, uint32_t len) {
    if (_currentSlot < 0 || len == 0) return 0;

    // Limit = the physical slot boundary, not dataSize (the audio sits after it).
    uint32_t slotSpan = ((_currentSlot + 1) < NAND_SLOT_COUNT)
                            ? (NAND_SLOT_ADDRS[_currentSlot + 1] - NAND_SLOT_ADDRS[_currentSlot])
                            : (0x1000000UL - NAND_SLOT_ADDRS[_currentSlot]);
    if (offset >= slotSpan) return 0;
    if (offset + len > slotSpan) len = slotSpan - offset;

    readRaw(NAND_SLOT_ADDRS[_currentSlot] + offset, buf, len);
    return (int)len;
}

SlotEntry NandStorage::getSlotInfo(uint8_t slot) const {
    if (slot < NAND_SLOT_COUNT) return _slots[slot];
    SlotEntry empty = {};
    return empty;
}

bool NandStorage::isSlotValid(uint8_t slot) const {
    if (slot >= NAND_SLOT_COUNT) return false;
    return (memcmp(_slots[slot].magic, "VJPG", 4) == 0 ||
            memcmp(_slots[slot].magic, "VIMG", 4) == 0 ||
            memcmp(_slots[slot].magic, "SLBX", 4) == 0);
}

bool NandStorage::isSlotVideo(uint8_t slot) const {
    if (slot >= NAND_SLOT_COUNT) return false;
    return memcmp(_slots[slot].magic, "VJPG", 4) == 0;
}

bool NandStorage::isSlotImage(uint8_t slot) const {
    if (slot >= NAND_SLOT_COUNT) return false;
    return memcmp(_slots[slot].magic, "VIMG", 4) == 0;
}

int8_t NandStorage::findFirstValidSlot() const {
    for (uint8_t i = 0; i < NAND_SLOT_COUNT; i++) {
        if (isSlotValid(i)) return i;
    }
    return -1;
}

int8_t NandStorage::findNextValidSlot(int8_t currentSlot) const {
    for (uint8_t i = 1; i <= NAND_SLOT_COUNT; i++) {
        uint8_t next = (currentSlot + i) % NAND_SLOT_COUNT;
        if (isSlotValid(next)) return next;
    }
    return -1;
}

bool NandStorage::openSlot(uint8_t slot) {
    if (!_tableValid || !isSlotValid(slot)) return false;

    _currentSlot = slot;
    _cursor = 0;
    _slotSize = _slots[slot].dataSize;
    return true;
}

int NandStorage::readData(uint8_t* buf, uint32_t len) {
    if (_currentSlot < 0 || _cursor >= _slotSize) return 0;

    uint32_t toRead = len;
    if (_cursor + toRead > _slotSize) toRead = _slotSize - _cursor;

    uint32_t addr = NAND_SLOT_ADDRS[_currentSlot] + _cursor;
    readRaw(addr, buf, toRead);
    _cursor += toRead;

    return (int)toRead;
}

void NandStorage::seekSlot(uint32_t offset) {
    _cursor = offset;
}

void NandStorage::closeSlot() {
    _currentSlot = -1;
    _cursor = 0;
    _slotSize = 0;
}

int8_t NandStorage::getCurrentSlot() const {
    return _currentSlot;
}

void NandStorage::readRaw(uint32_t addr, uint8_t* data, uint32_t len) {
    if (!acquireSPI()) return;

    SPI.beginTransaction(NAND_READ_SPI_SETTINGS);
    digitalWrite(PIN_NAND_CS, LOW);

    SPI.transfer(W25Q_READ_DATA);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    // Bulk transfer: ~0.45us/byte vs ~3.5us/byte for per-byte SPI.transfer().
    SPI.transferBytes(nullptr, data, len);

    digitalWrite(PIN_NAND_CS, HIGH);
    SPI.endTransaction();

    releaseSPI();
}

static constexpr uint8_t W25Q_WRITE_ENABLE = 0x06;
static constexpr uint8_t W25Q_SECTOR_ERASE = 0x20;
static constexpr uint8_t W25Q_BLOCK_ERASE_32K = 0x52;
static constexpr uint8_t W25Q_BLOCK_ERASE_64K = 0xD8;
static constexpr uint8_t W25Q_PAGE_PROGRAM = 0x02;

static void waitBusyInternal() {
    digitalWrite(PIN_NAND_CS, LOW);
    SPI.transfer(W25Q_READ_STATUS_1);
    while (SPI.transfer(0x00) & 0x01) {
        delayMicroseconds(100);
    }
    digitalWrite(PIN_NAND_CS, HIGH);
}

static void writeEnableInternal() {
    digitalWrite(PIN_NAND_CS, LOW);
    SPI.transfer(W25Q_WRITE_ENABLE);
    digitalWrite(PIN_NAND_CS, HIGH);
}

bool NandStorage::eraseSector(uint32_t addr) {
    // Never skip an erase silently: writing over unerased flash gives garbage with no error.
    if (!acquireSPI()) {
        DLOG("[NAND] ERR: eraseSector SPI timeout @ %lu", (unsigned long)addr);
        return false;
    }

    SPI.beginTransaction(NAND_SPI_SETTINGS);

    writeEnableInternal();

    digitalWrite(PIN_NAND_CS, LOW);
    SPI.transfer(W25Q_SECTOR_ERASE);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);
    digitalWrite(PIN_NAND_CS, HIGH);

    waitBusyInternal();

    SPI.endTransaction();
    releaseSPI();
    return true;
}

bool NandStorage::eraseRange(uint32_t addr, uint32_t len) {
    if (len == 0) return true;

    uint32_t current = addr;
    uint32_t remaining = len;

    while (remaining > 0) {
        if ((current % 65536U) == 0 && remaining >= 65536U) {
            if (!acquireSPI()) {
                DLOG("[NAND] ERR: erase64k SPI timeout @ %lu", (unsigned long)current);
                return false;
            }

            SPI.beginTransaction(NAND_SPI_SETTINGS);
            writeEnableInternal();

            digitalWrite(PIN_NAND_CS, LOW);
            SPI.transfer(W25Q_BLOCK_ERASE_64K);
            SPI.transfer((current >> 16) & 0xFF);
            SPI.transfer((current >> 8) & 0xFF);
            SPI.transfer(current & 0xFF);
            digitalWrite(PIN_NAND_CS, HIGH);

            waitBusyInternal();
            SPI.endTransaction();
            releaseSPI();

            current += 65536U;
            remaining -= 65536U;
            continue;
        }

        if ((current % 32768U) == 0 && remaining >= 32768U) {
            if (!acquireSPI()) {
                DLOG("[NAND] ERR: erase32k SPI timeout @ %lu", (unsigned long)current);
                return false;
            }

            SPI.beginTransaction(NAND_SPI_SETTINGS);
            writeEnableInternal();

            digitalWrite(PIN_NAND_CS, LOW);
            SPI.transfer(W25Q_BLOCK_ERASE_32K);
            SPI.transfer((current >> 16) & 0xFF);
            SPI.transfer((current >> 8) & 0xFF);
            SPI.transfer(current & 0xFF);
            digitalWrite(PIN_NAND_CS, HIGH);

            waitBusyInternal();
            SPI.endTransaction();
            releaseSPI();

            current += 32768U;
            remaining -= 32768U;
            continue;
        }

        if (!eraseSector(current & ~4095U)) return false;
        current = (current & ~4095U) + 4096U;
        if (remaining > 4096U) {
            remaining -= 4096U;
        } else {
            remaining = 0;
        }
    }
    return true;
}

bool NandStorage::writeRaw(uint32_t addr, const uint8_t* data, uint32_t len) {
    if (!data || len == 0) return false;
    // Report the failure, or the caller ends up with holes in the file.
    if (!acquireSPI()) {
        DLOG("[NAND] ERR: write SPI timeout @ %lu", (unsigned long)addr);
        return false;
    }

    SPI.beginTransaction(NAND_WRITE_SPI_SETTINGS);

    uint32_t currentAddr = addr;
    uint32_t bytesLeft = len;
    uint32_t dataOffset = 0;

    while (bytesLeft > 0) {
        writeEnableInternal();

        uint32_t pageOffset = currentAddr % 256;
        uint32_t chunkLen = 256 - pageOffset;
        if (chunkLen > bytesLeft) chunkLen = bytesLeft;

        digitalWrite(PIN_NAND_CS, LOW);
        SPI.transfer(W25Q_PAGE_PROGRAM);
        SPI.transfer((currentAddr >> 16) & 0xFF);
        SPI.transfer((currentAddr >> 8) & 0xFF);
        SPI.transfer(currentAddr & 0xFF);

        // Bulk write (see readRaw()): a 256B page takes ~120us instead of ~900us.
        SPI.writeBytes(data + dataOffset, chunkLen);
        digitalWrite(PIN_NAND_CS, HIGH);

        waitBusyInternal();

        currentAddr += chunkLen;
        dataOffset += chunkLen;
        bytesLeft -= chunkLen;
    }

    SPI.endTransaction();
    releaseSPI();
    return true;
}

bool NandStorage::acquireSPI() {
    if (_spiMutex == nullptr) return true;
    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE3));
        digitalWrite(PIN_TFT_DC, LOW);
        delayMicroseconds(2);
        SPI.transfer(0x00); //send NOP command data
        SPI.transfer(0x00);
        delayMicroseconds(2);
        digitalWrite(PIN_TFT_DC, HIGH); // data mode: the display ignores bus traffic
        SPI.endTransaction();

        return true;
    }
    DLOG("[NAND] ERR: SPI mutex timeout");
    return false;
}

void NandStorage::releaseSPI() {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}

void NandStorage::writeSlotTable() {
    uint8_t header[4 + NAND_SLOT_COUNT * sizeof(SlotEntry)];
    memcpy(header, "NSL3", 4);
    for (uint8_t i = 0; i < NAND_SLOT_COUNT; i++) {
        memcpy(header + 4 + i * sizeof(SlotEntry), &_slots[i], sizeof(SlotEntry));
    }
    // A failed slot-table write loses every slot's metadata: report it loudly.
    if (!eraseRange(0x000000, 4096) || !writeRaw(0x000000, header, sizeof(header))) {
        DLOG("[NAND] ERR: slot table write FAILED");
        return;
    }
    _tableValid = true;
}

void NandStorage::formatAll() {
    DLOG("[NAND] Formatting W25Q128...");

    if (!acquireSPI()) {
        DLOG("[NAND] ERR: format SPI timeout");
        return;
    }

    SPI.beginTransaction(NAND_SPI_SETTINGS);
    writeEnableInternal();

    digitalWrite(PIN_NAND_CS, LOW);
    SPI.transfer(0xC7); // Chip Erase
    digitalWrite(PIN_NAND_CS, HIGH);

    waitBusyInternal();
    SPI.endTransaction();
    releaseSPI();

    _currentSlot = -1;
    _cursor = 0;
    _slotSize = 0;
    memset(_slots, 0, sizeof(_slots));
    writeSlotTable();
    DLOG("[NAND] Format complete");
}
