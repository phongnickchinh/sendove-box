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

    // NVS có thể còn trạng thái của bố cục 5 slot cũ: bit 3-4 và write_idx 3/4 không
    // còn slot tương ứng. Để nguyên thì isFull() kẹt true vĩnh viễn.
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
    // DLOG saved every time -> dropped
}

bool NandStorageProvider::init(SemaphoreHandle_t spiMutex) {
    bool ok = _nand.init(spiMutex);
    loadNvsState();

    // Doi magic NSLT -> NSL2 lam bang slot tren flash bi xoa sach, nhung
    // unread_mask trong NVS thi khong. Neu khong don, lan boot dau sau khi nap
    // firmware moi hop bao "co tin chua doc" tro toi slot rong -> tin nhan trang.
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
        slot = _writeSlotIndex; // Mặc định dùng cur_point
    }

    // Kiểm tra nếu slot tại cur_point đang chứa tin nhắn chưa đọc -> từ chối ghi (Bộ nhớ đầy)
    if (_unreadBitmask & (1 << slot)) {
        DLOG("[NANDP] ERR: slot %d unread full", slot);
        return false;
    }

    _activeSlot = slot;
    _writeOffset = 4;
    _slotCapacity = slotSpan(slot);

    // Erase-as-you-write: chỉ erase 1 block 64KB đầu (~150-2000ms) thay vì nguyên
    // slot ~5.3MB (15-25s block đồng bộ). Erase nguyên slot khiến socket HTTP đã
    // mở đứng im quá lâu -> TCP Zero-Window -> CDN backoff -> download timeout
    // giữa chừng (xác nhận qua log thực tế + review độc lập, không phải suy đoán).
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

    // Erase thêm block 64KB kế tiếp khi con trỏ ghi sắp chạm vùng chưa erase. BẤT
    // BIẾN bắt buộc: luôn erase bắt đầu từ _erasedUpToAddr, KHÔNG BAO GIỜ từ
    // startAddr — eraseRange() với địa chỉ không align 4096 sẽ tự lùi về đầu
    // sector chứa nó, dùng sai mốc có thể xoá đè lên dữ liệu vừa ghi trước đó.
    uint32_t slotEndAddr = slotStartAddr + _slotCapacity;
    while (_erasedUpToAddr < endAddr && _erasedUpToAddr < slotEndAddr) {
        uint32_t toErase = 65536U;
        if (_erasedUpToAddr + toErase > slotEndAddr) {
            toErase = slotEndAddr - _erasedUpToAddr;
        }
        if (!_nand.eraseRange(_erasedUpToAddr, toErase)) {
            DLOG("[NANDP] ERR: erase-ahead FAILED @ %lu", (unsigned long)_erasedUpToAddr);
            return 0;   // NetworkManager thay written < len -> writeError -> discardWrite()
        }
        _erasedUpToAddr += toErase;
    }

    // Truoc day goi writeRaw() roi `return len` VO DIEU KIEN: mot lan lay SPI mutex
    // that bai (timeout 1000ms khi Task_MediaPlayer priority 3 dang render standby)
    // se tao ra file tai "thanh cong" nhung thung lo du lieu. Gio bao loi that.
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

    // Ghi kích thước dữ liệu (4 bytes) vào offset 0
    uint32_t rawJpegSize = (_writeOffset >= 4) ? (_writeOffset - 4) : 0;
    if (!_nand.writeRaw(slotStartAddr, (const uint8_t*)&rawJpegSize, 4)) {
        DLOG("[NANDP] ERR: size header write FAILED slot %d", _activeSlot);
    }

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
            _nand.setSlotInfo(_activeSlot, "VIMG", _writeOffset, finalFps, totalFrames, maxDisplayTime);
            // Dropped Detected SLBX image
        } else {
            if (totalFrames == 0) totalFrames = 1;
            _nand.setSlotInfo(_activeSlot, "VJPG", _writeOffset, finalFps, totalFrames, maxDisplayTime);
            // Dropped Detected SLBX video
        }
    } else if (memcmp(header, "SLOT", 4) == 0 || memcmp(header, "VJPG", 4) == 0 || memcmp(header, "VIMG", 4) == 0) {
        uint32_t dataSize = *(uint32_t*)(header + 4);
        uint16_t fps = *(uint16_t*)(header + 8);
        uint16_t totalFrames = *(uint16_t*)(header + 10);
        if (dataSize == 0 || dataSize > _writeOffset) dataSize = _writeOffset;
        const char* magic = (totalFrames > 1) ? "VJPG" : "VIMG";
        _nand.setSlotInfo(_activeSlot, magic, dataSize, (fps > 0) ? fps : 10, totalFrames, maxDisplayTime);
        // Dropped Detected pre-encoded
    } else {
        _nand.setSlotInfo(_activeSlot, "VIMG", _writeOffset, 1, 1, maxDisplayTime);
        // Dropped Raw JPEG
    }

    // 1. Đánh dấu bit thứ cur_point là chưa đọc (1)
    _unreadBitmask |= (1 << _activeSlot);

    // 2. Lưu lại slot và offset hiện tại để dùng cho openForAppend (ghi audio nối tiếp)
    _lastWrittenSlot   = _activeSlot;
    _lastWrittenOffset = _writeOffset;

    // 3. Dịch tiến con trỏ hàng chờ sang Slot tiếp theo
    int8_t writtenSlot = _activeSlot;
    _writeSlotIndex = (_activeSlot + 1) % NAND_SLOT_COUNT;
    _slotCapacity = 0;

    // Ghi slot table vào NAND TRƯỚC khi commit unread bitmask vào NVS.
    // Nếu bị reset giữa chừng, NAND sẽ có data hợp lệ trước khi NVS biết slot là unread.
    _nand.writeSlotTable();
    saveNvsState();
    DLOG("[NANDP] written slot %d next %d", writtenSlot, _writeSlotIndex);
}

void NandStorageProvider::discardWrite() {
    if (_activeSlot >= 0 && _activeSlot < NAND_SLOT_COUNT) {
        DLOG("[NANDP] discardWrite slot %d (bo %lu byte do)", _activeSlot, (unsigned long)_writeOffset);
        // Vung vat ly da bi erase do dang luc openForWrite(); phai xoa magic trong
        // bang slot de isSlotValid()/findFirstValidSlot() khong nhat nham du lieu
        // rac nay la mot item hop le.
        const char emptyMagic[4] = {0, 0, 0, 0};
        _nand.setSlotInfo(_activeSlot, emptyMagic, 0, 0, 0, 0);
        _nand.writeSlotTable();

        // Doi voi duong ghi "khong anh" (chi audio/text), closeWrite() da chay
        // TRUOC de commit placeholder (can cho openForAppend() tinh offset dung)
        // roi moi biet tai audio/text co thanh cong khong -> unread bit va
        // _writeSlotIndex co the DA bi set/dich truoc khi discardWrite() duoc goi.
        // Xoa bit + lui _writeSlotIndex ve dung slot nay de lan sync sau retry
        // dung slot vua fail, khong dot them 1 slot moi. Vo hai neu bit chua tung set.
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
        // Fallback về slot vừa ghi
        slot = _lastWrittenSlot;
    }
    if (slot < 0) return false;

    // Không erase lại — tiếp tục ghi từ vị trí cuối của lần ghi video.
    // CHỈ đặt _activeSlot: _writeSlotIndex là con trỏ hàng chờ, closeWrite đã dịch
    // nó sang slot kế. Kéo nó lùi lại đây làm isFull() thấy slot chưa đọc -> báo đầy.
    _activeSlot   = slot;
    _writeOffset  = _lastWrittenOffset;
    _slotCapacity = slotSpan(slot);

    // Tự tính lại _erasedUpToAddr từ _writeOffset (làm tròn lên block 64KB kế
    // tiếp) thay vì dựa vào giá trị còn sống sót từ phiên openForWrite() trước
    // đó — tự chữa lành, đúng bất kể thứ tự gọi thực tế trong call-chain.
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
    if (_activeSlot != _lastWrittenSlot) return;   // không phải phiên append

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
    _activeSlot = 0;
    _lastWrittenSlot = -1;
    _lastWrittenOffset = 0;
    _writeOffset = 0;
    _slotCapacity = 0;
    _erasedUpToAddr = 0;
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
