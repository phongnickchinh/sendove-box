#include "NandStorageProvider.h"
#include "ConfigManager.h"
#include "ScreenLogger.h"
#include "config.h"

int8_t NandStorageProvider::parseSlotId(const char* identifier) const {
    if (!identifier || identifier[0] == '\0') return -1;
    // Accepts both "0", "1" and "slot_0"
    if (strncmp(identifier, "slot_", 5) == 0) {
        return atoi(identifier + 5);
    }
    return atoi(identifier);
}

uint32_t NandStorageProvider::slotSpan(int8_t slot) {
    if (slot < 0 || slot >= NAND_SLOT_COUNT) return 0;
    return ((slot + 1) < NAND_SLOT_COUNT) ? (NAND_SLOT_ADDRS[slot + 1] - NAND_SLOT_ADDRS[slot])
                                          : (0x1000000UL - NAND_SLOT_ADDRS[slot]);
}

void NandStorageProvider::loadNvsState() {
    _prefs.begin("nand_queue", true);
    _unreadBitmask = _prefs.getUChar("unread_mask", 0);
    _writeSlotIndex = _prefs.getChar("write_idx", 0);
    _prefs.end();

    // Drop NVS state left from the old 5-slot layout, or isFull() stays true forever.
    _unreadBitmask &= SLOT_ALL_MASK;
    if (_writeSlotIndex < 0 || _writeSlotIndex >= NAND_SLOT_COUNT) _writeSlotIndex = 0;
    _activeSlot = _writeSlotIndex;
    DLOG("[NANDP] loaded NVS mask=0x%02X", _unreadBitmask);
}

void NandStorageProvider::saveNvsState() {
    _prefs.begin("nand_queue", false);
    _prefs.putUChar("unread_mask", _unreadBitmask);
    _prefs.putChar("write_idx", _writeSlotIndex);
    _prefs.end();
}

bool NandStorageProvider::init(SemaphoreHandle_t spiMutex) {
    bool ok = _nand.init(spiMutex);
    loadNvsState();

    // A magic change wipes the slot table but not unread_mask in NVS: clear it too,
    // or "unread" would point at an empty slot.
    if (ok && !_nand.isTableValid() && (_unreadBitmask != 0 || _writeSlotIndex != 0)) {
        DLOG("[NANDP] bang slot moi -> reset hang cho NVS");
        _unreadBitmask  = 0;
        _writeSlotIndex = 0;
        _activeSlot     = 0;
        saveNvsState();
    }
    return ok;
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

int NandStorageProvider::readAt(uint32_t offset, uint8_t* buffer, uint32_t len) {
    return _nand.readAtSlot(offset, buffer, len);
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
    info.audioSize = entry.audioSize;
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
        slot = _writeSlotIndex; // default: the queue cursor
    }

    // The cursor's slot is still unread: storage full
    if (_unreadBitmask & (1 << slot)) {
        DLOG("[NANDP] ERR: slot %d unread full", slot);
        return false;
    }

    _activeSlot = slot;
    _writeOffset = 4;
    _slotCapacity = slotSpan(slot);

    // Erase-as-you-write: only the first 64KB block now. Erasing the whole slot
    // (15-25s) stalls the open HTTP socket until the download times out.
    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_activeSlot];
    uint32_t initialEraseLen = (_slotCapacity < 65536U) ? _slotCapacity : 65536U;
    if (!_nand.eraseRange(slotStartAddr, initialEraseLen)) {
        DLOG("[NANDP] ERR: initial erase FAILED slot %d", _activeSlot);
        _slotCapacity = 0;
        return false;
    }
    _erasedUpToAddr = slotStartAddr + initialEraseLen;

    DLOG("[NANDP] open write slot %d", _activeSlot);
    return true;
}

size_t NandStorageProvider::writeChunk(const uint8_t* data, size_t len) {
    if (!data || len == 0) return 0;

    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_activeSlot];
    uint32_t startAddr = slotStartAddr + _writeOffset;
    uint32_t endAddr = startAddr + (uint32_t)len;

    if (_slotCapacity == 0 || (uint64_t)_writeOffset + (uint64_t)len > _slotCapacity) {
        DLOG("[NANDP] ERR: write exceed cap");
        return 0;
    }

    // Erase the next 64KB block before the cursor reaches unerased space.
    // INVARIANT: erase from _erasedUpToAddr, NEVER from startAddr — eraseRange()
    // rounds an unaligned address down and would erase data just written.
    uint32_t slotEndAddr = slotStartAddr + _slotCapacity;
    while (_erasedUpToAddr < endAddr && _erasedUpToAddr < slotEndAddr) {
        uint32_t toErase = 65536U;
        if (_erasedUpToAddr + toErase > slotEndAddr) {
            toErase = slotEndAddr - _erasedUpToAddr;
        }
        if (!_nand.eraseRange(_erasedUpToAddr, toErase)) {
            DLOG("[NANDP] ERR: erase-ahead FAILED @ %lu", (unsigned long)_erasedUpToAddr);
            return 0;   // short count = write error for NetworkManager
        }
        _erasedUpToAddr += toErase;
    }

    // Report the real result, never `return len`: a failed mutex take would
    // leave holes in a "successful" download.
    if (!_nand.writeRaw(startAddr, data, len)) {
        DLOG("[NANDP] ERR: write failed @ %lu", (unsigned long)startAddr);
        return 0;
    }
    _writeOffset += len;

    return len;
}

void NandStorageProvider::closeWrite(uint32_t maxDisplayTime) {
    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_activeSlot];

    DLOG("[NANDP] closeWrite slot %d", _activeSlot);

    // Data size (4 bytes) at offset 0
    uint32_t rawJpegSize = (_writeOffset >= 4) ? (_writeOffset - 4) : 0;
    if (!_nand.writeRaw(slotStartAddr, (const uint8_t*)&rawJpegSize, 4)) {
        DLOG("[NANDP] ERR: size header write FAILED slot %d", _activeSlot);
    }

    // Container header at offset 4 (SLBX / SLOT / VJPG / VIMG)
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
            _nand.setSlotInfo(_activeSlot, "VIMG", _writeOffset, finalFps, totalFrames, maxDisplayTime);
        } else {
            if (totalFrames == 0) totalFrames = 1;
            _nand.setSlotInfo(_activeSlot, "VJPG", _writeOffset, finalFps, totalFrames, maxDisplayTime);
        }
    } else if (memcmp(header, "SLOT", 4) == 0 || memcmp(header, "VJPG", 4) == 0 || memcmp(header, "VIMG", 4) == 0) {
        uint32_t dataSize = *(uint32_t*)(header + 4);
        uint16_t fps = *(uint16_t*)(header + 8);
        uint16_t totalFrames = *(uint16_t*)(header + 10);
        if (dataSize == 0 || dataSize > _writeOffset) dataSize = _writeOffset;
        const char* magic = (totalFrames > 1) ? "VJPG" : "VIMG";
        _nand.setSlotInfo(_activeSlot, magic, dataSize, (fps > 0) ? fps : 10, totalFrames, maxDisplayTime);
    } else {
        _nand.setSlotInfo(_activeSlot, "VIMG", _writeOffset, 1, 1, maxDisplayTime);
    }

    // 1. Mark the slot unread
    _unreadBitmask |= (1 << _activeSlot);

    // 2. Remember slot + offset for openForAppend (audio)
    _lastWrittenSlot   = _activeSlot;
    _lastWrittenOffset = _writeOffset;

    // 3. Advance the queue cursor
    int8_t writtenSlot = _activeSlot;
    _writeSlotIndex = (_activeSlot + 1) % NAND_SLOT_COUNT;
    _slotCapacity = 0;

    // Slot table to flash BEFORE the unread bitmask to NVS (safe across a reset midway).
    _nand.writeSlotTable();
    saveNvsState();
    DLOG("[NANDP] written slot %d next %d", writtenSlot, _writeSlotIndex);
}

void NandStorageProvider::discardWrite() {
    if (_activeSlot >= 0 && _activeSlot < NAND_SLOT_COUNT) {
        DLOG("[NANDP] discardWrite slot %d (bo %lu byte do)", _activeSlot, (unsigned long)_writeOffset);
        // Clear the slot's magic so the partly erased area isn't taken for a valid item.
        const char emptyMagic[4] = {0, 0, 0, 0};
        _nand.setSlotInfo(_activeSlot, emptyMagic, 0, 0, 0, 0);
        _nand.writeSlotTable();

        // The image-less path committed a placeholder before its audio download:
        // undo the unread bit and step the cursor back so the next sync reuses this slot.
        if (_unreadBitmask & (1 << _activeSlot)) {
            _unreadBitmask &= ~(1 << _activeSlot);
            _writeSlotIndex = _activeSlot;
            saveNvsState();
        }
    }
    _writeOffset = 0;
    _slotCapacity = 0;
    _erasedUpToAddr = 0;
}

void NandStorageProvider::setItemText(const char* identifier, const char* text) {
    int8_t slot = parseSlotId(identifier);
    if (slot < 0 || slot >= NAND_SLOT_COUNT || !text) return;
    _nand.setSlotText(slot, text, (uint16_t)strlen(text));
    _nand.writeSlotTable();
    DLOG("[NANDP] setItemText slot %d (%u bytes)", slot, (unsigned)strlen(text));
}

bool NandStorageProvider::getItemText(const char* identifier, char* outBuf, size_t maxLen) const {
    int8_t slot = parseSlotId(identifier);
    if (slot < 0) slot = _nand.getCurrentSlot();
    if (slot < 0 || slot >= NAND_SLOT_COUNT || !outBuf || maxLen == 0) return false;
    return _nand.getSlotText(slot, outBuf, maxLen) > 0;
}

bool NandStorageProvider::openForAppend(const char* identifier) {
    int8_t slot = parseSlotId(identifier);
    if (slot < 0 || slot >= NAND_SLOT_COUNT) {
        // Fall back to the slot just written
        slot = _lastWrittenSlot;
    }
    if (slot < 0) return false;

    // No re-erase: continue where the video write ended. Set ONLY _activeSlot;
    // pulling _writeSlotIndex back would make isFull() report full.
    _activeSlot   = slot;
    _writeOffset  = _lastWrittenOffset;
    _slotCapacity = slotSpan(slot);

    // Recompute _erasedUpToAddr from _writeOffset rather than trust the last session's value.
    uint32_t slotStartAddr = NAND_SLOT_ADDRS[_activeSlot];
    uint32_t currentPos = slotStartAddr + _writeOffset;
    _erasedUpToAddr = ((currentPos + 65535U) / 65536U) * 65536U;
    if (_erasedUpToAddr > slotStartAddr + _slotCapacity) {
        _erasedUpToAddr = slotStartAddr + _slotCapacity;
    }

    DLOG("[NANDP] openForAppend slot %d @ offset %lu", slot, (unsigned long)_writeOffset);
    return true;
}

void NandStorageProvider::closeAppend() {
    if (_activeSlot < 0 || _activeSlot >= NAND_SLOT_COUNT) return;
    if (_activeSlot != _lastWrittenSlot) return;   // not an append session

    uint32_t audioSize = (_writeOffset > _lastWrittenOffset) ? (_writeOffset - _lastWrittenOffset) : 0;
    _nand.setSlotAudioSize(_activeSlot, audioSize);
    _nand.writeSlotTable();
    _slotCapacity = 0;
    DLOG("[NANDP] closeAppend slot %d audio=%lu", _activeSlot, (unsigned long)audioSize);
}

bool NandStorageProvider::isFull() const {
    return ((_unreadBitmask & SLOT_ALL_MASK) == SLOT_ALL_MASK) ||
           ((_unreadBitmask & (1 << _writeSlotIndex)) != 0);
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

    // Oldest unread first, scanning from the queue cursor. An unread slot that
    // can't be read is cleared and skipped, so the box never gets stuck on it.
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
        _unreadBitmask &= ~(1 << slot);
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
    _activeSlot = 0;
    _lastWrittenSlot = -1;
    _lastWrittenOffset = 0;
    _writeOffset = 0;
    _slotCapacity = 0;
    _erasedUpToAddr = 0;
    saveNvsState();

    // Reset last_download_ts so messages download from scratch
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveLastDownloadTimestamp(0);
        cfg.end();
    }

    DLOG("[NANDP] FULL format done");
    return true;
}
