#include "NandStorage.h"

// ============================================================================
// NandStorage Implementation — Hardware SPI2
// ============================================================================

// W25Q128 SPI commands
static constexpr uint8_t W25Q_READ_DATA      = 0x03;
static constexpr uint8_t W25Q_READ_STATUS_1  = 0x05;

// SPI transaction settings cho NAND 
// ĐỔI SANG MODE 3 ĐỂ CÙNG CLOCK POLARITY VỚI ST7789 (Tránh lệch bit khi nhảy SCK)
static const SPISettings NAND_SPI_SETTINGS(20000000, MSBFIRST, SPI_MODE3);

// ============================================================================
// Init
// ============================================================================

bool NandStorage::init(SemaphoreHandle_t spiMutex) {
    _spiMutex = spiMutex;

    // Cấu hình CS pin
    pinMode(PIN_NAND_CS, OUTPUT);
    digitalWrite(PIN_NAND_CS, HIGH);

    Serial.println(F("[NAND] Reading Slot Table..."));
    Serial.flush();

    // Đọc Slot Table (256 bytes đầu tiên tại địa chỉ 0x000000)
    uint8_t header[4 + NAND_SLOT_COUNT * sizeof(SlotEntry)];
    readRaw(0, header, sizeof(header));

    // Kiểm tra magic "NSLT"
    if (memcmp(header, "NSLT", 4) != 0) {
        Serial.printf("[NAND] ERROR: Invalid magic 0x%02X 0x%02X 0x%02X 0x%02X\n", 
                      header[0], header[1], header[2], header[3]);
        Serial.flush();
        _tableValid = false;
        return false;
    }

    // Parse slot entries
    for (uint8_t i = 0; i < NAND_SLOT_COUNT; i++) {
        memcpy(&_slots[i], header + 4 + i * sizeof(SlotEntry), sizeof(SlotEntry));
    }

    _tableValid = true;

    // Log slot info
    Serial.println(F("[NAND] Slot Table OK: \"NSLT\""));
    for (uint8_t i = 0; i < NAND_SLOT_COUNT; i++) {
        if (isSlotValid(i)) {
            char magic[5] = {0};
            memcpy(magic, _slots[i].magic, 4);
            Serial.printf("[NAND] Slot %d: %s, %u frames, %u FPS, %u bytes\n",
                          i, magic, _slots[i].totalFrames, _slots[i].fps,
                          _slots[i].dataSize);
        } else {
            Serial.printf("[NAND] Slot %d: (empty)\n", i);
        }
    }

    return true;
}

// ============================================================================
// Slot Query
// ============================================================================

SlotEntry NandStorage::getSlotInfo(uint8_t slot) const {
    if (slot < NAND_SLOT_COUNT) return _slots[slot];
    SlotEntry empty = {};
    return empty;
}

bool NandStorage::isSlotValid(uint8_t slot) const {
    if (slot >= NAND_SLOT_COUNT) return false;
    return (memcmp(_slots[slot].magic, "VJPG", 4) == 0 ||
            memcmp(_slots[slot].magic, "VIMG", 4) == 0);
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

// ============================================================================
// Sequential Read
// ============================================================================

bool NandStorage::openSlot(uint8_t slot) {
    if (!_tableValid || !isSlotValid(slot)) return false;

    _currentSlot = slot;
    _cursor = 0;
    _slotSize = _slots[slot].dataSize;

    char magic[5] = {0};
    memcpy(magic, _slots[slot].magic, 4);
    Serial.printf("[NAND] Opened slot %d (%s, %u bytes)\n", slot, magic, _slotSize);
    return true;
}

int NandStorage::readData(uint8_t* buf, uint32_t len) {
    if (_currentSlot < 0 || _cursor >= _slotSize) return 0;

    uint32_t toRead = len;
    if (_cursor + toRead > _slotSize) {
        toRead = _slotSize - _cursor;
    }

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

// ============================================================================
// Low-level SPI
// ============================================================================

void NandStorage::readRaw(uint32_t addr, uint8_t* data, uint32_t len) {
    if (!acquireSPI()) return;

    SPI.beginTransaction(NAND_SPI_SETTINGS);
    digitalWrite(PIN_NAND_CS, LOW);

    SPI.transfer(W25Q_READ_DATA);
    SPI.transfer((addr >> 16) & 0xFF);
    SPI.transfer((addr >> 8) & 0xFF);
    SPI.transfer(addr & 0xFF);

    // Đọc block dữ liệu
    for (uint32_t i = 0; i < len; i++) {
        data[i] = SPI.transfer(0x00);
    }

    digitalWrite(PIN_NAND_CS, HIGH);
    SPI.endTransaction();

    releaseSPI();
}

bool NandStorage::acquireSPI() {
    if (_spiMutex == nullptr) return true;
    if (xSemaphoreTake(_spiMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // [HACK BẢO VỆ ST7789]
        // Vì màn hình ST7789 không có CS, nó sẽ lắng nghe mọi tín hiệu trên SCK/MOSI.
        // Ta cần gửi lệnh NOP (0x00) cho màn hình trước, sau đó kéo DC=HIGH.
        // Mọi tín hiệu của NAND lúc này ST7789 sẽ tưởng là tham số rác của NOP và bỏ qua an toàn.
        SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE3)); // Màn hình dùng MODE 3
        digitalWrite(PIN_TFT_DC, LOW);  // Command Mode
        delayMicroseconds(2);           // Chờ ổn định chân DC
        SPI.transfer(0x00);             // Gửi NOP command
        SPI.transfer(0x00);             // Gửi thêm 1 NOP cho chắc chắn
        delayMicroseconds(2);
        digitalWrite(PIN_TFT_DC, HIGH); // Data Mode -> Bắt đầu bỏ qua data
        SPI.endTransaction();

        return true;
    }
    Serial.println(F("[NAND] ERROR: SPI mutex timeout"));
    return false;
}

void NandStorage::releaseSPI() {
    if (_spiMutex != nullptr) {
        xSemaphoreGive(_spiMutex);
    }
}
