#include "NandStorageProvider.h"
#include "ConfigManager.h"
#include "config.h"

int8_t NandStorageProvider::parseSlotId(const char* identifier) const {
    if (!identifier || identifier[0] == '\0') return -1;
    // Hỗ trợ cả chuỗi dạng "0", "1" hoặc "slot_0"
    if (strncmp(identifier, "slot_", 5) == 0) {
        return atoi(identifier + 5);
    }
    return atoi(identifier);
}

void NandStorageProvider::loadNvsState() {
    _prefs.begin("nand_queue", true);
    _unreadBitmask = _prefs.getUChar("unread_mask", 0);
    _writeSlotIndex = _prefs.getChar("write_idx", 0);
    _prefs.end();
}

void NandStorageProvider::saveNvsState() {
    _prefs.begin("nand_queue", false);
    _prefs.putUChar("unread_mask", _unreadBitmask);
    _prefs.putChar("write_idx", _writeSlotIndex);
    _prefs.end();
}

bool NandStorageProvider::init(SemaphoreHandle_t spiMutex) {
    loadNvsState();
    return _nand.init(spiMutex);
}

bool NandStorageProvider::openForRead(const char* identifier) {
    int8_t slot = parseSlotId(identifier);
    if (slot < 0 || slot >= NAND_SLOT_COUNT) return false;
    return _nand.openSlot(slot);
}

int NandStorageProvider::readData(uint8_t* buffer, uint32_t len) {
    return _nand.readData(buffer, len);
}

void NandStorageProvider::seek(uint32_t offset) {
    _nand.seekSlot(offset);
}

void NandStorageProvider::closeRead() {
    _nand.closeSlot();
}

StorageItemInfo NandStorageProvider::getItemInfo(const char* identifier) const {
    StorageItemInfo info;
    int8_t slot = parseSlotId(identifier);
    if (slot < 0) {
        slot = _nand.getCurrentSlot();
    }
    if (slot < 0 || slot >= NAND_SLOT_COUNT) return info;

    SlotEntry entry = _nand.getSlotInfo(slot);
    snprintf(info.id, sizeof(info.id), "%d", slot);
    info.dataSize = entry.dataSize;
    info.fps = entry.fps;
    info.totalFrames = entry.totalFrames;

    if (strncmp(entry.magic, "VJPG", 4) == 0) {
        info.type = StorageItemType::VIDEO;
    } else if (strncmp(entry.magic, "VIMG", 4) == 0 || strncmp(entry.magic, "SLBX", 4) == 0) {
        info.type = (entry.totalFrames > 1) ? StorageItemType::VIDEO : StorageItemType::IMAGE;
    } else {
        info.type = StorageItemType::EMPTY;
    }

    return info;
}

bool NandStorageProvider::openForWrite(const char* identifier) {
    int8_t slot = parseSlotId(identifier);
    if (slot < 0 || slot >= NAND_SLOT_COUNT) slot = 0;
    _writeSlotIndex = slot;
    // QUAN TRỌNG: Luôn khởi tạo _writeOffset = 4 để chừa 4 byte đầu (offset 0..3) sạch (0xFF)
    // cho kích thước frame (4-byte size header). Data thực sự ghi từ offset 4 trở đi.
    _writeOffset = 4;
    _lastErasedSectorAddr = 0xFFFFFFFF;

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];

    // Erase sector đầu tiên (4KB) để slot sạch 100%
    _nand.eraseSector(slotStartAddr);
    _lastErasedSectorAddr = slotStartAddr;

    Serial.printf("[NandStorageProvider] openForWrite: slot=%d addr=0x%06X (Reserved 4-byte header at offset 0)\n",
                  _writeSlotIndex, (unsigned int)slotStartAddr);
    return true;
}

size_t NandStorageProvider::writeChunk(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];
    uint32_t startAddr = slotStartAddr + _writeOffset;
    uint32_t endAddr = startAddr + len - 1;

    // Xác định sector bắt đầu và kết thúc của chunk này
    uint32_t startSector = startAddr & ~4095U;
    uint32_t endSector = endAddr & ~4095U;

    // Tự động xóa các sector chưa từng xóa trong lượt ghi này
    for (uint32_t sec = startSector; sec <= endSector; sec += 4096) {
        if (_lastErasedSectorAddr == 0xFFFFFFFF || sec > _lastErasedSectorAddr) {
            _nand.eraseSector(sec);
            _lastErasedSectorAddr = sec;
            Serial.printf("[NandStorageProvider] Erased sector at 0x%06X\n", (unsigned int)sec);
        }
    }

    _nand.writeRaw(startAddr, data, len);
    _writeOffset += len;

    return len;
}

void NandStorageProvider::closeWrite() {
    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];

    // Ghi kích thước dữ liệu (4 bytes) vào offset 0
    uint32_t rawJpegSize = (_writeOffset >= 4) ? (_writeOffset - 4) : 0;
    _nand.writeRaw(slotStartAddr, (const uint8_t*)&rawJpegSize, 4);

    // Kiểm tra 16 byte header container ở offset 4 (SLBX / SLOT / VJPG / VIMG)
    uint8_t header[16];
    _nand.readRaw(slotStartAddr + 4, header, 16);

    if (memcmp(header, "SLBX", 4) == 0) {
        uint16_t fps = *(uint16_t*)(header + 10);
        uint16_t totalFrames = (fps > 0) ? fps : 1;
        _nand.setSlotInfo(_writeSlotIndex, "SLBX", _writeOffset, (fps > 0) ? fps : 10, totalFrames);
        Serial.printf("[NandStorageProvider] Detected SendLove Box SLBX media (Total offset: %u bytes).\n", _writeOffset);
    } else if (memcmp(header, "SLOT", 4) == 0 || memcmp(header, "VJPG", 4) == 0 || memcmp(header, "VIMG", 4) == 0) {
        uint32_t dataSize = *(uint32_t*)(header + 4);
        uint16_t fps = *(uint16_t*)(header + 8);
        uint16_t totalFrames = *(uint16_t*)(header + 10);
        if (dataSize == 0 || dataSize > _writeOffset) dataSize = _writeOffset;
        const char* magic = (totalFrames > 1) ? "VJPG" : "VIMG";
        _nand.setSlotInfo(_writeSlotIndex, magic, dataSize, (fps > 0) ? fps : 10, totalFrames);
        Serial.printf("[NandStorageProvider] Detected pre-encoded container media (%u frames, %u FPS, magic: %s).\n", totalFrames, fps, magic);
    } else {
        _nand.setSlotInfo(_writeSlotIndex, "VIMG", _writeOffset, 1, 1);
        Serial.println(F("[NandStorageProvider] Raw JPEG media registered as VIMG."));
    }

    _unreadBitmask |= (1 << _writeSlotIndex);
    saveNvsState();
    _nand.writeSlotTable();
    Serial.printf("[NandStorageProvider] Closed slot %d. Media size: %u bytes (Total slot offset: %u). Unread mask: 0x%02X\n",
                  _writeSlotIndex, rawJpegSize, _writeOffset, _unreadBitmask);
}

bool NandStorageProvider::hasUnreadMessage() const {
    return (_unreadBitmask != 0);
}

bool NandStorageProvider::getNextUnreadIdentifier(char* outId, size_t maxLen) {
    if (!hasUnreadMessage() || !outId || maxLen == 0) return false;

    // Tìm slot chưa đọc cũ nhất bắt đầu từ writeSlotIndex
    for (int i = 0; i < NAND_SLOT_COUNT; i++) {
        int8_t slot = (_writeSlotIndex + i) % NAND_SLOT_COUNT;
        if (_unreadBitmask & (1 << slot)) {
            snprintf(outId, maxLen, "%d", slot);
            return true;
        }
    }
    return false;
}

void NandStorageProvider::markAsRead(const char* identifier) {
    int8_t slot = parseSlotId(identifier);
    if (slot >= 0 && slot < NAND_SLOT_COUNT) {
        _unreadBitmask &= ~(1 << slot);
        saveNvsState();
    }
}

bool NandStorageProvider::getFirstValidIdentifier(char* outId, size_t maxLen) const {
    int8_t slot = _nand.findFirstValidSlot();
    if (slot < 0 || !outId || maxLen == 0) return false;
    snprintf(outId, maxLen, "%d", slot);
    return true;
}

bool NandStorageProvider::getNextValidIdentifier(const char* currentId, char* outId, size_t maxLen) const {
    int8_t currSlot = parseSlotId(currentId);
    int8_t nextSlot = _nand.findNextValidSlot(currSlot);
    if (nextSlot < 0 || !outId || maxLen == 0) return false;
    snprintf(outId, maxLen, "%d", nextSlot);
    return true;
}

bool NandStorageProvider::formatStorage() {
    _nand.formatAll();
    _unreadBitmask = 0;
    _writeSlotIndex = 0;
    _writeOffset = 0;
    saveNvsState();

    // Reset mốc last_download_ts trong NVS về 0 để sẵn sàng tải tin nhắn mới từ đầu
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveLastDownloadTimestamp(0);
        cfg.end();
    }

    Serial.println(F("[NandStorageProvider] Full NAND storage formatted and lastTs reset to 0!"));
    return true;
}
