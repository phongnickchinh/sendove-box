#include "SDStorageProvider.h"

#include "ConfigManager.h"
#include "ScreenLogger.h"
#include "config.h"

// Enough for "/media/slot_19.bin" (19 chars) plus margin
static constexpr size_t SD_PATH_MAX = 48;

// ============================================================================
// Helpers
// ============================================================================

int8_t SDStorageProvider::parseIndex(const char* identifier) const {
    if (!identifier || identifier[0] == '\0') return -1;
    const char* p = (strncmp(identifier, "slot_", 5) == 0) ? identifier + 5 : identifier;
    if (*p < '0' || *p > '9') return -1;
    int v = atoi(p);
    if (v < 0 || v >= SD_SLOT_COUNT) return -1;
    return (int8_t)v;
}

void SDStorageProvider::buildPath(int8_t idx, char* out, size_t maxLen) const {
    snprintf(out, maxLen, "%s/slot_%02u.bin", SD_MEDIA_DIR, (unsigned)idx);
}

void SDStorageProvider::buildTextPath(int8_t idx, char* out, size_t maxLen) const {
    snprintf(out, maxLen, "%s/slot_%02u.txt", SD_MEDIA_DIR, (unsigned)idx);
}

bool SDStorageProvider::isSlotValid(int8_t idx) const {
    if (idx < 0 || idx >= SD_SLOT_COUNT) return false;
    const char* m = _m.slots[idx].magic;
    return (memcmp(m, "VJPG", 4) == 0) || (memcmp(m, "VIMG", 4) == 0) ||
           (memcmp(m, "SLBX", 4) == 0);
}

int8_t SDStorageProvider::writeIndexSafe() const {
    int8_t i = _m.writeSlotIndex;
    return (i < 0 || i >= SD_SLOT_COUNT) ? 0 : i;
}

// ============================================================================
// Manifest
// ============================================================================

void SDStorageProvider::resetManifest() {
    memset(&_m, 0, sizeof(_m));
    _m.magic = SD_MANIFEST_MAGIC;
    _m.version = SD_MANIFEST_VERSION;
    _m.slotCount = SD_SLOT_COUNT;
    _m.writeSlotIndex = 0;
}

// The manifest is written atomically: write a temp file, delete the old one, rename.
// After a power loss midway (dead battery, charger pulled — a mandatory case,
// MEMORY.md §28) exactly one of the two
// files is complete at boot. Writing index.bin in place with "w" would truncate it
// first -> a power loss at that moment loses the whole message queue.
static constexpr const char* SD_MANIFEST_TMP = "/media/index.tmp";

bool SDStorageProvider::loadManifest() {
    // The previous write died after deleting the old file, before the rename -> the temp file is the right one.
    if (!_sd.fileExists(SD_MANIFEST_PATH) && _sd.fileExists(SD_MANIFEST_TMP)) {
        _sd.renameFile(SD_MANIFEST_TMP, SD_MANIFEST_PATH);
    }
    int32_t n = _sd.readFile(SD_MANIFEST_PATH, (uint8_t*)&_m, sizeof(_m));
    if (n != (int32_t)sizeof(_m)) return false;
    if (_m.magic != SD_MANIFEST_MAGIC) return false;
    if (_m.version != SD_MANIFEST_VERSION) return false;
    if (_m.slotCount != SD_SLOT_COUNT) return false;
    if (_m.writeSlotIndex < 0 || _m.writeSlotIndex >= SD_SLOT_COUNT) _m.writeSlotIndex = 0;
    return true;
}

void SDStorageProvider::saveManifest() {
    if (!_mounted) return;
    if (_sd.writeFile(SD_MANIFEST_TMP, (const uint8_t*)&_m, sizeof(_m)) != (int32_t)sizeof(_m)) {
        DLOG("[SDP] ERR: luu manifest FAIL");
        return;
    }
    _sd.deleteFile(SD_MANIFEST_PATH);
    if (!_sd.renameFile(SD_MANIFEST_TMP, SD_MANIFEST_PATH)) {
        DLOG("[SDP] ERR: doi ten manifest FAIL");
    }
}

bool SDStorageProvider::remount() {
    _activeIndex = -1;
    _writeOpen = false;
    _readIndex = -1;
    _mounted = _sd.remount();
    if (!_mounted) {
        resetManifest();
        return false;
    }
    if (!loadManifest()) {
        resetManifest();
        saveManifest();
    }
    DLOG("[SDP] remount OK unread=%u", getUnreadCount());
    return true;
}

// ============================================================================
// Init
// ============================================================================

bool SDStorageProvider::init(SemaphoreHandle_t spiMutex) {
    _mounted = _sd.init(PIN_SD_CS, spiMutex);

    if (!_mounted) {
        // NEVER return false: main.cpp hangs forever (while(1) delay) when
        // storage->init() fails. NandStorage::init() never returns false, so that
        // branch never ran — but SD.begin() returns false whenever the card is
        // missing/loose/not FAT, which is NORMAL for a removable card.
        resetManifest();
        DLOG("[SDP] khong co the -> chay rong");
        return true;
    }

    if (!loadManifest()) {
        // Corrupt manifest: create a new one but do NOT delete existing files.
        // Rebuilding the manifest by scanning files is impossible (dataSize can't
        // be told apart from audioSize) and would produce confidently wrong
        // playback — far worse than an honest "no messages".
        DLOG("[SDP] manifest moi (giu nguyen file cu)");
        resetManifest();
        saveManifest();
    } else {
        DLOG("[SDP] manifest OK unread=%u next=%d", getUnreadCount(), (int)_m.writeSlotIndex);
    }
    return true;
}

// ============================================================================
// Read
// ============================================================================

bool SDStorageProvider::openForRead(const char* identifier) {
    // MediaPlayer leaves an item open on its error branches and then calls
    // openForRead() again over it. NAND tolerates that because openSlot() only
    // assigns variables; SD has real file descriptors, so close explicitly or leak one.
    closeRead();

    if (!_mounted) return false;
    int8_t idx = parseIndex(identifier);
    if (!isSlotValid(idx)) return false;

    char path[SD_PATH_MAX];
    buildPath(idx, path, sizeof(path));

    if (!_sd.openFileForRead(path)) return false;
    if (!_sd.openAtFile(path)) {
        _sd.closeReadFile();
        return false;
    }

    // The card is removable, so a file can be SHORTER than the manifest says
    // (pulled mid-download, or edited on a computer) — damage fileExists() can't
    // see. If ignored: _readCeil > the real length -> EVERY decodeOneFrame() hits
    // MediaPlayer's "Read short" + showMessage() + delay(2000), frame after frame
    // in Task_MediaPlayer, with that toast holding the SPI mutex.
    // Returning false here sends the slot down the path NAND already has:
    // getNextUnreadIdentifier() clears the unread flag and moves on. The NAND build
    // is immune because openSlot() inspects the actual flash data, which can't
    // diverge from the slot table this way.
    if (_sd.atFileSize() < _m.slots[idx].dataSize) {
        DLOG("[SDP] slot %d cut: file %lu < dataSize %lu", (int)idx,
             (unsigned long)_sd.atFileSize(), (unsigned long)_m.slots[idx].dataSize);
        _sd.closeAtFile();
        _sd.closeReadFile();
        return false;
    }

    _readIndex = idx;
    _readCursor = 0;
    _readCeil = _m.slots[idx].dataSize;
    return true;
}

int SDStorageProvider::readData(uint8_t* buffer, uint32_t len) {
    if (_readIndex < 0 || !buffer || len == 0) return 0;
    // The ceiling is dataSize (the video part). The audio region sits AFTER
    // dataSize and only readAt() can reach it — same as NandStorage::readData().
    if (_readCursor >= _readCeil) return 0;

    uint32_t avail = _readCeil - _readCursor;
    if (len > avail) len = avail;

    size_t n = _sd.readBlock(buffer, len);
    _readCursor += (uint32_t)n;
    return (int)n;
}

void SDStorageProvider::seek(uint32_t offset) {
    if (_readIndex < 0) return;
    // Skip when already in position: MediaPlayer calls seek(20) right after
    // reading the 20 header bytes, and a real seek there only defeats stdio's readahead.
    if (offset != _readCursor) {
        _sd.seekReadFile(offset);
    }
    _readCursor = offset;
}

int SDStorageProvider::readAt(uint32_t offset, uint8_t* buffer, uint32_t len) {
    // A SEPARATE handle in SDCardManager — the sequential cursor is untouched.
    return _sd.readAtFile(offset, buffer, len);
}

void SDStorageProvider::closeRead() {
    _sd.closeAtFile();
    _sd.closeReadFile();
    _readIndex = -1;
    _readCursor = 0;
    _readCeil = 0;
}

StorageItemInfo SDStorageProvider::getItemInfo(const char* identifier) const {
    // PURE RAM, ZERO I/O: main.cpp calls this on EVERY player.update() loop
    // (i.e. every frame during video playback).
    StorageItemInfo info;
    int8_t idx = parseIndex(identifier);
    if (idx < 0) idx = _readIndex;
    if (idx < 0 || idx >= SD_SLOT_COUNT) return info;

    const SdSlotEntry& e = _m.slots[idx];
    snprintf(info.id, sizeof(info.id), "%d", (int)idx);
    info.dataSize = e.dataSize;
    info.audioSize = e.audioSize;
    info.fps = e.fps;
    info.totalFrames = e.totalFrames;
    info.maxDisplayTime = e.maxDisplayTime > 0 ? e.maxDisplayTime : 60;

    // Same magic -> type mapping as NandStorageProvider::getItemInfo()
    if (memcmp(e.magic, "VJPG", 4) == 0) {
        info.type = StorageItemType::VIDEO;
    } else if (memcmp(e.magic, "VIMG", 4) == 0 || memcmp(e.magic, "SLBX", 4) == 0) {
        info.type = (e.totalFrames > 1) ? StorageItemType::VIDEO : StorageItemType::IMAGE;
    } else {
        info.type = StorageItemType::EMPTY;
    }
    return info;
}

// ============================================================================
// Write
// ============================================================================

bool SDStorageProvider::openForWrite(const char* identifier) {
    _sd.closeWriteFile();
    _writeOpen = false;

    if (!_mounted) {
        DLOG("[SDP] ERR: openForWrite khong co the");
        return false;
    }

    int8_t idx = parseIndex(identifier);
    if (idx < 0) idx = writeIndexSafe();

    if (_m.slots[idx].unread) {
        DLOG("[SDP] ERR: slot %d unread full", (int)idx);
        return false;
    }

    char path[SD_PATH_MAX];
    buildPath(idx, path, sizeof(path));
    if (!_sd.openFileForWrite(path)) return false;

    // A 4-byte placeholder for the size prefix, patched in closeWrite(). Keeps the
    // NAND slot layout so MediaPlayer finds the magic at offset 4.
    const uint8_t zero4[4] = {0, 0, 0, 0};
    if (_sd.appendChunk(zero4, sizeof(zero4)) != sizeof(zero4)) {
        _sd.closeWriteFile();
        DLOG("[SDP] ERR: ghi prefix FAIL");
        return false;
    }

    _activeIndex = idx;
    _writeSize = 4;
    _writeOpen = true;
    _capturingHeader = true;
    memset(_hdrPeek, 0, sizeof(_hdrPeek));

    DLOG("[SDP] open write slot %d", (int)idx);
    return true;
}

size_t SDStorageProvider::writeChunk(const uint8_t* data, size_t len) {
    if (!_writeOpen || !data || len == 0) return 0;

    // Capture the 16-byte container header into RAM as it passes, instead of
    // reading it back from the file in closeWrite() like NAND does. NAND can read
    // back because it writes straight to flash; on SD a stdio buffer sits in
    // between, so reading back would need a flush first.
    if (_capturingHeader && _writeSize < 20) {
        uint32_t need = 20 - _writeSize;
        uint32_t take = (len < need) ? (uint32_t)len : need;
        memcpy(_hdrPeek + (_writeSize - 4), data, take);
        if (_writeSize + take >= 20) _capturingHeader = false;
    }

    size_t w = _sd.appendChunk(data, len);
    _writeSize += (uint32_t)w;

    // NO DLOG here (R1): this runs every 2KB throughout a download.
    // Returning w < len is the writeError signal NetworkManager relies on.
    return w;
}

void SDStorageProvider::closeWrite(uint32_t maxDisplayTime) {
    int8_t idx = _activeIndex;
    if (idx < 0 || idx >= SD_SLOT_COUNT) return;

    // 1. Patch the size prefix at offset 0.
    //    Mode "w" is O_TRUNC — truncation happens at OPEN, so overwriting 4 bytes
    //    at the start can't shorten the file.
    uint32_t rawSize = (_writeSize >= 4) ? (_writeSize - 4) : 0;
    const uint8_t sizeBytes[4] = {(uint8_t)(rawSize & 0xFF), (uint8_t)((rawSize >> 8) & 0xFF),
                                  (uint8_t)((rawSize >> 16) & 0xFF),
                                  (uint8_t)((rawSize >> 24) & 0xFF)};
    _sd.patchWriteFileAt0(sizeBytes);

    // 2. Close (flush) the file BEFORE the manifest advertises it — same reasoning
    //    as NAND writing the slot table before committing the unread bitmask to NVS.
    _sd.closeWriteFile();
    _writeOpen = false;
    _capturingHeader = false;

    // 3. Classify the container from _hdrPeek — same branch logic as
    //    NandStorageProvider::closeWrite().
    SdSlotEntry& e = _m.slots[idx];
    memset(&e, 0, sizeof(e));
    e.maxDisplayTime = maxDisplayTime;

    if (memcmp(_hdrPeek, "SLBX", 4) == 0) {
        uint8_t mediaType = _hdrPeek[5];
        uint8_t fps = _hdrPeek[10];
        uint16_t totalFrames = 1;
        memcpy(&totalFrames, _hdrPeek + 11, sizeof(totalFrames));

        e.fps = (fps > 0) ? fps : 1;
        e.dataSize = _writeSize;
        if (mediaType == 0x02 || totalFrames <= 1) {
            memcpy(e.magic, "VIMG", 4);
            e.totalFrames = 1;
        } else {
            memcpy(e.magic, "VJPG", 4);
            e.totalFrames = totalFrames;
        }
    } else if (memcmp(_hdrPeek, "SLOT", 4) == 0 || memcmp(_hdrPeek, "VJPG", 4) == 0 ||
               memcmp(_hdrPeek, "VIMG", 4) == 0) {
        uint32_t dataSize = 0;
        uint16_t fps = 0;
        uint16_t totalFrames = 0;
        memcpy(&dataSize, _hdrPeek + 4, sizeof(dataSize));
        memcpy(&fps, _hdrPeek + 8, sizeof(fps));
        memcpy(&totalFrames, _hdrPeek + 10, sizeof(totalFrames));
        if (dataSize == 0 || dataSize > _writeSize) dataSize = _writeSize;

        memcpy(e.magic, (totalFrames > 1) ? "VJPG" : "VIMG", 4);
        e.dataSize = dataSize;
        e.fps = (fps > 0) ? fps : 10;
        e.totalFrames = totalFrames;
    } else {
        // Raw JPEG, or the image-less "still message" path (0 payload bytes ->
        // _hdrPeek all zero) -> dataSize = 4 is the SENTINEL
        // MediaPlayer::playItem() looks for (type==IMAGE && dataSize<=4).
        memcpy(e.magic, "VIMG", 4);
        e.dataSize = _writeSize;
        e.fps = 1;
        e.totalFrames = 1;
    }

    // 4. Delete the slot's old caption. NAND is immune because setSlotInfo()
    //    memsets the whole SlotEntry (killing textLen); a sidecar file has no such
    //    coupling, so an old message's caption would show on a new message
    //    reusing the slot.
    char tpath[SD_PATH_MAX];
    buildTextPath(idx, tpath, sizeof(tpath));
    _sd.deleteFile(tpath);

    // 5. Mark unread + advance the queue cursor
    e.unread = 1;
    _lastWrittenIndex = idx;
    _lastWrittenSize = e.dataSize; // audio append base = dataSize
    _m.writeSlotIndex = (int8_t)((idx + 1) % SD_SLOT_COUNT);

    saveManifest();
    DLOG("[SDP] written slot %d next %d", (int)idx, (int)_m.writeSlotIndex);
}

void SDStorageProvider::discardWrite() {
    int8_t idx = _activeIndex;

    _sd.closeWriteFile();
    _writeOpen = false;
    _capturingHeader = false;

    if (idx >= 0 && idx < SD_SLOT_COUNT) {
        DLOG("[SDP] discard slot %d (bo %lu byte)", (int)idx, (unsigned long)_writeSize);

        char path[SD_PATH_MAX];
        buildPath(idx, path, sizeof(path));
        _sd.deleteFile(path);
        buildTextPath(idx, path, sizeof(path));
        _sd.deleteFile(path);

        bool wasUnread = (_m.slots[idx].unread != 0);
        memset(&_m.slots[idx], 0, sizeof(SdSlotEntry));

        // The "no image" write path already ran closeWrite() to commit a placeholder
        // BEFORE knowing whether the audio downloads -> the unread flag and
        // writeSlotIndex may ALREADY be set/advanced. Step back to this slot so the
        // next sync retries it instead of burning a new slot on every failure.
        if (wasUnread) _m.writeSlotIndex = idx;

        saveManifest();
    }

    _writeSize = 0;
}

bool SDStorageProvider::openForAppend(const char* identifier) {
    int8_t idx = parseIndex(identifier);
    if (idx < 0) idx = _lastWrittenIndex;
    if (idx < 0 || idx >= SD_SLOT_COUNT || !_mounted) return false;

    // Seek to dataSize, NOT EOF: the AUDC header then lands at the exact offset
    // AudioPlayer::loadFromStorage() probes (readAt(dataSize, ...)), including the
    // dataSize == 4 sentinel of an image-less still message.
    uint32_t at = _m.slots[idx].dataSize;

    char path[SD_PATH_MAX];
    buildPath(idx, path, sizeof(path));
    if (!_sd.openFileForAppend(path, at)) return false;

    _activeIndex = idx;
    _writeSize = at;
    _writeOpen = true;
    _capturingHeader = false;

    DLOG("[SDP] append slot %d @ %lu", (int)idx, (unsigned long)at);
    return true;
}

void SDStorageProvider::closeAppend() {
    uint32_t finalSize = _writeSize;

    // Close the handle BEFORE any guard: SD has real file descriptors; NAND
    // doesn't, so its version can early-return without leaking.
    _sd.closeWriteFile();
    _writeOpen = false;

    if (_activeIndex < 0 || _activeIndex >= SD_SLOT_COUNT) return;
    if (_activeIndex != _lastWrittenIndex) return; // not an append session

    uint32_t audioSize = (finalSize > _lastWrittenSize) ? (finalSize - _lastWrittenSize) : 0;
    _m.slots[_activeIndex].audioSize = audioSize;
    saveManifest();
    DLOG("[SDP] closeAppend slot %d audio=%lu", (int)_activeIndex, (unsigned long)audioSize);
}

void SDStorageProvider::setItemText(const char* identifier, const char* text) {
    int8_t idx = parseIndex(identifier);
    if (idx < 0 || !text || !_mounted) return;

    uint16_t len = (uint16_t)strlen(text);
    uint16_t copyLen = (len < SD_TEXT_MAX_LEN - 1) ? len : (uint16_t)(SD_TEXT_MAX_LEN - 1);
    // Truncate safely at a UTF-8 boundary — same as NandStorage::setSlotText(), so
    // captions on SD display exactly as on NAND.
    while (copyLen > 0 && (((uint8_t)text[copyLen]) & 0xC0) == 0x80) {
        copyLen--;
    }
    if (copyLen == 0) return;

    char path[SD_PATH_MAX];
    buildTextPath(idx, path, sizeof(path));
    if (_sd.writeFile(path, (const uint8_t*)text, copyLen) != (int32_t)copyLen) {
        DLOG("[SDP] ERR: ghi caption slot %d", (int)idx);
        return;
    }
    DLOG("[SDP] setItemText slot %d (%u bytes)", (int)idx, (unsigned)copyLen);
}

bool SDStorageProvider::getItemText(const char* identifier, char* outBuf, size_t maxLen) const {
    int8_t idx = parseIndex(identifier);
    if (idx < 0) idx = _readIndex;
    if (idx < 0 || idx >= SD_SLOT_COUNT || !outBuf || maxLen == 0) return false;

    char path[SD_PATH_MAX];
    buildTextPath(idx, path, sizeof(path));

    int32_t n = _sd.readFile(path, (uint8_t*)outBuf, maxLen - 1);
    if (n <= 0) {
        outBuf[0] = '\0';
        return false;
    }
    outBuf[n] = '\0';
    return true;
}

// ============================================================================
// Queue
// ============================================================================

bool SDStorageProvider::isFull() const {
    // PURE RAM: main.cpp calls this every UI tick.
    // No card -> report full so the system doesn't try to download into nothing.
    if (!_mounted) return true;
    // Equivalent to NAND's (allUnread || unread[writeSlotIndex]): if every slot is
    // unread then unread[writeSlotIndex] is necessarily 1, so the left term is redundant.
    return _m.slots[writeIndexSafe()].unread != 0;
}

bool SDStorageProvider::getNextWriteSlotIdentifier(char* outId, size_t maxLen) {
    if (!outId || maxLen == 0) return false;
    if (isFull()) {
        DLOG("[SDP] FULL");
        return false;
    }
    // Plain decimal, at most 2 chars -> fits NetworkManager's char writeSlotId[16].
    snprintf(outId, maxLen, "%d", (int)writeIndexSafe());
    return true;
}

bool SDStorageProvider::hasUnreadMessage() const {
    if (!_mounted) return false;
    for (uint8_t i = 0; i < SD_SLOT_COUNT; i++) {
        if (_m.slots[i].unread) return true;
    }
    return false;
}

uint8_t SDStorageProvider::getUnreadCount() const {
    if (!_mounted) return 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < SD_SLOT_COUNT; i++) {
        if (_m.slots[i].unread) count++;
    }
    return count;
}

bool SDStorageProvider::getNextUnreadIdentifier(char* outId, size_t maxLen) {
    if (!_mounted || !outId || maxLen == 0) return false;

    bool healed = false;
    // OLDEST unread first, scanning circularly from the write cursor.
    for (uint8_t i = 0; i < SD_SLOT_COUNT; i++) {
        int8_t idx = (int8_t)((writeIndexSafe() + i) % SD_SLOT_COUNT);
        if (!_m.slots[idx].unread) continue;

        char path[SD_PATH_MAX];
        buildPath(idx, path, sizeof(path));
        if (!isSlotValid(idx) || !_sd.fileExists(path)) {
            // Self-healing: the card is removable, so the file may have been deleted on a computer.
            DLOG("[SDP] WARN: slot %d mat file -> bo co", (int)idx);
            _m.slots[idx].unread = 0;
            healed = true;
            continue;
        }

        if (healed) saveManifest();
        snprintf(outId, maxLen, "%d", (int)idx);
        return true;
    }

    if (healed) saveManifest();
    return false;
}

void SDStorageProvider::markAsRead(const char* identifier) {
    int8_t idx = parseIndex(identifier);
    if (idx < 0 || !_mounted) return;
    if (_m.slots[idx].unread) {
        _m.slots[idx].unread = 0;
        saveManifest();
    }
    DLOG("[SDP] marked slot %d READ", (int)idx);
}

bool SDStorageProvider::getFirstValidIdentifier(char* outId, size_t maxLen) const {
    if (!_mounted || !outId || maxLen == 0) return false;
    for (uint8_t i = 0; i < SD_SLOT_COUNT; i++) {
        if (isSlotValid((int8_t)i)) {
            snprintf(outId, maxLen, "%u", (unsigned)i);
            return true;
        }
    }
    return false;
}

bool SDStorageProvider::getNextValidIdentifier(const char* currentId, char* outId,
                                               size_t maxLen) const {
    if (!_mounted || !outId || maxLen == 0) return false;
    int8_t curr = parseIndex(currentId);
    if (curr < 0) curr = 0;
    for (uint8_t i = 1; i <= SD_SLOT_COUNT; i++) {
        int8_t next = (int8_t)((curr + i) % SD_SLOT_COUNT);
        if (isSlotValid(next)) {
            snprintf(outId, maxLen, "%d", (int)next);
            return true;
        }
    }
    return false;
}

bool SDStorageProvider::formatStorage() {
    if (_mounted) {
        closeRead();
        _sd.closeWriteFile();
        _writeOpen = false;

        char path[SD_PATH_MAX];
        for (uint8_t i = 0; i < SD_SLOT_COUNT; i++) {
            buildPath((int8_t)i, path, sizeof(path));
            _sd.deleteFile(path);
            buildTextPath((int8_t)i, path, sizeof(path));
            _sd.deleteFile(path);
        }
    }

    resetManifest();
    _activeIndex = -1;
    _lastWrittenIndex = -1;
    _lastWrittenSize = 0;
    _writeSize = 0;
    saveManifest();

    // Reset last_download_ts to 0 so messages download from scratch — same as
    // NandStorageProvider::formatStorage().
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveLastDownloadTimestamp(0);
        cfg.end();
    }

    DLOG("[SDP] FULL format done");
    return true;
}
