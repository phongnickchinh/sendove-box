# BÁO CÁO REVIEW TOÀN DIỆN CODEBASE SENDLOVE BOX FIRMWARE

**Phiên bản phân tích**: Firmware v2.1.0 (ESP32-C3-DevKitM-1, Arduino Framework / FreeRTOS)  
**Mục tiêu**: Phát hiện toàn bộ lỗi tiềm ẩn về **Bộ nhớ (Memory)**, **Xung đột & Đa nhiệm (Concurrency & Deadlocks)**, **Dư thừa & Mã chết (Redundancy & Dead Code)**, và **Bảo mật (Security & Safety)**.

---

## 1. TỔNG QUAN KIẾN TRÚC & TÀI NGUYÊN HỆ THỐNG

- **MCU Target**: ESP32-C3 RISC-V Single-core 160MHz, 320KB SRAM, 4MB SPI Flash.
- **Mô hình tác vụ FreeRTOS**:
  - `Task_MediaPlayer` (Priority 3, Stack 8KB): Giải mã video MJPEG / Raw SLBX, phát audio PCM I2S, cập nhật Standby UI 1s/lần.
  - `Task_UIController` (Priority 5, Stack 8KB): Đọc cảm ứng TTP223 (100Hz), quản lý chuyển trạng thái và quản lý chu kỳ ngủ Light-Sleep.
  - `Task_NetworkController` (Priority 2, Stack 8KB): Phục vụ WebServer OTA và Captive Portal Wi-Fi.
  - `FbSync` (Tác vụ nền động, Priority 2, Stack 12KB): Đồng bộ REST API Firebase, tải dữ liệu media stream.
  - `NtpSync` (Tác vụ nền động, Priority 1, Stack 4KB): Cập nhật thời gian thực NTP.
- **Phần cứng chia sẻ**: Màn hình ST7789 240x240 (CS = -1) và W25Q128 16MB NAND Flash dùng chung bus Hardware SPI2 (`SPI_MODE3`).

---

## 2. VẤN ĐỀ BỘ NHỚ (MEMORY ISSUES)

### 🔴 2.1. Nguy cơ tràn RAM (OOM) và phân mảnh Heap do TLS Handshake song song
- **Vị trí**: [`NetworkManager.cpp:L282`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L282), [`NetworkManager.cpp:L329`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L329), [`MediaPlayer.cpp:L61`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/MediaPlayer/MediaPlayer.cpp#L61).
- **Phân tích kỹ thuật**:
  - Task `FbSync` được tạo động với stack **12KB** (`12288 bytes`).
  - Trong mỗi chu trình đồng bộ, `WiFiClientSecure` khởi tạo TLS context (mbedTLS) yêu cầu từ **35KB đến 45KB Heap liên tục**.
  - Khi `MediaPlayer` phát video/ảnh, con trỏ `_jpegBuffer` cấp phát động thêm **48KB** (`malloc(48 * 1024)`).
  - Tổng RAM khả dụng thực tế của ESP32-C3 sau khi nạp Wi-Fi stack và các task cố định chỉ còn khoảng **140KB - 160KB Heap**.
  - Nếu `FbSync` chạy nền đúng lúc người dùng chạm mở video hoặc khi heap bị phân mảnh, lệnh `malloc(48KB)` hoặc TLS Handshake sẽ trả về `NULL`, gây lỗi tải dữ liệu (HTTP `-1`) hoặc crash hệ thống.
- **Khuyến nghị**: Tái sử dụng một TLS Client duy nhất (Keep-Alive) hoặc cấu hình giảm mbedTLS RX/TX buffer size (ví dụ `client.setBufferSizes(2048, 1024)`) để tiết kiệm ~30KB RAM.

### 🟡 2.2. Treo vĩnh viễn cờ `_isNtpSyncing` khi `xTaskCreate` thất bại
- **Vị trí**: [`NetworkManager.cpp:L76-L83`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L76-L83).
- **Phân tích**:
  ```cpp
  void NetworkManager::triggerNtpSync() {
      if (WiFi.status() != WL_CONNECTED) return;
      if (_isNtpSyncing) return;
      ...
      _isNtpSyncing = true;
      xTaskCreate(ntpTaskWorker, "NtpSync", 4096, this, 1, nullptr);
  }
  ```
  Biến `_isNtpSyncing` được gán `true` trước khi gọi `xTaskCreate`. Nếu RAM heap không đủ cấp 4KB stack, `xTaskCreate` trả về `pdFAIL` nhưng cờ `_isNtpSyncing` vẫn giữ nguyên `true`. Khi đó, toàn bộ các lần gọi `triggerNtpSync()` về sau sẽ bị chặn lại vĩnh viễn, làm mất khả năng tự đồng bộ giờ.
- **Khuyến nghị**:
  ```cpp
  BaseType_t res = xTaskCreate(ntpTaskWorker, "NtpSync", 4096, this, 1, nullptr);
  if (res != pdPASS) {
      _isNtpSyncing = false;
      DLOG("[NET] NTP task alloc FAIL");
  }
  ```

### 🟡 2.3. Cấp phát heap động trong `std::vector<JsonObject>` và `WidgetConfig`
- **Vị trí**: [`NetworkManager.cpp:L530-L545`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L530-L545), [`LayoutEngine.h:L19-L30`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/LayoutEngine/LayoutEngine.h#L19-L30).
- **Phân tích**:
  - `std::vector<JsonObject> msgList` cấp phát và reallocate động trên heap theo số lượng tin nhắn trả về từ Cloud.
  - Cấu trúc `WidgetConfig` chứa 4 biến `String` (`align`, `font`, `format`, `src`). Khi nạp widget động, việc tạo và sao chép `String` nhiều lần tạo ra các vùng nhớ rác nhỏ làm phân mảnh heap.
- **Khuyến nghị**: Chuyển các trường cố định của `WidgetConfig` sang `enum` hoặc mảng `char[16]` tĩnh.

---

## 3. XUNG ĐỘT & ĐA NHIỆM (CONCURRENCY, RACE CONDITIONS & DEADLOCKS)

### 🔴 3.1. Nguy cơ Deadlock tiềm ẩn khi `DLOG` gọi bên trong khối giữ `_spiMutex`
- **Vị trí**: [`MediaPlayer.cpp:L349-L365`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/MediaPlayer/MediaPlayer.cpp#L349-L365), [`ScreenLogger.cpp:L41`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/ScreenLogger/ScreenLogger.cpp#L41).
- **Phân tích kỹ thuật**:
  - Mutex `_spiMutex` được khởi tạo bằng `xSemaphoreCreateMutex()` (Non-recursive Mutex) trong [`main.cpp:L326`](file:///P:/coddd/sendove-box/sendlove_firmware/src/main.cpp#L326).
  - Trong `MediaPlayer::decodeOneFrame()`:
    1. Task `Task_MediaPlayer` đã chiếm `_spiMutex` bằng `_display->acquireSPI()`.
    2. Nếu `_jpeg.openRAM()` thất bại, code gọi `DLOG("[PLAY] ERR: openRAM")`.
    3. `DLOG` chuyển tiếp vào `ScreenLogger::render()`, hàm này lại gọi `_display->acquireSPI(50)`.
    4. Vì `_spiMutex` là Mutex thông thường (không đệ quy), việc cùng một task cố gắng lấy lại mutex của chính mình sẽ khiến FreeRTOS **treo task đó (Deadlock)** trong 50ms cho đến khi hết timeout.
- **Khuyến nghị**: Khởi tạo `_spiMutex` dạng Recursive Mutex (`xSemaphoreCreateRecursiveMutex()`) và dùng `xSemaphoreTakeRecursive` / `xSemaphoreGiveRecursive`.

### 🔴 3.2. Thiếu NOP Hack trong `SDCardManager` gây nhiễu và xé màn hình ST7789
- **Vị trí**: [`SDCardManager.cpp:L167-L181`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/SDCardManager/SDCardManager.cpp#L167-L181) so với [`NandStorage.cpp:L287-L302`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NandStorage/NandStorage.cpp#L287-L302).
- **Phân tích**:
  - Màn hình ST7789 không có chân CS (`PIN_TFT_CS = -1`) nên phần cứng LCD luôn nhận tất cả xung nhịp SCK trên bus SPI2.
  - Driver `NandStorage` đã áp dụng NOP Hack (`PIN_TFT_DC = LOW`, đẩy `0x00`, rồi kéo `HIGH`) để màn hình bỏ qua các luồng dữ liệu của Flash.
  - Ngược lại, `SDCardManager::acquireSPI()` **hoàn toàn không có NOP Hack này**. Khi kích hoạt `#define ACTIVE_STORAGE_TYPE STORAGE_TYPE_SD`, việc đọc/ghi thẻ SD sẽ đẩy dữ liệu thô vào LCD, làm màn hình bị chớp nháy và nhiễu hạt mè nghiêm trọng.
- **Khuyến nghị**: Bổ sung đoạn sequence NOP Hack chuẩn vào `SDCardManager::acquireSPI()`.

### 🟡 3.3. Race Condition giữa `Task_UIController` (P5) và `Task_MediaPlayer` (P3)
- **Vị trí**: [`main.cpp:L52`](file:///P:/coddd/sendove-box/sendlove_firmware/src/main.cpp#L52), [`main.cpp:L99`](file:///P:/coddd/sendove-box/sendlove_firmware/src/main.cpp#L99), [`main.cpp:L234`](file:///P:/coddd/sendove-box/sendlove_firmware/src/main.cpp#L234).
- **Phân tích**:
  Biến `currentAppState` và `lastUserActivity` là các biến toàn cục không được đồng bộ bằng Mutex hay `std::atomic`. Khi `Task_UIController` (độ ưu tiên 5) ngắt giữa chừng quá trình `Task_MediaPlayer` (độ ưu tiên 3) đang cập nhật trạng thái hoặc dừng player, hệ thống có thể bị rơi vào trạng thái bất định (ví dụ: cờ `currentAppState` là `STATE_VIDEO` nhưng `player.stop()` đã bị gọi).
- **Khuyến nghị**: Dùng `std::atomic<AppState>` cho `currentAppState` hoặc quản lý toàn bộ việc đổi state thông qua Event Queue.

### 🟡 3.4. Lỗi Dropped Sample và lệch pha âm thanh trong `AudioPlayer::fillChunk()`
- **Vị trí**: [`AudioPlayer.cpp:L138-L146`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/MediaPlayer/AudioPlayer.cpp#L138-L146).
- **Phân tích**:
  - `i2s_write` được gọi trong vòng lặp từng sample (lên tới 800 lần/chunk) với `timeout = 0`.
  - Nếu DMA buffer đầy, `written = 0` (sample bị drop), nhưng biến đếm `_audioCursor` ở cuối hàm vẫn tăng trọn vẹn `bytesRead`. Điều này làm lệch vị trí đọc trong stream âm thanh, gây hiện tượng lách tách, giật tiếng hoặc méo tốc độ phát.
- **Khuyến nghị**: Chuyển đổi Mono sang Stereo theo khối vào buffer tạm và gọi `i2s_write` một lần duy nhất với timeout nhỏ (hoặc theo dõi chính xác số byte thực tế đã ghi vào DMA).

---

## 4. DƯ THỪA & BẤT NHẤT LOGIC (REDUNDANCY & DEAD CODE)

### 🗑️ 4.1. Module `FirebaseClient` hoàn toàn không sử dụng (Dead Code)
- **Vị trí**: [`lib/NetworkManager/FirebaseClient.h`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/FirebaseClient.h), [`lib/NetworkManager/FirebaseClient.cpp`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/FirebaseClient.cpp).
- **Hiện trạng**: Toàn bộ logic Firebase REST API đã được tích hợp trực tiếp vào `NetworkManager` (`syncFirebaseWakeup`, `checkFirebaseFlags`, `checkAndDownloadNewMessages`). Module `FirebaseClient` không còn được include ở bất kỳ đâu trong dự án.
- **Khuyến nghị**: Xóa bỏ 2 file này để làm sạch codebase.

### 🗑️ 4.2. File dữ liệu âm thanh mẫu 1.16MB không sử dụng
- **Vị trí**: [`src/audio_data.h`](file:///P:/coddd/sendove-box/sendlove_firmware/src/audio_data.h).
- **Hiện trạng**: File có dung lượng **1.16MB** chứa mảng `const unsigned char audio_data[] = {...}` (190KB data thô) không được include ở bất cứ file code nào.
- **Khuyến nghị**: Xóa file `src/audio_data.h` để giải phóng dung lượng repository.

### 🗑️ 4.3. Các file backup tồn đọng trong cây thư mục
- `src/main.cpp.bak` (17.1KB)
- `lib/DisplayDriver/DisplayDriver.h.bak` (2.8KB)
- **Khuyến nghị**: Xóa các file `.bak`.

### ⚠️ 4.4. Các hàm Mock và giá trị cứng chưa được kết nối phần cứng thực
1. **Dung lượng Pin**: [`PowerManager.cpp:L38`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/PowerManager/PowerManager.cpp#L38) trả về cứng `return 60;`.
2. **Icon Pin UI**: [`LayoutEngine.cpp:L229`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/LayoutEngine/LayoutEngine.cpp#L229) gán cứng `int state = 3;` (luôn hiển thị nấc 75%).
3. **Lỗi chính tả hiển thị thứ Bảy**: [`NetworkManager.cpp:L157`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L157) mảng `days[]` chứa `"Sar"` thay vì `"Sat"`.

### ⚠️ 4.5. Lệnh xóa Flash W25Q128 (`formatAll`) có nguy cơ kích hoạt FreeRTOS Task Watchdog
- **Vị trí**: [`NandStorage.cpp:L335-L338`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NandStorage/NandStorage.cpp#L335-L338).
- **Phân tích**:
  Lệnh `0xC7` (Chip Erase) trên Flash 16MB mất từ 20 đến 80 giây. Hàm `waitBusyInternal()` polling liên tục `while (SPI.transfer(0x00) & 0x01)` với `delayMicroseconds(100)` mà không nhả CPU hay yield task, dễ làm kích hoạt Task Watchdog Timer (TWDT) gây Panic reset.
- **Khuyến nghị**: Dùng `vTaskDelay(pdMS_TO_TICKS(50))` trong vòng lặp chờ Chip Erase.

---

## 5. RỦI RO BẢO MẬT & AN TOÀN (SECURITY & SAFETY)

### 🚨 5.1. Endpoints OTA Update hoàn toàn không có xác thực (Unauthenticated OTA)
- **Vị trí**: [`OtaHandler.cpp:L74-L80`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/OtaHandler/OtaHandler.cpp#L74-L80).
- **Phân tích**:
  - Hai endpoint `/api/ota/begin` và `/api/ota/upload` mở công khai trên port 80. Bất kỳ máy nào trong mạng Wi-Fi LAN đều có thể gửi file `.bin` độc hại để ghi đè phân vùng ứng dụng của ESP32.
  - Header `Access-Control-Allow-Origin: *` cho phép mọi website người dùng duyệt qua mạng LAN có thể thực hiện Cross-Origin Request nạp đè firmware (CSRF / DNS Rebinding).
- **Khuyến nghị**: Bổ sung Secret Token Header (ví dụ `X-OTA-Key`) kiểm tra mật khẩu trước khi gọi `Update.begin()`.

### ⚠️ 5.2. Thông tin Wi-Fi mặc định bị hardcode trong source code
- **Vị trí**: [`config.h:L81-L84`](file:///P:/coddd/sendove-box/sendlove_firmware/include/config.h#L81-L84).
- **Phân tích**: `DEFAULT_WIFI_SSID = "@Ruijie-s4617"` và `DEFAULT_WIFI_PASSWORD = "56Daiyen"` được commit trực tiếp lên git.
- **Khuyến nghị**: Đưa 2 biến này vào [`config_secrets.h`](file:///P:/coddd/sendove-box/sendlove_firmware/include/config_secrets.h) (file đã được `.gitignore` bảo vệ).

### ⚠️ 5.3. Tên Bucket Firebase Storage bị hardcode trong code logic
- **Vị trí**: [`NetworkManager.cpp:L679`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L679), [`NetworkManager.cpp:L740`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L740).
- **Phân tích**: Chuỗi `"https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/..."` hardcode cố định bucket `iot-app-839a2.firebasestorage.app`.
- **Khuyến nghị**: Định nghĩa `FIREBASE_STORAGE_BUCKET` trong `config_secrets.h`.

### ⚠️ 5.4. Truyền thông tin mật khẩu qua HTTP Cleartext ở Captive Portal
- **Vị trí**: [`NetworkManager.cpp:L200-L223`](file:///P:/coddd/sendove-box/sendlove_firmware/lib/NetworkManager/NetworkManager.cpp#L200-L223).
- **Phân tích**: SoftAP `SendloveBox-Setup` không đặt mật khẩu WPA2, form HTML gửi SSID & Pass qua HTTP Cleartext không mã hóa.
- **Khuyến nghị**: Đặt mật khẩu WPA2 cho SoftAP (ví dụ: `sendlove123`) để bảo vệ dữ liệu truyền tải trên sóng vô tuyến.

---

## 6. LỘ TRÌNH KHẮC PHỤC THEO ĐỘ ƯU TIÊN

```
Priority 1 (Critical):
├── 1. Đổi _spiMutex thành Recursive Mutex chống Deadlock khi gọi DLOG
├── 2. Bổ sung NOP Hack cho SDCardManager::acquireSPI()
└── 3. Bổ sung Token Auth cho OtaHandler (/api/ota/*)

Priority 2 (High):
├── 4. Bọc kiểm tra pdPASS cho xTaskCreate(ntpTaskWorker) trong NetworkManager
├── 5. Tối ưu AudioPlayer::fillChunk (ghi khối stereo, tránh drop sample)
└── 6. Thêm vTaskDelay trong vòng lặp Chip Erase NandStorage::formatAll

Priority 3 (Medium / Cleanup):
├── 7. Xóa các file rác: FirebaseClient.*, audio_data.h, *.bak
├── 8. Chuyển hardcoded secrets (WiFi, Bucket) từ config.h/NetworkManager vào config_secrets.h
└── 9. Sửa lỗi chính tả "Sar" -> "Sat" và nối dữ liệu pin thực tế vào LayoutEngine
```
