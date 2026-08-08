#include "NandStorageProvider.h"
#include "ConfigManager.h"
#include "ScreenLogger.h"
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
    DLOG("[NANDP] loaded NVS mask=0x%02X", _unreadBitmask);
}

void NandStorageProvider::saveNvsState() {
    _prefs.begin("nand_queue", false);
    _prefs.putUChar("unread_mask", _unreadBitmask);
    _prefs.putChar("write_idx", _writeSlotIndex);
    _prefs.end();
    // DLOG saved every time -> dropped
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
    info.maxDisplayTime = entry.maxDisplayTime > 0 ? entry.maxDisplayTime : 60;

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
        DLOG("[NANDP] ERR: slot %d unread full", slot);
        return false;
    }

    _writeSlotIndex = slot;
    _writeOffset = 4;
    _slotCapacity = ((slot + 1) < NAND_SLOT_COUNT) ? (NAND_SLOT_ADDRS[slot + 1] - NAND_SLOT_ADDRS[slot])
                                                   : (0x1000000UL - NAND_SLOT_ADDRS[slot]);

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];
    _nand.eraseRange(slotStartAddr, _slotCapacity);

    DLOG("[NANDP] open write slot %d", _writeSlotIndex);
    return true;
}

size_t NandStorageProvider::writeChunk(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];
    uint32_t startAddr = slotStartAddr + _writeOffset;

    if (_slotCapacity == 0 || (uint64_t)_writeOffset + (uint64_t)len > _slotCapacity) {
        DLOG("[NANDP] ERR: write exceed cap");
        return 0;
    }

    _nand.writeRaw(startAddr, data, len);
    uint32_t prevOffset = _writeOffset;
    _writeOffset += len;

    // Dropped periodic monitor writeChunk

    return len;
}

void NandStorageProvider::closeWrite(uint32_t maxDisplayTime) {
    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_writeSlotIndex];

    DLOG("[NANDP] closeWrite slot %d", _writeSlotIndex);

    // Ghi kích thước dữ liệu (4 bytes) vào offset 0
    uint32_t rawJpegSize = (_writeOffset >= 4) ? (_writeOffset - 4) : 0;
    _nand.writeRaw(slotStartAddr, (const uint8_t*)&rawJpegSize, 4);

    // Kiểm tra 16 byte header container ở offset 4 (SLBX / SLOT / VJPG / VIMG)
    uint8_t header[16];
    _nand.readRaw(slotStartAddr + 4, header, 16);

    if (memcmp(header, "SLBX", 4) == 0) {
        uint8_t mediaType = header[5];
        uint8_t fps      = header[10];            // 1 byte
        uint16_t totalFrames = 1;
        memcpy(&totalFrames, header + 11, sizeof(totalFrames)); // 2 bytes

        uint16_t finalFps = (fps > 0) ? fps : 1;
        if (mediaType == 0x02 || totalFrames <= 1) {
            totalFrames = 1;
            _nand.setSlotInfo(_writeSlotIndex, "VIMG", _writeOffset, finalFps, totalFrames, maxDisplayTime);
            // Dropped Detected SLBX image
        } else {
            if (totalFrames == 0) totalFrames = 1;
            _nand.setSlotInfo(_writeSlotIndex, "VJPG", _writeOffset, finalFps, totalFrames, maxDisplayTime);
            // Dropped Detected SLBX video
        }
    } else if (memcmp(header, "SLOT", 4) == 0 || memcmp(header, "VJPG", 4) == 0 || memcmp(header, "VIMG", 4) == 0) {
        uint32_t dataSize = *(uint32_t*)(header + 4);
        uint16_t fps = *(uint16_t*)(header + 8);
        uint16_t totalFrames = *(uint16_t*)(header + 10);
        if (dataSize == 0 || dataSize > _writeOffset) dataSize = _writeOffset;
        const char* magic = (totalFrames > 1) ? "VJPG" : "VIMG";
        _nand.setSlotInfo(_writeSlotIndex, magic, dataSize, (fps > 0) ? fps : 10, totalFrames, maxDisplayTime);
        // Dropped Detected pre-encoded
    } else {
        _nand.setSlotInfo(_writeSlotIndex, "VIMG", _writeOffset, 1, 1, maxDisplayTime);
        // Dropped Raw JPEG
    }

    // 1. Đánh dấu bit thứ cur_point là chưa đọc (1)
    uint8_t oldUnread = _unreadBitmask;
    _unreadBitmask |= (1 << _writeSlotIndex);

    // 2. Dịch tiến con trỏ cur_point sang Slot tiếp theo (0..4)
    int8_t writtenSlot = _writeSlotIndex;
    _writeSlotIndex = (_writeSlotIndex + 1) % NAND_SLOT_COUNT;
    _slotCapacity = 0;

    // Ghi slot table vào NAND TRƯỚC khi commit unread bitmask vào NVS.
    // Nếu bị reset giữa chừng, NAND sẽ có data hợp lệ trước khi NVS biết slot là unread.
    _nand.writeSlotTable();
    saveNvsState();
    DLOG("[NANDP] written slot %d next %d", writtenSlot, _writeSlotIndex);
}

bool NandStorageProvider::isFull() const {
    return (_unreadBitmask == 0x1F) || ((_unreadBitmask & (1 << _writeSlotIndex)) != 0);
}

bool NandStorageProvider::getNextWriteSlotIdentifier(char* outId, size_t maxLen) {
    if (!outId || maxLen == 0) return false;
    if (isFull()) {
        DLOG("[NANDP] FULL! all slots unread");
        return false;
    }
    snprintf(outId, maxLen, "%d", _writeSlotIndex);
    return true;
}

bool NandStorageProvider::hasUnreadMessage() const {
    return (_unreadBitmask != 0);
}

uint8_t NandStorageProvider::getUnreadCount() const {
    uint8_t count = 0;
    for (int i = 0; i < NAND_SLOT_COUNT; i++) {
        if (_unreadBitmask & (1 << i)) count++;
    }
    return count;
}

bool NandStorageProvider::getNextUnreadIdentifier(char* outId, size_t maxLen) {
    if (!hasUnreadMessage() || !outId || maxLen == 0) return false;

    // Duyệt tìm tin chưa đọc CŨ NHẤT bắt đầu từ cur_point theo vòng tròn modulo.
    // Nếu slot được đánh dấu unread nhưng data thực tế không đọc được (bị ngắt khi write),
    // tự động xóa bit và bỏ qua slot đó để tránh treo vĩnh viễn.
    for (int i = 0; i < NAND_SLOT_COUNT; i++) {
        int8_t slot = (_writeSlotIndex + i) % NAND_SLOT_COUNT;
        if (!(_unreadBitmask & (1 << slot))) continue;

        if (!_nand.openSlot(slot)) {
            DLOG("[NANDP] WARN: slot %d unreadable", slot);
            _unreadBitmask &= ~(1 << slot);
            saveNvsState();
            continue;
        }
        _nand.closeSlot();

        snprintf(outId, maxLen, "%d", slot);
        return true;
    }
    return false;
}

void NandStorageProvider::markAsRead(const char* identifier) {
    int8_t slot = parseSlotId(identifier);
    if (slot >= 0 && slot < NAND_SLOT_COUNT) {
        uint8_t oldUnread = _unreadBitmask;
        _unreadBitmask &= ~(1 << slot); // Xóa bit thứ slot về 0 (đã đọc)
        saveNvsState();
        DLOG("[NANDP] marked slot %d READ", slot);
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

    DLOG("[NANDP] FULL format done");
    return true;
}
