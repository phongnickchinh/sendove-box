#include "SDStorageProvider.h"

#include "ConfigManager.h"
#include "ScreenLogger.h"
#include "config.h"

// Đủ cho "/media/slot_19.bin" (19 ký tự) + dư an toàn
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

bool SDStorageProvider::loadManifest() {
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
    if (_sd.writeFile(SD_MANIFEST_PATH, (const uint8_t*)&_m, sizeof(_m)) != (int32_t)sizeof(_m)) {
        DLOG("[SDP] ERR: luu manifest FAIL");
    }
}

// ============================================================================
// Init
// ============================================================================

bool SDStorageProvider::init(SemaphoreHandle_t spiMutex) {
    _mounted = _sd.init(PIN_SD_CS, spiMutex);

    if (!_mounted) {
        // TUYỆT ĐỐI KHÔNG trả false: main.cpp treo vĩnh viễn (while(1) delay)
        // khi storage->init() thất bại. NandStorage::init() không bao giờ trả
        // false nên nhánh đó chưa từng chạy — nhưng SD.begin() trả false mỗi khi
        // thẻ vắng/lỏng/không phải FAT, là tình huống BÌNH THƯỜNG với thẻ rút được.
        resetManifest();
        DLOG("[SDP] khong co the -> chay rong");
        return true;
    }

    if (!loadManifest()) {
        // Manifest hỏng: tạo mới nhưng KHÔNG xoá file đang có. Dựng lại manifest
        // bằng cách quét file là bất khả thi (không tách được dataSize khỏi
        // audioSize) và sẽ cho ra playback sai một cách tự tin — tệ hơn hẳn
        // trạng thái "không có tin" trung thực.
        DLOG("[SDP] manifest moi (giu nguyen file cu)");
        resetManifest();
        saveManifest();
    } else {
        DLOG("[SDP] manifest OK unread=%u next=%d", getUnreadCount(), (int)_m.writeSlotIndex);
    }
    return true;
}

// ============================================================================
// Đọc
// ============================================================================

bool SDStorageProvider::openForRead(const char* identifier) {
    // MediaPlayer để item mở lửng khi rơi vào nhánh lỗi rồi gọi lại openForRead()
    // đè lên. NAND chịu được vì openSlot() chỉ gán biến; SD có file descriptor
    // thật nên phải đóng tường minh, không thì rò FD.
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

    // Thẻ rút được nên file có thể NGẮN HƠN manifest khai (rút giữa chừng lúc
    // tải, hoặc bị sửa trên máy tính) — kiểu hỏng mà fileExists() không thấy.
    // Nếu bỏ qua: _readCeil > độ dài thật -> MỖI decodeOneFrame() đâm vào
    // "Read short" + showMessage() + delay(2000) của MediaPlayer, lặp từng frame
    // trong Task_MediaPlayer và toast đó còn giữ SPI mutex.
    // Trả false ở đây đẩy slot vào đúng đường mà NAND đã có: getNextUnreadIdentifier()
    // bỏ cờ unread rồi đi tiếp. Bản NAND miễn nhiễm vì openSlot() soi dữ liệu
    // flash thật, không thể lệch với bảng slot theo kiểu này.
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
    // Trần là dataSize (chỉ phần video). Vùng audio nằm SAU dataSize và chỉ
    // readAt() với tới được — đúng như NandStorage::readData().
    if (_readCursor >= _readCeil) return 0;

    uint32_t avail = _readCeil - _readCursor;
    if (len > avail) len = avail;

    size_t n = _sd.readBlock(buffer, len);
    _readCursor += (uint32_t)n;
    return (int)n;
}

void SDStorageProvider::seek(uint32_t offset) {
    if (_readIndex < 0) return;
    // Bỏ qua khi đã đúng vị trí: MediaPlayer gọi seek(20) ngay sau khi đọc đủ
    // 20 byte header, seek thật ở đó chỉ phá readahead của stdio.
    if (offset != _readCursor) {
        _sd.seekReadFile(offset);
    }
    _readCursor = offset;
}

int SDStorageProvider::readAt(uint32_t offset, uint8_t* buffer, uint32_t len) {
    // Handle RIÊNG trong SDCardManager — không đụng con trỏ tuần tự.
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
    // THUẦN RAM, ZERO I/O: main.cpp:183 gọi hàm này MỖI vòng player.update()
    // (tức mỗi frame lúc phát video).
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

    // Ánh xạ magic -> type giống hệt NandStorageProvider::getItemInfo()
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
// Ghi
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

    // 4 byte placeholder cho tiền tố kích thước, vá lại ở closeWrite().
    // Giữ đúng bố cục slot NAND để MediaPlayer kiểm magic tại offset 4.
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

    // Chụp 16 byte header container vào RAM ngay lúc đi qua, thay vì đọc ngược
    // từ file lúc closeWrite() như NAND. NAND đọc lại được vì ghi thẳng xuống
    // flash; trên SD còn buffer stdio xen giữa nên đọc lại sẽ phải flush trước.
    if (_capturingHeader && _writeSize < 20) {
        uint32_t need = 20 - _writeSize;
        uint32_t take = (len < need) ? (uint32_t)len : need;
        memcpy(_hdrPeek + (_writeSize - 4), data, take);
        if (_writeSize + take >= 20) _capturingHeader = false;
    }

    size_t w = _sd.appendChunk(data, len);
    _writeSize += (uint32_t)w;

    // KHÔNG DLOG ở đây (R1): hàm này chạy mỗi 2KB suốt quá trình tải.
    // Trả w < len chính là tín hiệu writeError mà NetworkManager trông vào.
    return w;
}

void SDStorageProvider::closeWrite(uint32_t maxDisplayTime) {
    int8_t idx = _activeIndex;
    if (idx < 0 || idx >= SD_SLOT_COUNT) return;

    // 1. Vá tiền tố kích thước vào offset 0.
    //    Mode "w" là O_TRUNC — cắt file xảy ra lúc OPEN, nên ghi đè 4 byte ở đầu
    //    không thể làm ngắn file.
    uint32_t rawSize = (_writeSize >= 4) ? (_writeSize - 4) : 0;
    const uint8_t sizeBytes[4] = {(uint8_t)(rawSize & 0xFF), (uint8_t)((rawSize >> 8) & 0xFF),
                                  (uint8_t)((rawSize >> 16) & 0xFF),
                                  (uint8_t)((rawSize >> 24) & 0xFF)};
    _sd.patchWriteFileAt0(sizeBytes);

    // 2. Đóng file (flush) TRƯỚC khi manifest quảng cáo nó — cùng lập luận với
    //    NAND ghi slot table trước khi commit unread bitmask vào NVS.
    _sd.closeWriteFile();
    _writeOpen = false;
    _capturingHeader = false;

    // 3. Phân loại container từ _hdrPeek — logic nhánh giống hệt
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
        // Raw JPEG, hoặc đường "tin nhắn tĩnh" không ảnh (0 byte payload ->
        // _hdrPeek toàn 0) -> dataSize = 4 chính là SENTINEL mà
        // MediaPlayer::playItem() dò (type==IMAGE && dataSize<=4).
        memcpy(e.magic, "VIMG", 4);
        e.dataSize = _writeSize;
        e.fps = 1;
        e.totalFrames = 1;
    }

    // 4. Xoá caption cũ của slot. NAND miễn nhiễm vì setSlotInfo() memset cả
    //    SlotEntry (giết luôn textLen); file sidecar không có ràng buộc đó nên
    //    caption của tin cũ sẽ hiện đè lên tin mới dùng lại slot này.
    char tpath[SD_PATH_MAX];
    buildTextPath(idx, tpath, sizeof(tpath));
    _sd.deleteFile(tpath);

    // 5. Đánh dấu chưa đọc + dịch con trỏ hàng chờ
    e.unread = 1;
    _lastWrittenIndex = idx;
    _lastWrittenSize = e.dataSize; // Gốc append audio = dataSize
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

        // Đường ghi "không ảnh" đã closeWrite() commit placeholder TRƯỚC rồi mới
        // biết audio có tải được không -> cờ unread và writeSlotIndex có thể ĐÃ
        // bị set/dịch. Lùi lại đúng slot này để lần sync sau retry vào nó, không
        // đốt thêm một slot mới mỗi lần fail.
        if (wasUnread) _m.writeSlotIndex = idx;

        saveManifest();
    }

    _writeSize = 0;
}

bool SDStorageProvider::openForAppend(const char* identifier) {
    int8_t idx = parseIndex(identifier);
    if (idx < 0) idx = _lastWrittenIndex;
    if (idx < 0 || idx >= SD_SLOT_COUNT || !_mounted) return false;

    // Seek tới dataSize chứ KHÔNG phải EOF: như vậy header AUDC rơi đúng offset
    // mà AudioPlayer::loadFromStorage() dò (readAt(dataSize, ...)), kể cả trường
    // hợp sentinel dataSize == 4 của tin nhắn tĩnh không ảnh.
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

    // Đóng handle TRƯỚC mọi guard: SD có file descriptor thật, NAND thì không
    // nên bản của nó early-return được mà không rò gì.
    _sd.closeWriteFile();
    _writeOpen = false;

    if (_activeIndex < 0 || _activeIndex >= SD_SLOT_COUNT) return;
    if (_activeIndex != _lastWrittenIndex) return; // Không phải phiên append

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
    // Cắt bớt an toàn tại ranh giới UTF-8 — giống hệt NandStorage::setSlotText()
    // để caption trên SD hiển thị y như trên NAND.
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
// Hàng chờ
// ============================================================================

bool SDStorageProvider::isFull() const {
    // THUẦN RAM: main.cpp:234 gọi mỗi tick UI.
    // Không có thẻ -> báo đầy để hệ thống không cố tải về hư không.
    if (!_mounted) return true;
    // Tương đương NAND (allUnread || unread[writeSlotIndex]): nếu mọi slot đều
    // chưa đọc thì unread[writeSlotIndex] tất yếu = 1, nên vế trái là thừa.
    return _m.slots[writeIndexSafe()].unread != 0;
}

bool SDStorageProvider::getNextWriteSlotIdentifier(char* outId, size_t maxLen) {
    if (!outId || maxLen == 0) return false;
    if (isFull()) {
        DLOG("[SDP] FULL");
        return false;
    }
    // Thập phân trần, tối đa 2 ký tự -> vừa char writeSlotId[16] của NetworkManager.
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
    // Duyệt tin chưa đọc CŨ NHẤT trước, bắt đầu từ con trỏ ghi theo vòng tròn.
    for (uint8_t i = 0; i < SD_SLOT_COUNT; i++) {
        int8_t idx = (int8_t)((writeIndexSafe() + i) % SD_SLOT_COUNT);
        if (!_m.slots[idx].unread) continue;

        char path[SD_PATH_MAX];
        buildPath(idx, path, sizeof(path));
        if (!isSlotValid(idx) || !_sd.fileExists(path)) {
            // Tự chữa lành: thẻ rút được nên file có thể bị xoá trên máy tính.
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

    // Reset mốc last_download_ts về 0 để sẵn sàng kéo lại tin nhắn từ đầu —
    // giống hệt NandStorageProvider::formatStorage().
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.saveLastDownloadTimestamp(0);
        cfg.end();
    }

    DLOG("[SDP] FULL format done");
    return true;
}
