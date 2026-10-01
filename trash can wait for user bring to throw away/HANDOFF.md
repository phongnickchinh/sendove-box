# HANDOFF — bàn giao context (2026-08-31)

Tài liệu này để agent session mới đọc trước khi làm gì. Session cũ chạy trong worktree
`.claude/worktrees/sendlove-fe-design-screens-4364f5` nên bị khoá cwd; session mới phải chạy
thẳng ở `P:\coddd\sendove-box`.

---

## 0. Hiện trạng git

```
main = 354aee4  (ahead 1 so với origin/main — CHƯA PUSH, đúng chủ ý)
  354aee4 feat(backend): box config endpoint, sender message access, lock down rules
  6518836 Merge branch 'claude/video-convert-audio-playback-a569d8'
```

Branch còn lại: `main`, `ff`, `phong-tuyet-vong`, `pjhonggg` — đều của user, không đụng.
Mọi nhánh `claude/*` và nhánh backup đã xoá sạch (tất cả đều đã merged, xác minh trước khi xoá).

**Ràng buộc user đặt ra, còn hiệu lực:** không push, không tạo PR. Chỉ làm local.

**Ba file bí mật không bao giờ được commit** (đã có trong .gitignore):
`sendlove_firmware/include/config_secrets.h`, `sendlove_web/.env`,
`sendlove_backend/serviceAccountKey.json`.

---

## 1. Lỗi user báo

### 1.1 Hộp không phát được audio của tin nhắn mới
Gửi tin nhắn có tiếng nói, hộp nhận nhưng câm. Video/ảnh vẫn chạy bình thường.

### 1.2 Build ở hai thư mục ra firmware khác nhau
Worktree build ra Flash **73.1%** → hộp chạy đúng.
Thư mục chính build ra Flash **72.9%** → hộp lỗi.
User hỏi: vì sao, và làm sao lấy bản 73.1% về thư mục chính.

### 1.3 Tiếng bị rè
Audio phát ra nghe rè, chói. Chưa xử lý.

### 1.4 Nhầm lẫn về worktree
User tưởng worktree `claude/video-convert-audio-playback-a569d8` biến mất và code bị mất.

---

## 2. Đã điều tra ra gì

### 2.1 Nguyên nhân 73.1% vs 72.9% — ĐÃ TÌM RA, KHÁC VỚI GIẢ THUYẾT BAN ĐẦU

Hai giả thuyết đầu đều SAI, đừng đi lại:

- ~~"Build cũ bị stale"~~ — SAI. Build sạch lại cho ra md5 y hệt bản cũ.
- ~~"Do khác code — bản rework Wi-Fi/NTP là thủ phạm"~~ — SAI. Đã hoàn nguyên
  `include/lib/src` về `5fdc219`, build lại vẫn ra **72.9%**, không phải 73.1%.
  Sau đó `diff -rq` xác nhận source hai cây **byte-identical**.

**Nguyên nhân thật: khác phiên bản thư viện, không phải khác code.**

```
worktree:      LovyanGFX 1.2.28   → 73.1%
thư mục chính: LovyanGFX 1.2.26   → 72.9%
```

`platformio.ini` ghi `lovyan03/LovyanGFX@^1.1.12` — dải version mở, nên mỗi cây `.pio/libdeps/`
resolve ra bản khác nhau tuỳ thời điểm cài. LovyanGFX là driver màn hình ST7789.

**Chưa kiểm chứng:** liệu 1.2.26 → 1.2.28 có thật sự sửa lỗi audio không, hay 73.1% chỉ là
trùng hợp. Việc cần làm: nâng LovyanGFX ở thư mục chính lên 1.2.28, build, xác nhận đạt
73.1% và md5 = `7671825832ef2e457e14a5deec810e6a`, rồi nạp và thử tin nhắn MỚI.

Lưu ý phụ: `.pio/libdeps/` ở thư mục chính còn thư mục thừa `tft_test/` và `wokwi/` chứa
TFT_eSPI — rác từ env cũ, không được link vào build, vô hại.

### 2.2 Bản rework Wi-Fi/NTP — đang nằm trên main, CHƯA được minh oan

Commit `a5970fc` + `6518836` gộp `ensureConnected()` và `triggerNtpSync()` vào trong
`triggerWakeupSync()`, xoá 3 chỗ gọi tường minh trong `main.cpp` (chu kỳ 10s, sau wakeup,
và `setup()`). Đồng thời đổi `isFirebaseSyncing()` → `isSyncing()`.

Rủi ro còn bỏ ngỏ: nếu `wakeupSyncTaskWorker` bắt đầu tải trước khi `ensureConnected(timeoutMs)`
thành công thì phần append audio fail, `closeAppend()` không chạy → hộp câm. Và `isSyncing()`
rộng hơn hẳn (`_isSyncing || _isFirebaseSyncing || _isNtpSyncing || _isDownloadingMedia`) —
chỉ cần một cờ kẹt là sync 10s ngừng chạy vĩnh viễn.

Đây là **giả thuyết chưa bác bỏ**, không phải kết luận. Nếu nâng LovyanGFX không giải quyết
được thì quay lại chỗ này, đọc kỹ thứ tự trong `wakeupSyncTaskWorker`.

### 2.3 Giả thuyết thứ hai vẫn còn treo: dữ liệu flash cũ

Layout slot NOR đã đổi `NAND_SLOT_COUNT` 5 → 3, địa chỉ từ
`{0x010000,0x340000,0x670000,0x9A0000,0xCD0000}` → `{0x010000,0x560000,0xAB0000}`.
**Nạp lại firmware KHÔNG xoá data flash.** Tin nhắn cũ do firmware đời trước ghi vào sẽ đọc
sai và phát câm.

Cách phân biệt: nạp xong, gửi một tin **hoàn toàn mới**, xem log có
`[NET] Audio DL OK: <n> bytes` không. Có → đường tải chạy đúng, lỗi nằm ở data cũ.
Không → lỗi ở đường tải.

### 2.4 Worktree không mất code

`git worktree list` in ra SHA của branch ref, nhưng file `.git/worktrees/<tên>/HEAD` mới là
commit thực sự được checkout. Hai giá trị này có thể lệch nhau — branch ref bị dời mà HEAD của
worktree không đi theo. Đó là lý do `git diff main claude/...` trả về rỗng trong khi hai thư
mục build ra firmware khác nhau. Không có code nào bị mất.

---

## 3. Đã làm gì

1. Build sạch firmware ở thư mục chính → bác bỏ giả thuyết stale build.
2. Đối chiếu toàn bộ hai cây: backend giống hệt, web chỉ khác `Login.jsx`, firmware source
   giống hệt sau khi hoàn nguyên.
3. Hoàn nguyên `sendlove_firmware/{include,lib,src}` về `5fdc219` → build ra 72.9% → bác bỏ
   giả thuyết "do code". Sau đó **đã trả lại đúng HEAD**, hiện `git diff HEAD` trên firmware rỗng.
4. Phát hiện chênh lệch LovyanGFX 1.2.26 vs 1.2.28.
5. Xoá 6 nhánh `claude/*` + 3 nhánh backup. Tất cả xác minh merged trước khi xoá.
6. Cứu code backend từ worktree `backend-design-deploy-8b8182` (xem mục 4) và cherry-pick vào main.
7. Gỡ 2 worktree.

---

## 4. Commit 354aee4 — code backend vừa cứu về

17 file (8 `src/*.ts` gốc + 7 `lib/*.js` build ra + 2 file rules), +148/−36. Ba nhóm:

**a. Endpoint mới `PUT /boxes/:boxId/config`** — điều khiển `led_state` /
`display_brightness` / `playback_volume`. Thêm `config_flag` vào `BoxFlags`, chạy đúng khuôn
mẫu `a_flag`/`p_flag`/`ota_flag` sẵn có: service bật cờ, ESP32 poll thấy cờ thì nhận config
rồi backend tự tắt cờ.

> **QUAN TRỌNG:** firmware hiện tại CHƯA đọc `config_flag`. Tính năng này nằm im cho tới khi
> bổ sung phía ESP32. Đây là việc còn dang dở, không phải bug.

**b. Sửa phân quyền** — `requireRole()` giờ nhận mảng. Trước đó `GET /messages` chỉ cho
`receiver`, nghĩa là **người gửi không xem lại được tin mình đã gửi**. Có tác dụng ngay.

**c. Vá lỗ hổng bảo mật** — `database.rules.json` trước đó là
`".read": "auth != null || true"`. Vế `|| true` khiến biểu thức luôn đúng → Realtime Database
**mở toang cho bất kỳ ai đọc/ghi, không cần đăng nhập**. Đã đổi thành `false`, mọi truy cập đi
qua Admin SDK. `storage.rules` siết tương tự (signed URL không đi qua rules nên không ảnh hưởng).

---

## 5. Việc cần làm tiếp, theo thứ tự ưu tiên

### P0 — Xác minh giả thuyết LovyanGFX

Nâng LovyanGFX ở `P:\coddd\sendove-box\sendlove_firmware` lên 1.2.28, build sạch.

Tiêu chí đạt: Flash **73.1%**, md5 `7671825832ef2e457e14a5deec810e6a`.
Nếu md5 khớp → đã tái tạo chính xác firmware chạy được. Nếu không khớp → còn biến số khác,
đừng bàn giao vội.

Cân nhắc ghim version trong `platformio.ini` (`@1.2.28` thay vì `@^1.1.12`) để build tái lập được.

### P0 — Nạp và thử thực tế

Nạp COM8 (giữ BOOT → nhấn thả RESET → thả BOOT). Gửi tin nhắn **hoàn toàn mới**. Kiểm tra:
- log có `[NET] Audio DL OK: <n> bytes` không
- audio và video có kết thúc cùng lúc không
- còn in `[PLAY] late resync: -NNNms` không

### P1 — Sửa tiếng rè

Trong `sendlove_web/src/utils/mediaEncoder.js`, hàm `extractAudioFromVideo`: thêm
`BiquadFilterNode` lowpass ~3.4 kHz trước `offlineCtx.destination`, và nâng 8 kHz → 16 kHz.
`config.h` đã có sẵn `I2S_SAMPLE_RATE = 16000` (đang comment), `AUDIO_MAX_PCM_BYTES = 600000`
đã đủ chỗ cho 16 kHz.

### P1 — Quyết định số phận 11 file web chưa commit

Bản redesign FE + chuyển đổi audio, đang dirty trên main:
`index.html`, `App.jsx`, `AuthRoute.jsx`, `EncodingProgress.jsx`, `ImageInput.jsx`,
`VideoInput.jsx`, `VoiceInput.jsx`, `SenderUI.jsx`, `Login.jsx`, `mediaEncoder.js`,
xoá `VoiceInput.css`; untracked: `src/components/ui/`, `src/styles/`,
`sendlove-box-style-guide.md`.

User đã được hỏi có commit không nhưng **chưa trả lời**. Hỏi lại trước khi đụng.

### P2 — Firmware đọc `config_flag`

Hoàn thiện nửa còn lại của tính năng ở mục 4a.

### P2 — 17/22 màn hình design chưa chuyển

Còn `06-history` … `22-portal-theme`. Riêng `15`–`18` và `22` thuộc captive portal của
firmware, không phải React app.

### P3 — Dọn vặt

- `.gitignore` chưa có `firebase-debug.log` (đang untracked ở root).
- Thư mục rỗng `.claude/worktrees/focused-mahavira-737b5e` kẹt do khoá file, git đã gỡ đăng ký.
- Worktree `angry-mclean-3ec0b7` sạch, detached, gỡ được:
  `git worktree remove .claude/worktrees/angry-mclean-3ec0b7`

---

## 6. Kiến thức kỹ thuật cần biết trước

- ESP32-C3 RISC-V 160 MHz, single-core. Env PlatformIO: `esp32-c3-devkitm-1`.
- Màn ST7789 240×240 qua SPI2_HOST, `cfg.freq_write = 40000000` — 40 MHz vì chân 4/5/6 không
  vào được IOMUX của FSPI, phải qua GPIO matrix.
- I2S → MAX98357A chạy theo clock phần cứng, **không bao giờ chờ** → lệch A/V chỉ trôi một chiều.
- Container `SLBX`, header audio `AUDC`, bảng slot `NSL2` nằm trên SPI NOR flash ngoài.
- File firmware dùng **CRLF**. Mọi lệnh diff phải có `--strip-trailing-cr`, mọi lần sửa phải
  giữ nguyên CRLF. `git status` báo "70 file thay đổi" có thể chỉ là nhiễu CRLF — luôn kiểm
  bằng `git diff --stat` trước khi tin.
- PlatformIO CLI: `C:\Users\phamp\.platformio\penv\Scripts\platformio.exe`
- Mỗi worktree có `.pio/` riêng → libdeps riêng → có thể ra binary khác nhau dù code giống hệt.
  Đây chính là cái bẫy đã tốn cả buổi hôm nay.

---

## 7. Bản sao lưu

Trạng thái chưa commit của thư mục chính trước khi hoàn nguyên firmware đã lưu tại:

```
C:\Users\phamp\AppData\Local\Temp\claude\P--coddd-sendove-box--claude-worktrees-sendlove-fe-design-screens-4364f5\7386f47e-3a51-41f0-9668-12810b3bd861\scratchpad\backup\
```

gồm `central-uncommitted-tracked.patch`, `main.cpp.central-before-revert`,
`platformio.ini.central-before-revert`.

Đây là thư mục tạm theo session, **sẽ mất khi dọn dẹp**. Nếu còn cần thì copy ra chỗ khác ngay.
