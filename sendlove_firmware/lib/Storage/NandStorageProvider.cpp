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
    Serial.printf("[NandStorageProvider] Loaded NVS: unread_mask=0x%02X (bin: %d%d%d%d%d), write_idx=%d\n",
                  _unreadBitmask,
                  (_unreadBitmask >> 4) & 1, (_unreadBitmask >> 3) & 1,
                  (_unreadBitmask >> 2) & 1, (_unreadBitmask >> 1) & 1,
                  _unreadBitmask & 1, _writeSlotIndex);
}

void NandStorageProvider::saveNvsState() {
    _prefs.begin("nand_queue", false);
    _prefs.putUChar("unread_mask", _unreadBitmask);
    _prefs.putChar("write_idx", _writeSlotIndex);
    _prefs.end();
    Serial.printf("[NandStorageProvider] Saved NVS: unread_mask=0x%02X (bin: %d%d%d%d%d), write_idx=%d\n",
                  _unreadBitmask,
                  (_unreadBitmask >> 4) & 1, (_unreadBitmask >> 3) & 1,
                  (_unreadBitmask >> 2) & 1, (_unreadBitmask >> 1) & 1,
                  _unreadBitmask & 1, _writeSlotIndex);
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
    if (slot < 0 || slot >= NAND_SLOT_COUNT) {
        slot = _writeSlotIndex; // Mặc định dùng cur_point
    }

    // Kiểm tra nếu slot tại cur_point đang chứa tin nhắn chưa đọc -> từ chối ghi (Bộ nhớ đầy)
    if (_unreadBitmask & (1 << slot)) {
        Serial.printf("[NandStorageProvider] ERROR: Slot %d contains UNREAD message! Storage full (unread_mask=0x%02X).\n",
                      slot, _unreadBitmask);
        return false;
    }

    _writeSlotIndex = slot;
    _writeOffset = 4;
    _slotCapacity = ((slot + 1) < NAND_SLOT_COUNT) ? (NAND_SLOT_ADDRS[slot + 1] - NAND_SLOT_ADDRS[slot])
                                                   : (0x1000000UL - NAND_SLOT_ADDRS[slot]);

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];
    _nand.eraseRange(slotStartAddr, _slotCapacity);

    Serial.printf("[NandStorageProvider] openForWrite: slot=%d addr=0x%06X slotCap=%lu init _writeOffset=%lu _unreadBitmask=0x%02X\n",
                  _writeSlotIndex, (unsigned int)slotStartAddr, (unsigned long)_slotCapacity,
                  (unsigned long)_writeOffset, _unreadBitmask);
    return true;
}

size_t NandStorageProvider::writeChunk(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];
    uint32_t startAddr = slotStartAddr + _writeOffset;

    if (_slotCapacity == 0 || (uint64_t)_writeOffset + (uint64_t)len > _slotCapacity) {
        Serial.printf("[NandStorageProvider] ERROR: write exceeds slot capacity. slot=%d offset=%lu len=%u cap=%lu\n",
                      _writeSlotIndex, (unsigned long)_writeOffset, (unsigned)len, (unsigned long)_slotCapacity);
        return 0;
    }

    _nand.writeRaw(startAddr, data, len);
    uint32_t prevOffset = _writeOffset;
    _writeOffset += len;

    // Log định kỳ mỗi 10KB hoặc chunk đầu tiên để theo dõi tiến độ _writeOffset mà không làm trập Serial
    if (prevOffset == 4 || (_writeOffset / 10240) != (prevOffset / 10240)) {
        Serial.printf("[MONITOR] writeChunk: slot=%d _writeOffset %lu -> %lu / capacity %lu\n",
                      _writeSlotIndex, (unsigned long)prevOffset, (unsigned long)_writeOffset, (unsigned long)_slotCapacity);
    }

    return len;
}

void NandStorageProvider::closeWrite() {
    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];

    Serial.printf("[MONITOR] closeWrite: slot=%d final _writeOffset=%lu. Updating header & unreadBitmask...\n",
                  _writeSlotIndex, (unsigned long)_writeOffset);

    // Ghi kích thước dữ liệu (4 bytes) vào offset 0
    uint32_t rawJpegSize = (_writeOffset >= 4) ? (_writeOffset - 4) : 0;
    _nand.writeRaw(slotStartAddr, (const uint8_t*)&rawJpegSize, 4);

    // Kiểm tra 16 byte header container ở offset 4 (SLBX / SLOT / VJPG / VIMG)
    uint8_t header[16];
    _nand.readRaw(slotStartAddr + 4, header, 16);

    if (memcmp(header, "SLBX", 4) == 0) {
        uint8_t mediaType = header[5];
        uint16_t fps = 0;
        uint16_t totalFrames = 1;
        memcpy(&fps, header + 10, sizeof(fps));
        memcpy(&totalFrames, header + 11, sizeof(totalFrames));

        uint16_t finalFps = (fps > 0) ? fps : 1;
        if (mediaType == 0x02 || totalFrames <= 1) {
            totalFrames = 1;
            _nand.setSlotInfo(_writeSlotIndex, "VIMG", _writeOffset, finalFps, totalFrames);
            Serial.printf("[NandStorageProvider] Detected SLBX image (type=0x%02X, fps=%u, frames=%u, total offset=%u).\n",
                          mediaType, finalFps, totalFrames, _writeOffset);
        } else {
            if (totalFrames == 0) totalFrames = 1;
            _nand.setSlotInfo(_writeSlotIndex, "VJPG", _writeOffset, finalFps, totalFrames);
            Serial.printf("[NandStorageProvider] Detected SLBX video (type=0x%02X, fps=%u, frames=%u, total offset=%u).\n",
                          mediaType, finalFps, totalFrames, _writeOffset);
        }
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

    // 1. Đánh dấu bit thứ cur_point là chưa đọc (1)
    uint8_t oldUnread = _unreadBitmask;
    _unreadBitmask |= (1 << _writeSlotIndex);

    // 2. Dịch tiến con trỏ cur_point sang Slot tiếp theo (0..4)
    int8_t writtenSlot = _writeSlotIndex;
    _writeSlotIndex = (_writeSlotIndex + 1) % NAND_SLOT_COUNT;
    _slotCapacity = 0;

    saveNvsState();
    _nand.writeSlotTable();
    Serial.printf("[MONITOR] Written Slot %d (_writeOffset=%lu). Next cur_point->%d. _unreadBitmask: 0x%02X -> 0x%02X\n",
                  writtenSlot, (unsigned long)_writeOffset, _writeSlotIndex, oldUnread, _unreadBitmask);
}

bool NandStorageProvider::isFull() const {
    return (_unreadBitmask == 0x1F) || ((_unreadBitmask & (1 << _writeSlotIndex)) != 0);
}

bool NandStorageProvider::getNextWriteSlotIdentifier(char* outId, size_t maxLen) {
    if (!outId || maxLen == 0) return false;
    if (isFull()) {
        Serial.printf("[NandStorageProvider] Storage FULL! All slots unread (unread_mask=0x%02X).\n", _unreadBitmask);
        return false;
    }
    snprintf(outId, maxLen, "%d", _writeSlotIndex);
    return true;
}

bool NandStorageProvider::hasUnreadMessage() const {
    return (_unreadBitmask != 0);
}

bool NandStorageProvider::getNextUnreadIdentifier(char* outId, size_t maxLen) {
    if (!hasUnreadMessage() || !outId || maxLen == 0) return false;

    // Duyệt tìm tin chưa đọc CŨ NHẤT bắt đầu từ cur_point theo vòng tròn modulo
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
        uint8_t oldUnread = _unreadBitmask;
        _unreadBitmask &= ~(1 << slot); // Xóa bit thứ slot về 0 (đã đọc)
        saveNvsState();
        Serial.printf("[MONITOR] Marked slot %d as READ. _unreadBitmask: 0x%02X -> 0x%02X\n",
                      slot, oldUnread, _unreadBitmask);
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
    _slotCapacity = 0;
    saveNvsState();

    // Reset mốc last_download_ts trong NVS về 0 để sẵn sàng tải tin nhắn mới từ đầu
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveLastDownloadTimestamp(0);
        cfg.end();
    }

    Serial.println(F("[MONITOR] Full NAND storage formatted! Reset _unreadBitmask=0x00, _writeOffset=0"));
    return true;
}
