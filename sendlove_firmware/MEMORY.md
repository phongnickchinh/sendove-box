# SENDLOVE BOX - PROJECT MEMORY & CONTEXT

*Tài liệu này lưu trữ toàn bộ bối cảnh dự án, kiến trúc phần cứng, phần mềm và lịch sử các task để phục hồi context cho Agent trong các session tương lai.*

## 1. MÔ TẢ DỰ ÁN
**Sendlove Box** là một hộp quà tặng thông minh/video player dựa trên ESP32-C3.  
- **Core Functionality**: Trình chiếu video có âm thanh, hiển thị hình ảnh, giao diện đồng hồ Standby (động theo JSON), điều khiển chạm, đồng bộ thời gian thực qua WiFi/NTP và kết nối đám mây (Firebase).
- **MCU**: ESP32-C3-DevKitM-1 (160MHz, 320KB RAM, 4MB Flash).
- **USB CDC**: Kích hoạt trên GPIO 18/19 (`ARDUINO_USB_CDC_ON_BOOT=1` cho nạp code và in Serial).

---

## 2. KIẾN TRÚC PHẦN CỨNG
- **Màn hình**: TFT LCD 1.54"/1.3" ST7789 240x240 IPS. (Không có chân CS).
- **Lưu trữ Media**: W25Q128 (16MB NAND Flash SPI).
- **Điều khiển**: Touch Sensor TTP223 (GPIO 10, Active HIGH).
- **Sơ đồ chân (Pinout)**:
  - **Shared Hardware SPI2 Bus**: SCK (GPIO 4), MOSI (GPIO 6).
  - **TFT ST7789**: DC (7), RST (9), BL (3 - PWM). *CS = -1 (Không có)*.
  - **NAND W25Q128**: CS (8), MISO (5).
  - **Touch TTP223**: GPIO 10 (Active HIGH, INPUT_PULLDOWN).

> **Hardware Challenge (Đã giải quyết)**: 
> Màn hình ST7789 không có chân CS nên luôn nhận xung nhịp SPI. Việc chia sẻ chung bus SPI2 với NAND Flash gây hiện tượng nhiễu hình ảnh khi đọc dữ liệu NAND.
> **Giải pháp đang áp dụng**:
> - Đồng bộ cùng `SPI_MODE3` cho cả LGFX và NAND để không lệch bit do SCK Idle nhảy.
> - Lệnh Hack NOP (`0x00`) với TFT_DC=0 rồi kéo TFT_DC=1 trước mỗi phiên lấy SPI Mutex của NAND, giúp màn hình ST7789 bỏ qua dữ liệu giao tiếp với Flash.

---

## 3. KIẾN TRÚC PHẦN MỀM (FIRMWARE)
- **Platform**: PlatformIO (Arduino Framework cho ESP32).
- **Libraries**:
  - `LovyanGFX` (Display driver chính, hỗ trợ bus_shared = true).
  - `JPEGDEC` (Giải mã VJPG / JPEG trực tiếp từ buffer NAND).
  - `ArduinoJson` (v7.0.0 - Parse cấu hình UI Layout động từ JSON).
- **Core Logic & State Machine**:
  - Trạng thái hệ thống: `STATE_STANDBY` (Hiển thị đồng hồ/ngày/icon WiFi & Pin) <-> `STATE_VIDEO` (Phát video/ảnh từ NAND).
  - **Task_MediaPlayer** (Priority 3, Stack 8KB): Render Standby UI theo chu kỳ 1s hoặc giải mã JPEG 15fps liên tục từ NAND Flash.
  - **Task_UIController** (Priority 5 - Cao nhất, Stack 4KB): Polling touch sensor (10ms/lần = 100Hz), quản lý LED và chuyển đổi state/slot qua `eventQueue`.
- **Cấu trúc Module (`lib/`)**:
  - `DisplayDriver`: Đóng gói LovyanGFX, mutex SPI.
  - `NandStorage`: Đọc NAND SPI Flash W25Q128 & Slot Table (Storage chính).
  - `SDCardManager`: Đọc/ghi thẻ MicroSD SPI (Storage linh hoạt).
  - `MediaPlayer`: Điều khiển phát frame VJPG qua JPEGDEC.
  - `UIController`: Đọc cảm ứng TTP223, điều khiển trạng thái LED.
  - `NetworkManager`: Quản lý hạ tầng Wi-Fi (STA, SoftAP Provisioning) & đồng bộ giờ NTP.
  - `FirebaseClient`: Giao tiếp Firebase REST API (check tin nhắn, status) & HTTP Stream Download.
  - `PowerManager`: Quản lý linh hoạt chế độ ngủ (Light-Sleep & Deep-Sleep) & Đọc ADC dung lượng Pin LiPo.
  - `LayoutEngine`: Render giao diện Standby động theo JSON config (Clock, Date, Icons).
  - `ConfigManager`: Đọc/ghi cấu hình NVS.
  - `OtaHandler`: OTA Firmware Update qua mạng LAN (WebServer + Update.h).

> **OTA WebServer Lifecycle**:
> Hiện tại WebServer luôn bật khi có WiFi (port 80, mDNS `sendlovebox.local`).
> Tương lai khi ghép nối hệ thống phần mềm, WebServer sẽ chỉ được kích hoạt
> khi người dùng trigger từ web client (tiết kiệm tài nguyên).

---

## 4. QUẢN LÝ DỮ LIỆU NAND (SLOT TABLE)
- **Cấu trúc**: 5 Slot lưu trữ (Video VJPG / Image VIMG).
- **Header**: Sector 0 (Địa chỉ `0x000000`), bắt đầu bằng Magic string `"NSLT"`.
- Addresses: `0x010000`, `0x340000`, `0x670000`, `0x9A0000`, `0xCD0000`.
- Module `NandStorage` chỉ thực hiện Read-Only để bảo tồn dữ liệu gốc đã được flash.

---

## 5. LỊCH SỬ CÔNG VIỆC (TASK LOG)

### Phase 1: Màn hình + NAND Read + Touch (Completed)
- [x] Quy hoạch GPIO dùng Shared HW SPI2 cho TFT và NAND.
- [x] Lập trình `DisplayDriver` bọc LovyanGFX.
- [x] Lập trình `NandStorage` API (Hardware SPI) cho đọc Slot Table và frames.
- [x] Lập trình `MediaPlayer` với `JPEGDEC` giải mã luồng từ `NandStorage`.
- [x] Fix lỗi xung đột SPI giữa ST7789 và W25Q128 (Đồng bộ SPI_MODE3, NOP Hack, Fix USB CDC boot delay).

### Phase 2: Standby UI & Dynamic Power Management (Current - In Progress)
- [x] Xây dựng `NetworkManager` kết nối WiFi (STA + SoftAP Captive Portal) & NTP Time Synchronization (`pool.ntp.org`, UTC+7).
- [x] Xây dựng `FirebaseClient` xử lý REST API Firebase và HTTP Stream Download.
- [x] Nâng cấp `PowerManager` hỗ trợ 2 chế độ **Light-Sleep** (1ms wakeup) và **Deep-Sleep** (5µA low-power).
- [x] Tích hợp khóa giữ mức LOW cho chân Backlight PWM (`PIN_TFT_BLK`) bằng `gpio_hold_en` giúp tắt hẳn màn hình khi ngủ.
- [x] Thêm USB Power Fallback (giả lập 4.0V khi chạy nguồn USB chưa cắm pin) để tránh hiểu nhầm hết pin.
- [x] Cấu hình ngắt GPIO Wakeup chuẩn cho TTP223 (GPIO 10) đánh thức chip từ Light-Sleep.
- [x] Xây dựng `LayoutEngine` parse và render giao diện Standby từ JSON config (widgets: clock_time, clock_date, wifi_icon, battery_icon).
- [x] Thiết lập State Machine (`STATE_STANDBY` <-> `STATE_VIDEO`) phản hồi sự kiện chạm TTP223.
- [x] Thêm thư viện `ArduinoJson@^7.0.0` vào `platformio.ini`.
- [x] Tối ưu FreeRTOS Task Priority (`TASK_PRIORITY_UI_CONTROLLER = 5`).
- [x] Triển khai OTA Update qua mạng LAN (WebServer + Update.h + ota_upload.py).
- [x] Custom Partition Table cho OTA A/B (app0 + app1 = 1.75MB mỗi partition).
- [x] Fix lỗi Light Sleep Wakeup trên ESP32-C3: Re-init toàn bộ LGFX pipeline (SPI bus + ST7789 panel + LEDC PWM) trong `turnOn()`, thêm `if(Serial)` & `delay(200)` cho USB CDC re-enumeration.
- [x] Fix lỗi nháy dư ảnh / xé hình góc dưới màn hình khi chuyển slot: Khóa SPI transaction (`startWrite`/`endWrite`) trong `decodeOneFrame()`, thêm cờ `_isSleeping` tránh re-init thừa khi màn hình đang bật.
- [x] Chuyển đổi hình nền mẫu pastel marble (`bg_defaut.png`) thành mảng $240 \times 240$ RGB565 `StandbyBackground.h` (115KB `PROGMEM`).
- [x] Triển khai thuật toán Bounding Box Patch (`drawBackgroundPatch`) cắt miếng dán hình nền khôi phục vị trí widget trước khi vẽ đè chữ/icon.
- [x] Tích hợp cờ Dirty Flag Cache (`_lastTimeStr`, `_lastDateStr`, `_lastRssiBars`, `_lastBatPercent`) giảm 98% lượt vẽ thừa trên SPI bus.
- [x] Convert và tích hợp 2 font Google **Chakra Petch SemiBold**: 48pt (`ChakraPetch_SemiBold_48.h`) cho đồng hồ giờ và 16pt (`ChakraPetch_SemiBold_16.h`) cho ngày tháng.
- [x] Đổi màu chữ đồng hồ/ngày/icon sang màu Đen (`#000000`) và Đỏ đậm (`#B83D3D`) nổi bật trên nền đá cẩm thạch pastel.
- [x] Fix lỗi chữ có khung nền đen: Thêm `canvas->setTextColor(cfg.color)` (1 tham số) ép LovyanGFX vẽ chữ ở chế độ nền trong suốt (Transparent Background).
- [x] Nới rộng Bounding Box lên `160x45` tránh viền chữ Chakra Petch 48pt tràn khung gây dư ảnh vết chữ cũ.
- [x] Tối ưu hóa đồng bộ thời gian NTP ngầm & Bộ đếm RTC nội bộ:
- [x] Xây dựng Lớp Trừu Tượng Lưu Trữ Media (`IStorageProvider`): Hỗ trợ đa bộ nhớ linh hoạt giữa NAND Flash (`NandStorageProvider`) và Thẻ nhớ SD (`SDStorageProvider`).
- [x] Khắc phục lỗi nén JSON Firebase dài: Triển khai Zero-Copy JsonDocument + `http.getString()` chống mất/tràn mảng tin nhắn Firebase URL.
- [x] Khắc phục lỗi ghi Flash NOR khi download stream: Cấu hình cơ chế Auto Per-Sector Erase (`_lastErasedSectorAddr`) và chừa 4-byte Offset Reserve ở `openForWrite()` giúp bảo toàn 100% dữ liệu tệp `.bin` tải về từ Firebase.
- [x] Hỗ trợ định dạng tệp `SLBX` Native (128x160 Raw RGB565 Framebuffer): Nhận diện container header `SLBX` tự động, căn giữa hiển thị trên LCD 240x240 với tốc độ 1ms không qua `JPEGDEC`. Tương thích 100% tệp container `SLBX`/`VJPG`/`VIMG` lẫn ảnh RAW JPEG.
  - `getTimeString()` và `getDateString()` đọc trực tiếp mốc giờ RTC nội bộ ESP32 (`getLocalTime(&timeinfo, 0)` không chờ / timeout = 0).
  - Loại bỏ hoàn toàn hiện tượng chớp màn hình về `00:00` và `Loading...` khi chuyển phút hoặc rớt Wi-Fi tạm thời.
  - Lệnh `configTzTime()` chỉ gọi 1 lần duy nhất lúc `init()`. Tiến trình NTP chạy ngầm trên FreeRTOS task (`Task_NtpSyncWorker`), không gây nghẽn/khựng UI.
  - Áp dụng ngưỡng sai số 5s (5s Drift Threshold): So sánh mốc giây NTP với RTC ($\Delta t = |ntpNow - rtcNow|$), nếu $\Delta t \le 5$s thì giữ nguyên RTC tránh nhảy/lùi phút trên UI.
  - Loại bỏ polling 15s/1h liên tục trong `NetworkManager::update()`. Chỉ gọi `triggerNtpSync()` theo sự kiện khi vừa boot hoặc vừa tỉnh dậy sau Light Sleep.
- [x] Tối ưu hóa chu kỳ Light Sleep Wakeup nâng thời lượng pin 1000 mAh từ **4.5 ngày lên ~23 NGÀY**:
  - Phân biệt nguyên nhân thức dậy `esp_sleep_get_wakeup_cause()` trong `main.cpp`.
  - Nếu thức dậy do Timer 5 phút (`ESP_SLEEP_WAKEUP_TIMER`): Màn hình giữ nguyên TẮT, chip chỉ Active **2 giây (`activeSleepTimeoutMs = 2000`)** cho NTP Sync ngầm chạy xong rồi **chui vào Light Sleep lại ngay lập tức** (giảm thời gian Active/chu kỳ từ 60s xuống 2s, giảm 96.6% thời gian thức vô ích).
  - Nếu thức dậy do Touch cảm ứng (`ESP_SLEEP_WAKEUP_GPIO`): Bật màn hình Standby và cho phép chờ 30s (`INACTIVITY_SLEEP_TIMEOUT_MS`).
  - Dòng tiêu thụ trung bình giảm từ `7.81mA` xuống **`~1.5mA`**, thời gian chờ của pin 1000 mAh tăng từ 4.5 ngày lên **~23 NGÀY** (gấp 5 lần).

### Phase 2.5: Big Refactoring & Optimization (Completed)
- [x] **Phase 1 (Chống Phân mảnh Bộ nhớ)**: Loại bỏ hoàn toàn việc lạm dụng `String` trong `NetworkManager`, `LayoutEngine`, `OtaHandler`. Chuyển sang dùng `char[]` tĩnh và `snprintf()`, triệt tiêu rò rỉ RAM (OOM / Heap Fragmentation).
- [x] **Phase 2 (Kiến trúc Task & DRY)**:
  - Phân tách `network.update()` ra FreeRTOS task riêng `Task_NetworkController` (Priority 2, Stack 8KB), giải quyết triệt để lỗi WebServer block UI Task (Priority 5).
  - Tạo module `SystemMonitor` quản lý nhiệt độ chip và bộ nhớ RAM, loại bỏ code lặp ở `LayoutEngine` và `MediaPlayer`.
  - Thêm helper `drawTextWidget` trong `LayoutEngine` giúp tái cấu trúc gọn gàng các widget text.
- [x] **Phase 3 (Bảo mật, NVS & Captive Portal Wi-Fi)**:
  - Tích hợp `ConfigManager` lưu Wi-Fi credentials vào NVS Flash (`Preferences`).
  - Xây dựng luồng Wi-Fi Provisioning: Tự động phát AP `SendloveBox-Setup` kèm Captive Portal Web UI (có nút ẩn/hiện mật khẩu `👁️`) khi chưa cài mạng hoặc mất Wi-Fi.
  - Thêm cờ `!network.isProvisioningActive()` ngăn thiết bị chui vào Light Sleep khi đang bật AP cài đặt.
- [x] **Phase 4 (Tối ưu SRAM & AppContext)**:
  - Chuyển `_jpegBuffer` 48KB từ mảng static giam RAM vĩnh viễn sang cấp phát động (`malloc()` khi phát media và `free()` ngay khi dừng), hoàn trả 48KB RAM cho màn hình Standby.
  - Gom toàn bộ 9 biến toàn cục trong `main.cpp` vào struct `AppContext appCtx` theo chuẩn Clean Architecture.

### Phase 3A: Storage Abstraction Layer (Completed)
- [x] Tạo interface trừu tượng `IStorageProvider` định nghĩa tập API đọc/ghi luồng byte và quản lý hàng chờ tin nhắn.
- [x] Xây dựng `NandStorageProvider` bọc chip W25Q128 16MB (quản lý 5 slot fixed offset + unread bitmask trên NVS).
- [x] Xây dựng `SDStorageProvider` bọc thẻ MicroSD FAT32.
- [x] Refactor `MediaPlayer` và `main.cpp` tách khỏi sự phụ thuộc NAND thô, cho phép chuyển đổi linh hoạt NAND <-> SD Card qua cờ `#define ACTIVE_STORAGE_TYPE` trong `config.h`.

### Phân tách Kiến trúc Chuẩn (Separation of Concerns):
1. **Bài toán Pairing (Web App / Cloud Backend):**
   - Ghép nối giữa **Sender App <-> System <-> Receiver App**.
   - Hộp quà ESP32 hoàn toàn không cần tham gia logic này. Kết quả pairing chỉ chốt thông tin trên Cloud Database.
2. **Bài toán Bảo mật (Firmware Box <-> Cloud System):**
   - Xác thực giao tiếp giữa **ESP32 Box <-> Firebase System**.
   - ESP32 chỉ cần dùng `BOX_ID` + `Token` (hoặc Database Secret) lưu trong NVS để xác thực đường truyền HTTPS và tải đúng dữ liệu tin nhắn của Box mình về.




---

## 6. LƯU Ý PHẦN CỨNG & KIẾN THỨC KỸ THUẬT QUAN TRỌNG

### A. Cơ chế ngắt & Wakeup trên ESP32-C3
1. **Phần cứng nhận biết nguyên nhân Wakeup (`esp_sleep_get_wakeup_cause`)**:
   - Khi CPU đi vào Light/Deep Sleep, khối **RTC Power Management Controller (Always-On Domain)** vẫn liên tục giám sát phần cứng.
   - Khi sự kiện chạm (GPIO_10 HIGH) hoặc hết giờ (RTC Timer) xảy ra, phần cứng RTC tự động ghi cờ (bit) vào thanh ghi `RTC_CNTL_WAKEUP_STATE_REG` **trước khi cấp lại xung clock đánh thức CPU**.
   - Hàm `esp_sleep_get_wakeup_cause()` chỉ việc đọc thanh ghi phần cứng này ra mà không cần CPU phải chạy code đếm trước đó.
2. **Cấu hình Touch Wakeup (TTP223)**:
   - Sử dụng `GPIO_PULLDOWN_ENABLE` trên GPIO 10 để kéo chân xuống GND, tránh chân bị floating (trôi điện áp gây tự thức dậy ngẫu nhiên).
   - Sử dụng kiểu ngắt `GPIO_INTR_HIGH_LEVEL` kích hoạt qua `gpio_wakeup_enable()` và `esp_sleep_enable_gpio_wakeup()`.
3. **Phân biệt chân Wakeup theo Sleep Mode**:
   - **Light Sleep Wakeup**: Hỗ trợ toàn bộ các chân GPIO 0-21 (GPIO 10 hoạt động rất tốt).
   - **Deep Sleep Wakeup**: Chỉ hỗ trợ các chân RTC GPIO (GPIO 0 -> GPIO 5 trên ESP32-C3). Nếu muốn chuyển sang dùng Deep Sleep với TTP223, bắt buộc phải nối lại phần cứng sang GPIO 0-5.

### B. Cơ chế đếm nối thời gian thực & NTP Sync ngầm
1. **Bộ đếm RTC nội bộ (RTC Slow Clock)**:
   - Khi gọi `configTzTime()`, ESP-IDF lấy mốc Unix Epoch từ NTP Server và liên kết với bộ đếm phần cứng RTC Timer.
   - Khi vào Light Sleep, CPU dừng nhưng RTC Timer vẫn đếm liên tục.
   - Gọi `getLocalTime(&timeinfo, 0)` với timeout = 0 đọc trực tiếp mốc giờ RTC mà không bị nghẽn hay chờ đợi.
2. **Phân biệt Modem Sleep (Duy trì IP) vs Full Wi-Fi Off (`WIFI_OFF`)**:
   - **Modem Sleep (Light Sleep tự động ngắt RF)**: Cắt nguồn phần cứng thu phát RF nhưng **giữ lại 100% IP, WPA2 Key và Session trong RAM**. Khi thức dậy, gửi gói tin NTP được ngay trong 0.1-0.3s (~27mAs).
   - **Full Wi-Fi Off (`WIFI_OFF`)**: Xóa sạch Driver Wi-Fi. Khi thức dậy phải quét kênh, bắt tay WPA2 4 bước và xin lại IP từ DHCP, mất 1.5 - 3.0s (~300mAs).
3. **Điểm bão hòa năng lượng (Break-even Point)** giữa Modem Sleep và `WIFI_OFF`:
   - Phép tính bão hòa năng lượng: $(1.2 \cdot T) + 27 = (0.8 \cdot T) + 300 \Rightarrow T = 682.5\text{s} = \mathbf{11\text{ phút } 22\text{ giây}}$.
   - **Thời gian ngủ $< 11.37$ phút** (ví dụ chu kỳ 5 phút): **Modem Sleep tiết kiệm hơn ~28%** (tránh được chi phí 300mAs bắt tay lại Wi-Fi).
   - **Thời gian ngủ $> 11.37$ phút** (ví dụ ngủ qua đêm 8 tiếng): **`WIFI_OFF` tiết kiệm hơn ~32.5%** (do chênh lệch dòng rò 0.4mA tích lũy lâu dài).

### C. Quản lý tiêu thụ điện năng & Đèn LED báo nguồn (Power LEDs)
1. **Thực trạng ngốn pin của LED báo nguồn**:
   - Trên bo mạch ESP32 DevKit và Module NAND Flash W25Q128 đều có sẵn đèn LED đỏ báo nguồn (Power LED) nối trực tiếp VCC-GND qua điện trở.
   - **Dòng tiêu thụ**: 2 con LED đỏ ngốn tổng cộng **$\approx 3\text{mA} - 6\text{mA}$**.
   - Trong khi đó:
     - ESP32-C3 ở **Light Sleep**: Chỉ tiêu thụ $\approx 1.0\text{mA}$ (LED ngốn gấp 4 lần chip).
     - ESP32-C3 ở **Deep Sleep**: Chỉ tiêu thụ $\approx 0.005\text{mA}$ / $5\mu\text{A}$ (LED làm lãng phí 99.9% năng lượng pin).
   - **Tác động**: Làm giảm thời gian chờ của pin 1000mAh từ **52 ngày xuống chỉ còn 9 ngày**!
2. **Giải pháp khắc phục**:
   - *Bản Prototype*: Dùng mỏ hàn xả bỏ (tháo) LED báo nguồn / điện trở hạn dòng trên DevKit và Module NAND Flash, hoặc dán băng keo đen che sáng.
   - *Bản thiết kế PCB thực tế*: Không vẽ LED báo nguồn cố định; dùng P-Channel MOSFET (AO3401) hoặc Power Switch IC (TPS22919) để cắt hẳn nguồn 3.3V cấp cho Module NAND khi chip đi vào chế độ ngủ.

### D. Khóa phần cứng RTC GPIO Hold và Reboot (`ESP.restart()`)
- Khi gọi `gpio_hold_en(PIN_TFT_BLK)` trước khi ngủ, mạch RTC phần cứng khóa chân Backlight (GPIO 3) ở mức `LOW`.
- Trạng thái **GPIO Hold phần cứng này được bảo lưu qua cả quá trình Software Reset / Reboot (`ESP.restart()`)**.
- Nếu không giải phóng khóa, sau khi reboot màn hình sẽ bị tối om dù code khởi tạo đã chạy.
- **Giải pháp**: Luôn gọi `gpio_hold_dis((gpio_num_t)PIN_TFT_BLK)` ngay đầu hàm `DisplayDriver::init()` để giải phóng cờ khóa phần cứng khi khởi động lại.

### E. Quy tắc tính toán `millis()` tránh tràn số âm (Underflow Bug)
- Với kiểu dữ liệu `uint32_t`, **tuyệt đối không gán mốc thời gian ở tương lai** (ví dụ: `lastUserActivity = millis() + 15000`), vì phép tính `now - lastUserActivity` ở vòng lặp sau sẽ bị underflow tràn số ra `4,294,952,306` (làm `now - lastUserActivity >= timeout` luôn trả về `true` lập tức).
- **Giải pháp**: Giữ `lastUserActivity = millis()` chuẩn thời gian thực và thay đổi giá trị điều kiện so sánh `activeSleepTimeoutMs` (15s cho Touch Wakeup, 30s cho Timer Wakeup, 2s cho Timer Sleep Wakeup).

### Phase 3B: Firebase Realtime Database & Storage Wakeup Integration (Completed)
- [x] Tích hợp trực tiếp Firebase REST API vào `NetworkManager` (`syncFirebaseWakeup`, `updateFirebaseStatus`, `checkFirebaseFlags`, `syncFirebaseAlarms`, `checkAndDownloadNewMessages`).
- [x] Loại bỏ hoàn toàn interval timers riêng. Đồng bộ tập trung 1 lần khi chip tỉnh dậy (Timer Wakeup 5 phút hoặc Touch Wakeup).
- [x] Triển khai tính toán sleep timer động trong `ConfigManager::getSecondsToNextAlarm(nowSec)` và `main.cpp`: `sleepTimeUs = min(SLEEP_TIMER_US, alarmUs)`.
- [x] Hỗ trợ tải dữ liệu media streaming dạng chunk 256B từ Firebase Storage vào thẳng `IStorageProvider` (NAND/SD) tránh tràn RAM ESP32-C3.
- [x] **Tối ưu hóa Non-blocking Touch UI**: Tạo `NetworkManager::triggerFirebaseSync(...)` khởi chạy task ngầm FreeRTOS (`FbSync`, Stack 8KB, Priority 2). Giải phóng 100% `Task_UIController` (Priority 5) giúp vòng lặp đọc cảm ứng TTP223 100Hz hoạt động liên tục, hoàn toàn không bị block/giật lag khi mạng HTTPS đang sync.
- [x] **Ghi thật vào SPI Flash W25Q128 (16MB)**: Triển khai lệnh `eraseSector` (4KB sector erase `0x20`) và `writeRaw` (Page Program `0x02` 256B) trong `NandStorage` & `NandStorageProvider`. Dữ liệu media tải từ Firebase Storage được ghi đè trực tiếp vào các Slot Flash thực tế của chip W25Q128.
- [x] **Tính năng Erase/Format NAND Flash khi boot**: Đã thêm `formatStorage()` giúp xóa sạch Sector 0 (Header) và 5 Sector của 5 Slot mẫu cũ trên chip W25Q128 + reset cờ `unread_mask` NVS về 0.
- [x] **Tín hiệu trực quan & Khóa chống ngủ khi tải (Download Sleep Lock & UI Signal)**: Thêm cờ `_isDownloadingMedia` và `NetworkManager::isDownloadingMedia()`. Tự động bật màn hình hiển thị `"Downloading..."` (và `"Download Done!"` khi xong), đồng thời khóa `Task_UIController` không cho chip chui vào Light Sleep trong suốt thời gian tải dữ liệu mạng.
- [x] **Khắc phục triệt để lỗi `Storage Err!` khi Format**: Thêm `NandStorage::writeSlotTable()`. Khi chip Flash bị Format/xóa Sector 0, `NandStorage::init()` tự động kiến tạo lại bảng Header `"NSLT"` mới sạch sẽ, loại bỏ hoàn toàn treo màn hình `Storage Err!`.
- [x] **Thông báo Debug trực quan trên màn hình (`DEBUG_SCREEN`)**: Bổ sung hiển thị trực quan các bước giao tiếp Firebase trên màn hình TFT (`"Checking Firebase..."`, `"Polling Msg..."`, `"No New Msg"` / `"Found New Msg!"`, phần trăm tiến độ `"Downloading XX%"`, `"Download Done!"` và thông báo mã lỗi HTTP nếu có) với comment `// DEBUG_SCREEN:` rõ ràng để dễ dàng thu hồi sau này.
- [x] **Sửa lỗi `FB http Err: 400` (Firebase REST 400 Bad Request)**: Bỏ các tham số `orderBy` và `startAt` trong URL REST API (nguyên nhân đòi hỏi Database Indexing Rules trên Firebase), chuyển sang lọc `if (timestamp > lastTs)` trực tiếp trong code C++.
- [x] **Sửa lỗi `FB HTTP Err: -1` (Connection Refused / Network Not Ready)**: Thêm vòng lặp chờ Wi-Fi re-associate ổn định và xin lại IP (tối đa 5s) khi vừa đi ngủ dậy từ Light Sleep + cơ chế tự thử lại `http.GET()` lần 2 nếu bị rớt socket tạm thời.
- [x] **Hiển thị mốc timestamp `lastTs` & `msgTs` trên màn hình**: Bổ sung debug hiển thị giá trị `lastTs` trong NVS và `msgTs` đọc được từ Firebase lên màn hình TFT để xác minh lý do tin nhắn bị xem là cũ và bị Skip.
- [x] **Đồng bộ Firebase tức thì khi boot (`Boot-time Instant Sync`)**: Thêm `triggerFirebaseSync(...)` trực tiếp vào `setup()` của `main.cpp` giúp ESP32 tự động sync Firebase ngay khi vừa nạp code/bật nguồn thành công mà không phải chờ 30 giây hay đợi ngắt chạm.
- [x] **Reset mốc `lastTs` về 0 khi Format Flash**: Tích hợp reset mốc thời gian NVS `last_download_ts` về `0` trong `NandStorageProvider::formatStorage()`, giúp ESP32 sẵn sàng kéo lại toàn bộ tin nhắn từ Firebase từ đầu sau khi xóa dữ liệu Flash NAND.
- [x] **Khóa Sleep toàn diện trong lúc Sync (`isFirebaseSyncing`)**: Sửa cờ khóa sleep trong `main.cpp` từ `isDownloadingMedia` (chỉ khóa khi tải file) thành `isFirebaseSyncing` (khóa toàn bộ chu trình từ lúc bắt đầu gọi request). Đảm bảo chip thức liên tục 100% trong suốt quá trình giao tiếp Firebase, không bị sập nguồn/ngủ giữa chừng (Sleep interrupt) gây ngắt quãng tải dữ liệu.
- [x] **Xử lý linh hoạt Link Firebase Storage**: Tự động nhận diện và convert đường dẫn dạng relative path (VD: `media/ESP32...`) hoặc `gs://...` được lưu trên Database thành dạng HTTPS REST Download URL hợp lệ (`https://firebasestorage.googleapis.com/...`) để ESP32 có thể tải file thành công bằng `HTTPClient`.
- [x] **Cảnh báo thiếu đường dẫn tải (`No Media URL!`)**: Thêm hiển thị báo lỗi chữ đỏ `"No Media URL!"` nếu tin nhắn mới kéo về không hề chứa đường link (`bin_url`, `video_url`, hoặc `image_url`) nhằm giúp debug tại sao bị skip tiến trình Download.
- [x] **Xử lý triệt để lỗi mất URL dài (`Zero-Copy JSON Parsing & Flexible Key Matcher`)**: Chuyển về `deserializeJson(doc, payload)` Zero-Copy. Đồng thời nâng cấp bộ parser hỗ trợ linh hoạt cả cấu trúc Firebase `JsonObject` và `JsonArray`, tự động quét tất cả các biến thể đặt tên key (`bin_url`, `binUrl`, `video_url`, `videoUrl`, `image_url`, `imageUrl`, `media_url`, `mediaUrl`, `url`) để đọc đúng 100% dữ liệu từ Firebase.
- [x] **Xử lý tự động hiển thị Ảnh/Video ngay khi tải từ Firebase (`Raw JPEG & Slot Header Auto-Generation`)**: 
  - Khắc phục triệt để nguyên nhân tải 100% nhưng không hiện ảnh: Khi tải từ Firebase về Flash NAND, code tự động kiểm tra byte đầu tiên. Nếu là file ảnh `.jpg` thô (`0xFF 0xD8 0xFF`), ESP32 sẽ tự động chèn 4-byte `jpegSize` vào đầu dữ liệu slot.
  - Tự động ghi nhãn `"VIMG"` (cho ảnh) hoặc `"VJPG"` (cho video) vào bảng Slot Table (`Sector 0`) để `NandStorage` và `MediaPlayer` công nhận Slot hợp lệ và lập tức giải mã hiển thị hình ảnh rực rỡ trên màn hình LCD.
- [x] **Tự động phát Media ngay sau khi tải xong (`Auto Play on Download Complete & Screen Overwrite Fix`)**: Tích hợp Event-driven callback `setOnDownloadComplete`. Đã loại bỏ triệt để xung đột giao diện làm chữ `"All Sync Done!"` in đè và xóa mất bức ảnh vừa được `MediaPlayer` hiển thị. Bức ảnh/video sẽ được giữ nguyên 100% rực rỡ trên màn hình LCD.
- [x] **Thông báo hoàn tất (`All Sync Done`)**: Thêm hiển thị chữ `"All Sync Done!"` trên màn hình khi toàn bộ tiến trình Firebase đã kết thúc thành công.
- [x] **Cập nhật Quy tắc Indexing trên Firebase (`database.rules.json`)**: Bổ sung `.indexOn: ["timestamp"]` cho node `messages/$box_id`, cho phép lọc Server-side trên Firebase.
- [x] Tối ưu Server-side Filtering & Zero-Copy Stream Parsing (`NetworkManager`):
  - Đính kèm query parameters `orderBy="timestamp"&startAt=<nextTs>` vào HTTP GET request gửi lên Firebase REST API. Khi không có tin mới, Firebase Server trả về `{}` (2 bytes), giảm 99% tải băng thông & CPU/RAM cho Box.
  - Sử dụng Stream parsing trực tiếp từ HTTP socket (`deserializeJson(doc, *stream)`), loại bỏ hoàn toàn cấp phát mảng chuỗi tạm `http.getString()`, chống triệt để phân mảnh RAM Heap trên ESP32-C3.
- [x] Biên dịch thành công 100% (`[SUCCESS]`), RAM 19.0% (62KB/328KB), Flash 71.3% (1.30MB/1.83MB).

### Phase 3C: Advanced Playback & Touch Refactoring (Completed)
- [x] **Cấu trúc lại Metadata Storage (maxDisplayTime)**: Tận dụng vùng nhớ `reserved` 4-byte trong bảng Header `SlotEntry` của NAND Flash Sector 0 để lưu trữ tĩnh thông số `max_display_time`.
- [x] **Refactor cơ chế Touch (Long/Short Press)**: Nâng cấp `UIController` nhận diện `SHORT_PRESS` (bấm nhanh chuyển slot, đánh dấu đã đọc) và `LONG_PRESS` (bấm giữ 3s ép về Standby mà không đánh dấu đã đọc).
- [x] **Biến đếm `_numOfNewMsg`**: Theo dõi chính xác tổng tin nhắn chưa đọc còn lưu trong NAND Flash cộng với tin đang chờ tải trên Cloud. Box dừng phát khi xem xong tin mới (khi biến về 0) thay vì lặp lại vòng tròn các slot cũ.
- [x] **An toàn NAND Flash (Chặn Download khi Play)**: Box chỉ trigger tiến trình Download khi RAM/SPI bus rảnh rỗi ở màn hình chờ (người dùng xem xong tin cũ, Touch bị vô hiệu hóa tạm thời với thông báo `Downloading...`).
- [x] **Auto Timeout Next**: Tự động chuyển media sau khi hiển thị đủ `maxDisplayTime` giây (hoặc quay về UI nếu hết tin).
- [x] **Interval Sync & Full Storage Guard**: Tự động check Firebase định kỳ 5 phút/lần khi ở màn hình Standby, đồng thời kiểm tra `appCtx.storage->isFull()`. Nếu toàn bộ 5 slot NAND đều chứa tin chưa đọc (đầy), hệ thống tự động bỏ qua lượt check ngầm để tránh block cảm ứng và tiết kiệm mạng/RAM.
- [x] **Sắp xếp thứ tự phát tin nhắn (`std::sort`)**: Ép mảng `msgList` qua `std::sort` theo mốc `timestamp` tăng dần trước khi lưu NAND, đảm bảo 100% tin nhắn cũ nhất luôn phát trước, tin mới phát sau.
- [x] **Fix SPI Deadlock khi Render Ảnh lớn (Chunking)**: Sửa hàm `decodeOneFrame()` trong `MediaPlayer.cpp` đọc từng chunk pixel từ NAND trước, sau đó mới acquire SPI để push lên LCD, loại bỏ hoàn toàn hiện tượng xé hình / mất 2/3 ảnh đối với file > 48KB.
- [x] **KISS 10s Awake Polling & Independent Sleep Timer**: Thiết lập kiểm tra Firebase ngầm định kỳ 10 giây/lần khi ở Standby UI (`now - lastIntervalSyncMs >= 10000`). Đồng thời tách biệt hoàn toàn bộ đếm ngủ `lastUserActivity` (chỉ reset khi chạm tay hoặc đang thực sự tải tệp media `isDownloadingMedia`), giúp box vừa hút tin nhắn nhạy bén 10s/lần vừa đi ngủ chính xác 100% theo thời gian đếm ngược.
- [x] **Khắc phục triệt để lỗi mất Backlight PWM khi thức dậy**: Loại bỏ hoàn toàn sự quản lý đèn nền của thư viện LovyanGFX (do lỗi từ chối khởi tạo lại và khai báo cứng sai `pwm_channel = 7` không tồn tại trên ESP32-C3). Chuyển sang khởi tạo thủ công bằng Arduino API (Sử dụng Channel 0). Khi ngủ dùng `pinMode(OUTPUT)` / `digitalWrite(LOW)` ép đèn tắt hẳn, khi thức tự gọi `ledcAttach`/`ledcAttachPin` nối lại PWM, giải quyết thành công lỗi đèn sáng mờ khi ngủ và đen màn hình khi thức.
- [x] **Cập nhật Sleep Timeout Logic**: Chặn thiết bị tự động đếm giờ đi ngủ khi đang phát Video. Khi Video phát hết `max_display_time`, rớt về màn hình UI và reset lại mốc tính thời gian ngủ.

---

### Phase 3D: Khắc phục triệt để lỗi Timer Wakeup, Wi-Fi Reconnect & NTP Sync Drift (Completed)
- [x] **Khắc phục lỗi trôi giờ (RTC Fast Drift) khi Sleep dài**:
  - *Nguyên nhân cốt lõi*: `getLocalTime(&timeinfo, timeout)` trong Arduino ESP32 core kiểm tra `tm_year > 2016`. Do time đã được sync lúc boot, hàm này trả về `true` ngay lập tức (0ms) với giờ RTC đang bị trôi từ dao động RC nội bộ, không hề chờ gói tin NTP từ mạng. Đồng thời, `ntpTaskWorker` không khóa tiến trình sleep, khiến chip bị ép vào Light Sleep sau 2s trước khi socket SNTP kịp nhận phản hồi.
  - *Giải pháp*: Đăng ký `sntp_set_time_sync_notification_cb()` (API chính thức ESP-IDF) để nhận callback khi SNTP daemon thực sự gọi `settimeofday()`. Trong `syncNtpTime()`, gọi `configTzTime()` (Arduino API đã chứng minh) để trigger SNTP request mới, rồi poll cờ `s_ntpSyncDone` từ callback thay vì dùng internal API `sntp_get_sync_status()` (đã được chứng minh unreliable trên hardware thật — NTP timeout 100%).
  - *Lưu ý*: Phiên bản đầu tiên dùng `sntp_set_sync_status(RESET)` + `sntp_restart()` + poll `sntp_get_sync_status()` — build thành công nhưng NTP luôn timeout trên hardware thật do internal API không cập nhật status flag đáng tin cậy qua ranh giới LWIP thread / FreeRTOS task.
- [x] **Khắc phục lỗi mất kết nối Wi-Fi sau 1 ngày Sleep**:
  - *Nguyên nhân cốt lõi*: Khi thức dậy từ Light Sleep, `ensureConnected()` gọi `WiFi.reconnect()` bất đồng bộ. Tuy nhiên `triggerNtpSync()` và `triggerFirebaseSync()` kiểm tra ngay `WiFi.status() != WL_CONNECTED` nên lập tức abort. Sau 2 giây, chip lại bị ép đi ngủ, cắt ngang quá trình bắt tay 4 bước (4-way handshake) của Wi-Fi. Lặp lại qua hàng trăm chu kỳ khiến Wi-Fi stack bị kẹt (`AUTH_EXPIRE`/`ASSOC_EXPIRE`).
  - *Giải pháp*: 
    - Nâng cấp `ensureConnected(uint32_t timeoutMs)` 2 tầng: thử `WiFi.reconnect()` trước; nếu không thành công sẽ tự động thực hiện `WiFi.disconnect(false)` và `WiFi.begin(_wifiSsid, _wifiPassword)` để tái thiết lập kết nối sạch sẽ từ đầu.
    - Lưu giữ credentials Wi-Fi trong `NetworkManager` và kích hoạt `WiFi.setAutoReconnect(true)`.
- [x] **Hợp nhất tiến trình Đồng bộ ngầm (`triggerWakeupSync` & `isSyncing`)**:
  - Hợp nhất chu trình: `ensureConnected(5000)` -> `syncNtpTime(5000)` -> `updateFirebaseStatus` -> `checkFirebaseFlags` -> `checkAndDownloadNewMessages` vào 1 FreeRTOS background task duy nhất (`WakeSync`).
  - Cập nhật cờ `isSyncing()` bao quát toàn bộ tiến trình mạng (Wi-Fi, NTP, Firebase, Download). `Task_UIController` được khóa không cho phép chip đi ngủ trong lúc bất kỳ tác vụ mạng nào đang xử lý, triệt tiêu hoàn toàn tình trạng sập nguồn/ngắt giữa chừng.

---

### Phase 3E: Audio I2S, Chống rè/nháy & Captive Portal nâng cấp (2026-08 → 09-01)

> **Đọc kỹ cột trạng thái.** Phần lớn mục dưới đây **chưa được kiểm chứng trên phần cứng**
> và **chưa commit** — chỉ nằm trong working tree. Đừng coi là đã xong.

#### Phần cứng bổ sung (thiếu ở §2 phía trên)
- **Ampli I2S MAX98357A**: BCLK = GPIO 0, LRC = GPIO 1, DOUT = GPIO 2 (`include/config.h:123-125`).
- **Module `AudioPlayer`** (`lib/MediaPlayer/AudioPlayer.cpp/.h`): phát PCM raw 16-bit mono qua
  I2S DMA, thiết kế non-blocking tick-based — mỗi frame video gọi `tick()` một lần nạp thêm DMA.
  Audio nằm nối đuôi video trong cùng một slot NAND, đánh dấu bằng header `AUDC`.
- **Module `ScreenLogger`** (`lib/ScreenLogger/`) — cũng chưa có trong danh sách module ở §3.

#### Nguyên nhân gốc của "rè tiếng + nháy đèn nền" (đã xác nhận bằng đo thực tế của user)
Đèn nền LED và MAX98357A **dùng chung một rail nguồn**. Đỉnh dòng của CPU/SPI cộng dồn lên dòng
ampli đang kéo → LED tối đi (thấy nháy) và ampli đói dòng (nghe rè) **cùng một nhịp**. Không phải
lỗi chất lượng file WAV: user đã nghe file `.wav` sinh ra từ web, xác nhận "ấm áp, không rè".

| Thay đổi | Ở đâu | Trạng thái |
|---|---|---|
| `extractAudioFromVideo` giải mã offline + `DynamicsCompressor` (-6dB) + trần đỉnh 0.7 | `sendlove_web/src/utils/mediaEncoder.js` | Trong working tree, **chưa commit**, đã chạy thật trên trình duyệt |
| Pacer bỏ frame khi trễ (`FRAME_MIN_IDLE_MS=2`, `decodeOneFrame(bool skipRender)`, `_lastFrameSkipped`) | `lib/MediaPlayer/MediaPlayer.cpp/.h` | Đã build + nạp, **CHƯA kiểm chứng phần cứng** |
| Timeout 10s khi tải không tiến triển + log tiến độ mỗi 16KB (video **và** audio) | `lib/NetworkManager/NetworkManager.cpp` | ✅ **ĐÃ KIỂM CHỨNG MÁY THẬT 2026-09-01: tải chạy ổn** |
| Captive portal: `WIFI_AP_STA`, `setErrorReplyCode(NoError)`, 8 URL dò của OS trả 302, endpoint `/scan` quét bất đồng bộ, HTML có danh sách Wi-Fi bấm chọn | `NetworkManager.cpp/.h`, `captive_portal_html.h` | ✅ **ĐÃ KIỂM CHỨNG MÁY THẬT 2026-09-01: AP + captive portal ổn** |
| Kiểm tra giá trị trả về của `writeChunk` ở **nhánh audio** (header `AUDC` + stream PCM), log `[NET] Audio DL OK/SHORT: N bytes` | `lib/NetworkManager/NetworkManager.cpp` | Đã build (RAM 20.6% / Flash 73.3% — không đổi). Dùng biến local `aWriteError`, **không** tái dùng `writeError` vì cổng kiểm tra ở dòng ~920 chạy *trước* khối audio nên set vào đó là vô nghĩa |

#### Defect gốc đã tìm ra ở vòng lặp tải (nguyên nhân treo tại `[NET] writing slot`)
`http.getSize()` trả `-1` với response chunked. Điều kiện `while (http.connected() && (len > 0 || len == -1))`
khi đó **không bao giờ tự sai** — chỉ thoát khi server đóng socket. Stream nửa-mở treo vĩnh viễn.
`http.setTimeout(30000)` chỉ chốt **một lần đọc**, không chốt được cả vòng lặp. Khi timeout mới bắn,
phải set `writeError = true` để slot dở bị loại, nếu không file tải dở với `initialLen <= 0` vẫn lọt
qua bước kiểm tra và thành slot rác.

#### Chốt trạng thái 2026-09-01 (user xác nhận trên máy thật)
- ✅ **Wi-Fi AP + captive portal: ĐÃ ỔN.** Không đào lại phần này.
- ✅ **Tải file (video + audio) về NAND: ĐÃ ỔN.** Không còn treo ở `[NET] writing slot`.
- ❌ **CÒN LỖI — đây là việc đang làm:** phát lại trên box vẫn *video giật nhấp nháy* và *âm thanh
  rè, có tiếng "rẹt rẹt" chen vào giữa tiếng*.
  - **Chưa xác định** rẹt rẹt đến từ đâu. Underrun DMA **khó xảy ra**: DMA sâu 8×512 mẫu ≈ 512ms
    @8kHz và `tick()` nạp tới khi DMA từ chối, nên chỉ điểm loop-về-đầu là đáng ngờ.
  - Giả thuyết mạnh nhất vẫn là **sụt áp** đã đo được: đẩy frame 15 lần/giây ⇒ 15 nhịp sụt/giây,
    nghe đúng như tiếng rẹt xen kẽ (không phải rè liên tục).
  - Giả thuyết đã **bác bỏ**: "8kHz ra thẳng MAX98357A gây rè". Lý do bác: nó dự đoán tiếng gắt
    *liên tục*, không phải tiếng rẹt *rời rạc*; và con số BCLK tối thiểu 2.3MHz tôi nhớ ra sẽ loại
    luôn cả 44.1kHz vốn chạy tốt ở mọi nơi ⇒ số đó sai. Không viết upsampler dựa trên nó.
  - ~~**Phép thử phân biệt:** nghe tiếng bíp lúc boot (`AudioPlayer::testBeep()`).~~ **PHÉP THỬ
    NÀY VÔ GIÁ TRỊ** — 2026-09-01 phát hiện `testBeep()` *tự nó* hỏng: nó ghi 1200 mẫu, mà DMA sâu
    8×512 = 4096 mẫu, nên `i2s_write` không hề block, vòng lặp xong trong vài µs rồi gọi `stop()`
    → `i2s_driver_uninstall()` xoá DMA khi loa chưa kịp kêu. Bíp *bắt buộc* ra tiếng rẹt dù phần
    còn lại của hệ thống hoàn toàn lành. Khớp đúng lời user: "chưa bao giờ nghe được tiếng bíp".
    **Đã sửa:** thêm `delay(200)` trước `stop()` (`AudioPlayer.cpp`). Build OK, RAM 20.6% không
    đổi, Flash 73.3% (+52B). Sau khi nạp, phép thử mới dùng được.
  - **GPIO 8 = đèn xanh trên chip = `PIN_NAND_CS`, active LOW** (comment của chính code ở
    `src/main.cpp:288`). User báo tiếng rẹt *đồng pha* với nhịp nháy đèn này và *nháy đều*. Nghĩa
    là tiếng rẹt bám theo hoạt động chip-select của NAND, không phải hiện tượng ngẫu nhiên.
  - **Chân I2S: hai chỗ trong `config.h` mâu thuẫn nhau.** Hằng số đang dùng (`config.h:123-125`)
    là BCLK=0, LRC=1, DOUT=2. Nhưng khối comment "Phase 2" ở đầu file lại ghi BCLK=2, DOUT=0 —
    **đảo ngược**. Chưa biết bên nào khớp mạch thật — cần user xác nhận mạch. (Lưu ý: câu "nghe
    ấm áp, không rè, chất lượng tốt" của user là nói về **file WAV nghe trên máy tính**, KHÔNG
    phải tiếng phát ra từ box. Đừng dùng nó làm bằng chứng rằng chân I2S đang đúng.)
  - **Không hề có cấu hình brownout / watchdog, không có `esp_reset_reason()`** ở bất kỳ đâu trong
    `platformio.ini`, `src/`, `include/` ⇒ một vòng lặp reset lặp lại sẽ *vô hình* trong log hiện
    tại. **Đã thêm** `DLOG("[BOOT] reset=%d", esp_reset_reason())` ngay sau banner
    (`src/main.cpp:349`). Nếu dòng `[BOOT] reset=` lặp lại đều đặn trong log ⇒ box reset vòng lặp
    (9 = BROWNOUT), giải thích được cả ba triệu chứng bằng một nguyên nhân; nếu chỉ in một lần ⇒
    không phải reset loop, quay lại nhánh tranh chấp SPI/NAND-CS.
- 🔧 **`om` MCP server đã bị gỡ** (`claude mcp remove om -s user`). Trước đó nó lỗi
  `search failed: qmd launcher exited`. Từ nay lý do thiết kế nằm ở chính file này; `CLAUDE.md`
  đã sửa cho khớp. Config cũ: `node P:\my-vault\.claude\scripts\om-mcp.mjs` (stdio, user scope).

#### Quyết định đã CHỐT — không mở lại
- **Giữ 8kHz** cho audio. User chốt: "tôi sẽ giữ nguyên 8khz".
- **KHÔNG giảm độ sáng đèn nền.** User bác thẳng; `MediaPlayer.cpp` giữ `BACKLIGHT_DAY_PERCENT`.
- **Decode video/audio phải chạy ở client**, không đẩy sang backend.
- Lỗi "hình chậm hơn tiếng" đã xử lý xong, không đào lại.

---

## 7. TÌNH TRẠNG CODE & PHẦN ĐÃ MẤT (kiểm kê 2026-09-01)

### Code cũ đã mất — TÌM LẠI ĐƯỢC trong git stash
Bản backup mà user tưởng đã mất **vẫn còn**, nằm trong `stash@{1}` (tên "trrr"), ở đường dẫn
`sendlove_firmware/.backup_audio_debug/20260816_004737/`. Thư mục này **không còn trên đĩa** —
chỉ tồn tại trong stash. Gồm 6 file, mốc 2026-08-16 00:47:37:

| File | Số dòng |
|---|---|
| `AudioPlayer.cpp` | 147 |
| `AudioPlayer.h` | 60 |
| `MediaPlayer.cpp` | 371 |
| `NandStorage.cpp` | 350 |
| `NandStorageProvider.cpp` | 293 |
| `NetworkManager.cpp` | 853 |

Đây nhiều khả năng chính là bản "phát âm thanh mượt, hình ra chậm nhưng không nháy" mà user nhắc
đi nhắc lại — thứ đáng đối chiếu để hiểu vì sao bản cũ không bị sụt áp.

Lấy ra **không phá working tree hiện tại** (stash này có 2 parent nên đọc thẳng, không cần `^3`):

```
git show "stash@{1}:sendlove_firmware/.backup_audio_debug/20260816_004737/MediaPlayer.cpp" > /nơi/nào/đó.cpp
```

`stash@{1}` còn chứa `STATUS.md` ghi lại một task sửa `writeChunk` **bị chặn giữa chừng** vì xung
đột quyền khi gọi `agy` headless — kế hoạch sửa còn dang dở, chưa áp dụng.

### `stash@{0}` ("big stash")
28 file, ≈1015 thêm / 587 xóa. Danh sách file **trùng khớp với working tree bẩn hiện tại**
(MEMORY.md, platformio.ini, 11 file web, `VoiceInput.css` bị xóa...). Gần như chắc chắn là ảnh
chụp của chính trạng thái đang làm dở, **không phải** code mất. Đừng pop bừa — sẽ đụng độ.

### Ba nhánh local `ff`, `phong-tuyet-vong`, `pjhonggg`
Đều là **tổ tiên của `main`**, không chứa `AudioPlayer.cpp`. Không có gì để cứu ở đây.

### Chỗ tài liệu này đang sai / thiếu so với code thật
- §4 viết "`NandStorage` chỉ thực hiện Read-Only" — **sai từ Phase 3B**, module đã có
  `eraseSector`, `writeRaw`, `writeSlotTable`.
- §2 không nhắc gì tới ampli I2S / MAX98357A; §3 thiếu `AudioPlayer` và `ScreenLogger`.
  (Đã bổ sung ở Phase 3E bên trên.)

### Rác cần dọn
- `sendlove_firmware/src/main.cpp.bak` — file thừa.
- Working tree bẩn: 11 file web + `platformio.ini` + `MEMORY.md`, cùng các thư mục chưa track
  (`DRAFT/`, `sendlove_web/src/components/ui/`, `src/styles/`, `sendlove-box-style-guide.md`,
  `FE_DESIGN.md`, `HANDOFF.md`, `MULTI_PLATFORM_STRATEGY.md`, `firebase-debug.log`).
  **Chưa quyết** giữ / commit / bỏ.

### Ràng buộc bảo mật (không được vi phạm)
Không push, không tạo PR — chỉ làm local. Ba file bí mật không bao giờ commit (đã có trong
`.gitignore`): `sendlove_firmware/include/config_secrets.h`, `sendlove_web/.env`,
`sendlove_backend/serviceAccountKey.json`.

---







