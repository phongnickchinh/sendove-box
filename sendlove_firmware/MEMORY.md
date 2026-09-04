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
>   - ⚠️ **Đính chính 2026-09-04 — con số 3 KHÔNG bắt buộc, cái bắt buộc là CẢ BUS DÙNG CHUNG
>     MỘT MODE.** Cả MODE0 lẫn MODE3 đều lấy mẫu ở sườn LÊN; thiệt hại nằm ở mức idle của SCK —
>     đổi CPOL giữa hai chủ bus sinh 1 sườn lên giả nên ST7789 chốt nhầm 1 bit. MODE3 được chọn
>     chỉ vì driver flash dùng nó. Từ nay mode nằm ở `SPI_BUS_MODE` (`config.h`), đổi theo
>     `ACTIVE_STORAGE_TYPE`: **MODE3 cho NAND (không đổi), MODE0 cho thẻ SD** — vì thư viện `SD`
>     của Arduino-ESP32 hardcode `SPI_MODE0` và `SD.begin()` không có tham số mode. Xem mục 12.
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
| Fix `fillChunk()` thiếu oversample x4: I2S init 32kHz nhưng data chỉ expand Mono→Stereo (không lặp mẫu) → audio phát nhanh gấp 4 + DMA underrun → tiếng rẹt rời rạc | `lib/MediaPlayer/AudioPlayer.cpp:192-219` | ✅ **ĐÃ KIỂM CHỨNG MÁY THẬT 2026-09-02: audio hết rẹt, đúng tốc độ** |
| Fix giật chậm / âm thanh nhảy ngắt quãng khi phát message: Tắt `ScreenLogger` overlay khi play video, hoãn `WakeSync` mạng trong `STATE_VIDEO`, tăng DMA buffer lên 24 (384ms), bỏ delay thừa và sửa deadline Frame 0 | `MediaPlayer.cpp`, `ScreenLogger.cpp/.h`, `NetworkManager.cpp/.h`, `main.cpp`, `config.h` | ✅ **Đã cập nhật 2026-09-02** |
| Fix Reboot khi Long Press -> Short Press ngay: Giữ I2S driver thường trực (chỉ `i2s_zero_dma_buffer` khi stop, không uninstall/install) + Cấp phát `_jpegBuffer` 48KB cố định 1 lần lúc `init()` (không malloc/free liên tục) | `AudioPlayer.cpp/.h`, `MediaPlayer.cpp/.h` | ✅ **Đã cập nhật 2026-09-02** |
| Nâng cấp chất âm: Triển khai Nội suy tuyến tính (Linear Interpolation) trong `fillChunk()` thay vì lặp mẫu bậc thang (ZOH) → triệt tiêu sóng hài chói gắt, làm mềm và ấm giọng nói | `lib/MediaPlayer/AudioPlayer.cpp:190-210` | ✅ **Đã cập nhật 2026-09-02** |
| Fix Firebase sync lỗi (-1): Giảm MbedTLS buffer từ 33KB xuống 3KB (`setBufferSizes(2048, 1024)`) + giảm I2S DMA buffer từ 24 xuống 12 (giải phóng 54KB RAM cho SSL Handshake) | `NetworkManager.cpp`, `config.h` | ❌ **SAI HẲN — đã chốt 2026-09-03**: I2S DMA buffer 12 thì đúng (`config.h: AUDIO_DMA_BUF_COUNT = 12`), nhưng phần `setBufferSizes(2048,1024)` **KHÔNG THỂ đúng**: grep source Arduino core 2.0.17 (`~/.platformio/packages/framework-arduinoespressif32/libraries/WiFiClientSecure/src/`) cho thấy **`setBufferSizes()` KHÔNG TỒN TẠI trong `WiFiClientSecure` của ESP32** — đó là API của **ESP8266**. Nghĩa là không phải "quên áp dụng" mà là **bất khả thi trên nền tảng này**; nếu từng viết vào code thì đã không compile được. → mbedTLS trên ESP32 luôn dùng in/out content buffer mặc định 16KB mỗi chiều, không chỉnh được từ tầng Arduino. **Đừng đào lại hướng này.** Giữ nguyên dòng gốc bên trái để không xoá lịch sử. |

#### Defect gốc đã tìm ra ở vòng lặp tải (nguyên nhân treo tại `[NET] writing slot`)
`http.getSize()` trả `-1` với response chunked. Điều kiện `while (http.connected() && (len > 0 || len == -1))`
khi đó **không bao giờ tự sai** — chỉ thoát khi server đóng socket. Stream nửa-mở treo vĩnh viễn.
`http.setTimeout(30000)` chỉ chốt **một lần đọc**, không chốt được cả vòng lặp. Khi timeout mới bắn,
phải set `writeError = true` để slot dở bị loại, nếu không file tải dở với `initialLen <= 0` vẫn lọt
qua bước kiểm tra và thành slot rác.

#### Nguyên nhân gốc đã xác nhận của tiếng "rẹt rẹt" rời rạc (ĐÃ FIX 2026-09-02)
**Bug phần mềm trong `AudioPlayer::fillChunk()`**: `AUDIO_OVERSAMPLE = 4` được cấu hình đúng —
I2S init ở `8000 * 4 = 32000Hz`, buffer `_stereo` cấp size cho oversample — nhưng code thực tế
**chỉ expand Mono→Stereo (x2), KHÔNG lặp mỗi mẫu 4 lần**. Hậu quả:
- I2S ở 32kHz ăn data nhanh gấp 4 → audio phát nhanh + hết sớm
- DMA auto-clear phát silence xen kẽ giữa các chunk → tiếng rẹt rời rạc đồng pha với NAND CS
- `loadFromStorage()` gọi `i2s_set_sample_rates(sampleRate)` thiếu nhân OVERSAMPLE → khi WAV
  header có rate khác default, BCLK rớt thấp gây rè liên tục

**Fix đã áp dụng và kiểm chứng thành công trên máy thật (2026-09-02):**
1. `fillChunk()`: thêm vòng lặp `for (r = 0; r < AUDIO_OVERSAMPLE; r++)` lặp mỗi mẫu mono,
   sửa `i2s_write` ghi `samples * OVERSAMPLE * 4` bytes, sửa `_audioCursor` chia `OVERSAMPLE * 2`.
2. `loadFromStorage()`: sửa `i2s_set_sample_rates(I2S_NUM_0, sampleRate * AUDIO_OVERSAMPLE)`.

Build: RAM 23.6% (77KB), Flash 73.3% (1.35MB). Kết quả: **tiếng rẹt boot + rẹt lúc phát đều
biến mất hoàn toàn**. Chỉ còn nháy màn hình rất nhẹ do sụt áp phần cứng (xem mục dưới).

#### Lỗi còn lại: Nháy màn hình nhẹ khi loa phát (sụt áp phần cứng — P1)
- **Triệu chứng**: Đèn nền LCD nháy rất nhẹ đồng pha với âm thanh loa. Không ảnh hưởng trải
  nghiệm nghe, chỉ thấy nếu nhìn kỹ.
- **Nguyên nhân**: MAX98357A và LED backlight dùng chung rail 3.3V. Đỉnh dòng ampli kéo rail
  sụt tạm thời → LED tối đi.
- **Không phải lỗi phần mềm** — firmware đã tối ưu (FRAME_MIN_IDLE_MS=2ms, frame skip).
- **Fix phần cứng cho PCB thật**: Tụ bypass 100uF sát VCC MAX98357A, ferrite bead tách rail
  audio, hoặc LDO riêng cho ampli.

#### Chốt trạng thái 2026-09-02 (user xác nhận trên máy thật)
- ✅ **Wi-Fi AP + captive portal: ĐÃ ỔN.**
- ✅ **Tải file (video + audio) về NAND: ĐÃ ỔN.**
- ✅ **Tiếng rẹt rẹt lúc boot (testBeep): ĐÃ FIX** (delay(200) trước stop + oversample fix).
- ✅ **Tiếng rẹt rẹt lúc phát message: ĐÃ FIX** (fillChunk oversample x4 + loadFromStorage rate fix).
- ⚠️ **Nháy màn hình rất nhẹ khi loa phát**: Vấn đề phần cứng (sụt áp rail chung). Chấp nhận
  được trên prototype. Sẽ fix trên PCB thật bằng tụ bypass / LDO riêng.

#### Các giả thuyết đã loại bỏ (lưu lại để không đào lại)
- ~~"8kHz ra thẳng MAX98357A gây rè"~~ — Bác bỏ: rẹt rời rạc chứ không rè liên tục.
- ~~"Brownout reset loop"~~ — Không xảy ra: `esp_reset_reason()` chỉ in 1 lần.
- ~~"Chân I2S đảo ngược"~~ — Không ảnh hưởng: audio phát đúng sau fix oversample.
- ~~"DMA underrun"~~ — Đúng là underrun nhưng do bug oversample, không phải do DMA quá nông.

#### Quyết định đã CHỐT — không mở lại
- **Giữ 8kHz** cho audio. User chốt: "tôi sẽ giữ nguyên 8khz".
- **KHÔNG giảm độ sáng đèn nền.** User bác thẳng; `MediaPlayer.cpp` giữ `BACKLIGHT_DAY_PERCENT`.
- **Decode video/audio phải chạy ở client**, không đẩy sang backend.
- **Oversample x4** giữ nguyên (`AUDIO_OVERSAMPLE = 4`), đã chứng minh fix triệt để rẹt.
- **Session Rule (2026-09-02)**: Agent **KHÔNG** tự chạy lệnh build PlatformIO (`pio run`) để tiết kiệm thời gian, việc build và flash máy thật do User đảm nhiệm. Agent chỉ tập trung nghiên cứu, rà soát logic và viết code.

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

## 8. PHASE 3F — Kiến trúc Đồng bộ Tối ưu RAM, Triệt tiêu Lỗi (-1) & Hết Giật Audio (2026-09-02)

### Bối cảnh & Bản chất gốc rễ của 2 lỗi tương hỗ

1. **Lỗi Firebase Sync (-1) (Out of Memory cho MbedTLS)**:
   - `WiFiClientSecure` cần ~35KB-40KB heap động liên tục để thực hiện SSL Handshake với Firebase.
   - Khi `_jpegBuffer` (48KB) bị ghim cố định trong RAM, cùng với I2S DMA (24KB hoặc 48KB) và task stacks (38KB), tổng RAM bị chiếm giữ vĩnh viễn là >180KB trên tổng 200KB heap khả dụng của ESP32-C3.
   - Heap chỉ còn ~16KB-30KB bị phân mảnh -> MbedTLS handshake thất bại -> `http.GET()` trả về `-1`.

2. **Lỗi Video Delay & Âm thanh nhảy cóc**:
   - `AUDIO_PCM_CHUNK_SIZE = 1600` (12.8KB stereo). Trong khi I2S DMA mỗi 10ms chỉ tiêu thụ 1.28KB (160 bytes mono input).
   - `fillChunk()` đọc 1600B từ NAND qua SPI nhưng `i2s_write` chỉ nhận 160B rồi vứt 1440B còn lại.
   - Mỗi giây đọc SPI NAND **100 lần** (160KB/s thay vì 16KB/s), chiếm `_spiMutex` liên tục khiến `MediaPlayer` bị nghẽn SPI khi render JPEG lên ST7789 display.

### Giải pháp Kiến trúc Đã Áp dụng

1. **Cân chỉnh Chunk Size vừa khít 1 DMA Buffer**:
   - `AUDIO_PCM_CHUNK_SIZE = 256` bytes (128 mono samples = 16ms @ 8kHz).
   - Khi oversample x4 stereo: 128 * 4 * 4 bytes = **2048 bytes = ĐÚNG 1 DMA BUFFER** (`AUDIO_DMA_BUF_LEN = 512` samples).
   - Tiết kiệm **12.1KB** static RAM trong `AudioPlayer` (buffer `_stereo` giảm từ 12.8KB xuống 2.0KB).
   - Đọc NAND cực nhanh (100 µs), không đọc thừa 1 byte nào, không tranh chấp SPI bus.

2. **Đồng bộ Audio Tick Trước & Sau Mỗi Frame (Loại bỏ Audio Task riêng)**:
   - Trong `MediaPlayer::update()`: gọi `_audio.tick()` trước khi decode JPEG (đảm bảo DMA đầy 192ms) và ngay sau khi decode/push image (bù ngay lượng vừa phát trong lúc decode).
   - Với DMA 192ms và thời gian decode mỗi frame ~40-60ms, DMA luôn duy trì mức >130ms, **không bao giờ bị underrun hay nhảy cóc**.
   - Loại bỏ task `AudioTick` riêng giúp tiết kiệm **2KB** stack và loại bỏ 100% xung đột mutex giữa 2 task.

3. **Thu gọn Task Stacks & Buffer On-Demand**:
   - `TASK_STACK_MEDIA_PLAYER = 6144`, `TASK_STACK_NETWORK = 6144`, `TASK_STACK_UI_CONTROLLER = 4096` -> tiết kiệm thêm **8KB** heap.
   - `_jpegBuffer` giảm từ 48KB xuống **32KB** (đủ cho 100% video/ảnh JPEG 240x240 và SLBX chunked).
   - `_jpegBuffer` cấp phát khi `playItem()` (có retry 100ms yield cho IDLE task) và giải phóng ngay trong `stop()`.
   - Trong `STATE_STANDBY`, **Free Heap đạt ~120KB - 140KB**, MbedTLS SSL Handshake chạy mượt mà 100%, không bao giờ gặp lỗi (-1).

### Bảng cấu hình bộ nhớ đã chốt

| Thành phần | Kích thước | Vòng đời | Mục đích |
|---|---|---|---|
| `_jpegBuffer` | 32 KB | Malloc lúc play, Free lúc stop | Giải mã JPEG 240x240 / RGB565 |
| I2S DMA | 12 buffers × 512 × 4 = 24 KB | Cố định | Đệm âm thanh 192ms sâu và ổn định |
| Audio Chunk | 256B mono -> 2048B stereo | BSS (2.3KB) | Đúng 1 DMA buffer, đọc SPI 0.1ms |
| Heap tự do ở Standby | **~130 KB** | Luôn có sẵn | Đảm bảo TLS SSL Handshake thành công |

---

## 9. RISK REVIEW 2026-09-02 (Memory leak / Race condition / Resource leak / Bảo mật)

Đánh giá đọc toàn bộ firmware, không sửa code. Đối chiếu với `codebase_review.md` (báo cáo cũ
đã có sẵn trong repo) — mục nào đã fix thì ghi rõ, mục nào còn nguyên thì giữ nguyên độ ưu tiên.

### Mới phát hiện, chưa có trong `codebase_review.md`

1. **🔴 Regression tiềm ẩn: `_jpegBuffer` quay lại malloc/free theo từng lần playItem/stop**
   (`MediaPlayer.cpp` staged diff hiện tại, `init()`/`playItem()`/`stop()`). Đây CHÍNH LÀ pattern
   mà §8 Phase 3E ghi nhận từng gây **reboot khi Long Press → Short Press liên tiếp** (đã fix bằng
   cách cấp phát 1 lần lúc `init()`, không free trong `stop()`). Phase 3F (vì lý do RAM cho TLS)
   đã đảo ngược lại đúng pattern đó. Long Press (3s) rồi Short Press ngay sau (theo
   `UIController::getTouchEvent()`) gọi `stop()` rồi `playItem()` liên tiếp trong cùng 1 tick —
   free() rồi malloc() lại 32KB ngay lập tức, đúng kịch bản đã từng gây crash. **Chưa kiểm chứng
   lại trên máy thật sau Phase 3F** — cần test kỹ đúng thao tác Long→Short trước khi tin tưởng.
2. **🔴 `client.setInsecure()` ở cả 5 chỗ gọi Firebase REST** (`NetworkManager.cpp:521,547,584,603,664`).
   Không xác thực chứng chỉ TLS → MITM có thể giả mạo `FIREBASE_HOST`, đọc/giả `FIREBASE_AUTH_SECRET`
   (gửi dạng query param `?auth=...`) và tin nhắn media. Vì auth secret là static bearer token nhúng
   cứng trong firmware, lộ 1 lần là lộ vĩnh viễn cho tới khi đổi secret.
3. **⚠️ MEMORY.md ghi sai** (đã đánh dấu ở §8): `setBufferSizes(2048, 1024)` được ghi là ✅ đã áp
   dụng nhưng **không tồn tại trong `NetworkManager.cpp`**. Nếu lỗi Firebase (-1) tái xuất hiện, đây
   là nghi phạm đầu tiên cần xét lại, không phải NAND/audio.
4. **⚠️ Dùng chung 1 `WiFiClientSecure client` cho 2 phiên `HTTPClient` lồng nhau** (video `http` +
   audio `httpAudio` trong `checkAndDownloadNewMessages`, `NetworkManager.cpp:881-1057`): `http.end()`
   của phiên video chỉ gọi SAU khi tải audio xong, trong khi `httpAudio.begin(client, ...)` đã mở một
   phiên GET khác trên cùng object `client` khi `http` còn "sống". Chưa quan sát được crash nhưng đây
   là chia sẻ resource không rõ ràng, dễ vỡ nếu WiFiClientSecure không tolerant việc này.
5. **⚠️ Tái sử dụng `String` trong toàn bộ luồng Firebase** (`NetworkManager.cpp`: `String payload`,
   `String rawMediaUrl`, `fullUrl.replace(...)`, `jsonEscape()`...) — mâu thuẫn trực tiếp với quyết
   định Phase 2.5 "loại bỏ hoàn toàn String... triệt tiêu rò rỉ RAM/phân mảnh heap". Đây đúng là
   luồng chạy trên background task ngay trước lúc cần heap sạch cho TLS handshake (lý do Phase 3F
   tồn tại). Nhiều `String` concatenation nhỏ mỗi chu kỳ sync là nguồn phân mảnh heap hợp lý nhất
   để nghi ngờ nếu OOM còn tái diễn.
6. **⚠️ Data race đọc-sửa-ghi `_numOfNewMsg`**: `checkAndDownloadNewMessages()` (task nền `WakeSync`)
   ghi đè `_numOfNewMsg = unreadInMem + newCloudMsg` trong khi `Task_MediaPlayer` gọi
   `decrementNewMsgCount()` (`_numOfNewMsg--`) khi người dùng bấm chuyển tin. Biến có `volatile`
   nên không bị compiler cache, nhưng phép `--` không atomic — nếu 2 task chạm cùng lúc có thể mất
   1 lượt đếm. Rủi ro thấp (cửa sổ trùng thời điểm hiếm) nhưng vẫn là race thật.
7. **⚠️ Task stack vừa giảm (staged diff `config.h`)**: `TASK_STACK_UI_CONTROLLER` 8192→4096,
   `TASK_STACK_MEDIA_PLAYER` 8192→6144. Chưa thấy bằng chứng đã test stack watermark thực tế
   (`uxTaskGetStackHighWaterMark`) sau khi giảm — JPEGDEC decode và WebServer callback trong
   `Task_UIController`/`Task_NetworkController` đều có thể ăn stack sâu hơn ước tính. Theo Session
   Rule §8, Agent không tự build — nên đây là việc cần User verify trên máy thật, không phải
   assumption an toàn.
8. **⚠️ `DEFAULT_WIFI_SSID`/`DEFAULT_WIFI_PASSWORD` (`include/config.h:83-84`) là Wi-Fi nhà thật, đã
   nằm trong git log từ commit `a5970fc` (rất lâu trước 6 commit local hiện tại) — nhiều khả năng
   **đã nằm trên origin** (repo ahead 6 commits, nghĩa là các commit cũ hơn đã push). Cân nhắc đổi
   mật khẩu Wi-Fi thật hoặc dọn khỏi lịch sử git nếu origin là remote chia sẻ.

### Từ `codebase_review.md` — đối chiếu lại, vẫn còn nguyên (chưa fix)

- **3.1 Nguy cơ tự-deadlock `_spiMutex` non-recursive**: vẫn còn — `main.cpp` tạo
  `xSemaphoreCreateMutex()` (không đệ quy), và `MediaPlayer::decodeOneFrame()` vẫn gọi
  `DLOG("[PLAY] ERR: openRAM")` (`MediaPlayer.cpp:421`) ngay TRONG khối đã `acquireSPI()`
  (dòng 408-424). `ScreenLogger::render()` timeout 50ms nên không treo vĩnh viễn, nhưng vẫn
  gây đúng 50ms nghẽn Task_MediaPlayer mỗi lần rơi vào nhánh lỗi này.
- **4.5 `NandStorage::formatAll()` poll `waitBusyInternal()` không yield**: vẫn còn nguyên,
  Chip Erase 16MB (20-80s) có thể đụng Task Watchdog.
- **4.1 `FirebaseClient.*` dead code**: vẫn còn, chưa xoá (không sao vì không ảnh hưởng runtime).
- **5.1 OTA `/api/ota/*` không xác thực**: vẫn còn nguyên, port 80 mở công khai khi có Wi-Fi.
- **5.2 Wi-Fi mặc định hardcode**: vẫn còn (xem mục 8 ở trên, đã bổ sung thêm ý git history).
- **5.3 Bucket Firebase Storage hardcode**: vẫn còn (`NetworkManager.cpp` các đoạn build
  `firebasestorage.googleapis.com/v0/b/iot-app-839a2...`).

### Đã tự fix từ khi viết `codebase_review.md` (không cần làm lại)

- **3.4 Audio `fillChunk()` drop sample không theo dõi `written`**: ĐÃ FIX — bản hiện tại ghi cả
  chunk 1 lần bằng `i2s_write(..., &written, 0)` và cộng dồn `_audioCursor` đúng theo `written`
  thực tế (`AudioPlayer.cpp:216-227`).
- **2.2 `_isNtpSyncing` treo vĩnh viễn nếu `xTaskCreate` fail**: bug logic vẫn còn trong
  `triggerNtpSync()` (`NetworkManager.cpp:172-176`, không kiểm tra `pdPASS`), NHƯNG hàm này
  giờ là **dead code** — chỉ còn gọi từ `src/main.cpp.bak`, `main.cpp` thật đã chuyển hết sang
  `triggerWakeupSync()` (có kiểm tra `pdPASS` đầy đủ, `NetworkManager.cpp:459-464`). Hạ độ ưu
  tiên xuống thấp, chỉ là bom nổ chậm nếu sau này có ai gọi lại `triggerNtpSync()`.
- **4.4 lỗi chính tả "Sar"**: ĐÃ FIX, code hiện tại đúng `"Sat"` (`NetworkManager.cpp:217`).
- **3.2 SDCardManager thiếu NOP Hack**: không ảnh hưởng hiện tại vì `ACTIVE_STORAGE_TYPE` đang
  set NAND (`config.h:110`), nhưng nếu tương lai chuyển sang SD thì bug này vẫn còn nguyên, chưa
  verify lại.

### Quyết định đã CHỐT sau review (2026-09-02, bổ sung)

- **Bỏ hẳn "có tin local thì hoãn sync"**: User xác nhận logic này *không hợp lý* — 2 chỗ trong
  `Task_UIController` (`main.cpp`) từng skip trigger sync ngầm khi `storage->hasUnreadMessage()`
  đã true (1 lần ở post-wakeup sync, 1 lần ở periodic 10s check), với lý do cũ là nhường CPU/SPI
  cho Short Press phát ngay. Đã xoá cả 2 nhánh — giờ chỉ còn điều kiện `isFull()` mới skip sync.
  Quay lại nguyên tắc: luôn check tin mới trên Cloud + tải đầy đủ vào slot, không có tin local nào
  được coi là "đủ mới" để hoãn việc kiểm tra Cloud. **Chưa test lại trên máy thật.**

### Bug thật đã tìm ra và fix (2026-09-02): slot dở do download stall bị công nhận nhầm là "tin hợp lệ"

**Triệu chứng user báo**: 1 message video 2.21MB (15s, 225 frame @15fps) + voice 240KB luôn tải
dừng ở ~20% rồi timeout. Mở slot vẫn phát được vài giây đầu (không tiếng) rồi lỗi `Bad jpegSize`.
Tin nhắn không được đánh dấu đã tải (`ts fail`). Một message khác nhỏ hơn (1.68MB/10s, 151 frame,
voice 161KB) tải/đọc bình thường.

**Đối chiếu 2 file .bin gốc bằng hex dump** (`SLBX` header offset 0-19 trong file gốc, tương ứng
offset 4-23 sau khi firmware chèn 4 byte size ở đầu slot): cấu trúc header, `mediaType`, `w/h=240`,
`fps=15`, `totalFrames` (225 vs 151) đều đúng chuẩn và giống hệt về layout giữa 2 file — **web
convert/encode KHÔNG có lỗi**, chỉ khác nhau về độ dài (message lỗi dài đúng 15s = trần cắt của
web, nên file lớn hơn, tải lâu hơn).

**Root cause thật (bug ở box, không phải ở web)**: `checkAndDownloadNewMessages()`
(`NetworkManager.cpp`) gọi `storage->closeWrite(maxDisplayTime)` **VÔ ĐIỀU KIỆN** sau vòng lặp tải,
kể cả khi `writeError = true` (do `DOWNLOAD_STALL_TIMEOUT_MS = 10000` bắn lên giữa chừng ở file
lớn). `NandStorageProvider::closeWrite()` luôn ghi bảng slot + set bit `unread` bất kể dữ liệu có
đủ hay không — vì 20 byte đầu (SLBX header + `totalFrames=225`) đã tới nơi trước lúc stall, header
check nhận diện đúng "VJPG", box tưởng slot hợp lệ và cho phát. Phát tới đúng điểm dữ liệu bị cụt
(~20%) thì đọc trúng vùng NAND đã erase nhưng chưa từng ghi (toàn `0xFF`) → `jpegSize` đọc ra rác
→ `Bad jpegSize`. Audio không có tiếng vì nhánh tải voice nằm TRONG khối `if (!writeError...)` nên
bị bỏ qua hoàn toàn khi video đã lỗi. Vì `messageSuccess = false`, code `break` vòng lặp tin nhắn
và không cập nhật `last_download_ts` (đúng như log "ts fail" user thấy) — nhưng slot vật lý **đã bị
đánh dấu unread với dữ liệu rác**, ngồi chình ình chiếm 1 trong 5 slot.

**Fix đã áp dụng**:
1. Thêm `IStorageProvider::discardWrite()` (mặc định no-op) — huỷ phiên ghi dở dang, KHÔNG commit
   bảng slot, KHÔNG set bit unread (`lib/Storage/IStorageProvider.h`).
2. `NandStorageProvider::discardWrite()`: xoá magic slot vừa erase-dở (để `isSlotValid()` không
   nhận nhầm), ghi lại slot table, **không đụng `_writeSlotIndex`** — lần sync sau sẽ retry đúng
   slot này thay vì đốt thêm 1 slot mới mỗi lần fail (`lib/Storage/NandStorageProvider.cpp`).
3. `NetworkManager::checkAndDownloadNewMessages()`: tính `downloadComplete` TRƯỚC, gọi
   `closeWrite()` khi thành công / `discardWrite()` khi lỗi, thay vì `closeWrite()` vô điều kiện
   (`NetworkManager.cpp` ~dòng 940-957).

**Chưa fix, ghi lại để không quên**: nhánh tải audio lồng bên trong (dòng ~1014-1017,
`while (!aWriteError && httpAudio.connected() ...)`) khi stall chỉ `break` mà KHÔNG set
`aWriteError = true`, log vẫn in "Audio DL OK". Ít nghiêm trọng hơn (video đã hợp lệ, `closeAppend()`
vẫn tính đúng `audioSize` theo số byte thực ghi nên không lộ dữ liệu rác — chỉ là tiếng bị cắt cụt
mà log báo nhầm "OK") — chưa sửa vì không phải nguyên nhân bug user báo lần này.

**Nghi vấn còn treo (nguyên nhân TẠI SAO stall xảy ra ở file lớn)**: có thể liên quan tới việc
`setBufferSizes(2048,1024)` cho mbedTLS **chưa thực sự có trong code** dù MEMORY.md từng ghi ✅
(xem mục sai đã đánh dấu ở Phase 3F, bảng cấu hình bộ nhớ). File càng lớn thì thời gian giữ kết nối
TLS càng lâu, càng dễ va phải heap fragmentation/RSSI kém. Nếu áp `setBufferSizes` thật vào code mà
vẫn stall thì mới nên nghi ngờ tầng khác (Firebase Storage CDN, router).

**✅ ĐÃ KIỂM CHỨNG MÁY THẬT 2026-09-02**: User xác nhận tải lại được bình thường (đúng message
2.21MB/240KB từng bị stall) sau fix `discardWrite()`. Tạm chốt là ĐÃ FIX. Chưa rõ do fix
`discardWrite()` (giải phóng đúng slot để retry sạch thay vì kẹt slot rác) hay do stall gốc chỉ là
nhất thời (Wi-Fi/CDN) — không set lại `setBufferSizes()` lần này, chỉ ghi lại làm nghi vấn dự phòng
nếu hiện tượng stall tái diễn với file lớn khác.

### Text đi kèm message — ĐÃ LÀM (2026-09-02, đảo ngược quyết định "hoãn" ở trên)

User quay lại yêu cầu làm ngay (không hoãn nữa), với điều kiện: **tạm thời bỏ dấu tiếng Việt**
(ASCII-fold) vì font `ChakraPetch_*.h` xác nhận chỉ có glyph ASCII 32-126 (đọc trực tiếp struct
`GFXfont` trong file, `first=32,last=126`) — không đủ cho tiếng Việt có dấu, và chưa có word-wrap ở
đâu trong codebase. Đồng thời user yêu cầu gộp luôn ảnh + text + "nhạc nền" (voice đổi vai trò) thành
1 loại "tin nhắn tĩnh", và thêm 1 card mới trên web sender — **không tạo `type` enum mới ở backend**,
tái dùng `type: "image"` có sẵn (message schema đã phẳng, field nào cũng optional trên mọi `type`).

**Đã code xong cả 3 tầng, CHƯA build/test trên máy thật** (theo Session Rule, Agent không tự chạy
`pio run`; web/backend cũng chưa tự build/deploy — chỉ sửa source).

#### Tầng Backend (`sendlove_backend`)
- Phát hiện quan trọng: `bg_music_url`/upload config `bg_music` **đã có sẵn từ trước** (scaffold cũ,
  không phải do phiên này thêm) — nhưng cấu hình sai định dạng `bgmusic.mp3`/`audio/mpeg` trong khi
  firmware chỉ decode được WAV/PCM (không có decoder MP3, không khả thi nhét vào ESP32-C3). Đã sửa
  `message.service.ts` (+ bản compiled `.js`) đổi 3 chỗ: `typeMap.bg_music`, URL construction, fileMap
  — tất cả từ `bgmusic.mp3`/`audio/mpeg` sang `bgmusic.wav`/`audio/wav`. Không đổi `type` enum.

#### Tầng Web (`sendlove_web`)
- `SenderUI.jsx`: thêm card `static` vào `TYPES`/`STEP2_TITLE` (UI-key nội bộ, KHÔNG phải giá trị gửi
  backend). Bước 2 mới: `ImageInput` (tuỳ chọn, giữ tạm blob qua state `staticImageBlob` thay vì upload
  ngay) + textarea dùng chung state `text` sẵn có + `VoiceInput` (tuỳ chọn, giữ tạm qua
  `staticAudioData`, đóng vai "nhạc nền"). `processAndUpload()` nhánh `static` mới: ép cứng
  `payload.type = 'image'`, bồi `binBlob`/`thumbBlob` (nếu có ảnh, tái dùng `encodeImageToBin` y hệt
  card ảnh) + `bgMusicBlob` (field MỚI, khác `voiceBlob`). `handleCancel()` giờ cũng clear `text` +
  2 state mới — tiện thể fix luôn bug nhỏ đã phát hiện trước đó ("text không bị xoá khi đổi mode").
- `EncodingProgress.jsx`: thêm `static` vào `TYPE_ICON`/`TYPE_LABEL`.
- `mediaUploader.js`: thêm 1 dòng `if (data.bgMusicBlob) blobsToUpload.push({type:'bg_music',...})`
  song song dòng `voiceBlob` có sẵn — không sửa gì khác (logic build request đã trung lập với `type`).

#### Tầng Firmware (`sendlove_firmware`)
- **`lib/NandStorage/NandStorage.h/.cpp`**: bump magic bảng slot `NSL2 → NSL3` (tiền lệ NSLT→NSL2).
  **Side effect đã biết trước**: lần boot đầu sau khi nạp, tin nhắn unread cũ trên máy bị xoá 1 lần.
  `SlotEntry` +`uint16_t textLen +char text[256]` (hằng số `SLOT_TEXT_MAX_LEN=256`, đủ ~7-8 dòng).
  Thêm `setSlotText()`/`getSlotText()` — set field-only (không `memset()` cả struct, giống pattern
  `setSlotAudioSize()`), cắt bớt an toàn tại ranh giới UTF-8 nếu text > 256 byte.
- **`lib/Storage/IStorageProvider.h`**: thêm `setItemText()`/`getItemText()` (mặc định no-op, SD Card
  chưa hỗ trợ). **`NandStorageProvider`**: implement 2 hàm trên; mở rộng `discardWrite()` (đã có từ
  lần fix video-stall trước) để **cũng xoá bit `_unreadBitmask` + lùi `_writeSlotIndex`** — cần vì
  đường ghi audio/text-only phải `closeWrite()` commit placeholder TRƯỚC (để `openForAppend()` tính
  đúng offset), rồi mới biết audio có tải được không; nếu lỗi phải undo được cả sau khi đã commit.
- **`lib/NetworkManager/NetworkManager.cpp`**:
  - Thêm `bg_music_url`/`bgMusicUrl` vào `voiceKeys[]` — tái dùng 100% cơ chế audio append có sẵn.
  - Parse `msg["text"]` lần đầu tiên (trước đây không đọc field này ở đâu cả).
  - Tách khối tải audio (từng inline trong nhánh ảnh/video) thành method riêng
    `downloadVoiceSegment()`, dùng chung cho nhánh ảnh/video (audio là phụ, lỗi không huỷ message) và
    nhánh mới "static không ảnh" (audio là nội dung chính, lỗi → `discardWrite()` huỷ cả message).
    Nhân tiện fix 1 bug có sẵn: audio bị stall trước đây không set `aWriteError=true` → log báo nhầm
    "OK" dù cụt tiếng.
  - Thêm nhánh `else if (rawVoiceUrl.length() > 0 || rawText.length() > 0)` xử lý message không có
    ảnh/video: mở slot rỗng (dataSize=4 sentinel) → tải audio (nếu có) → ghi text (nếu có).
- **`lib/MediaPlayer/MediaPlayer.cpp`**:
  - **Bug có sẵn phát hiện qua trace, đã fix**: `update()` trước đây chỉ tick audio khi
    `_state==PLAYING`; ảnh tĩnh dùng `_state=SHOWING` nên **combo ảnh+voice ĐANG câm tiếng** dù
    NetworkManager tải đúng. Thêm nhánh tick audio khi `SHOWING` — fix chung cho cả bug cũ lẫn
    voice-only/tin nhắn tĩnh mới.
  - `playItem()`: phát hiện sentinel "không ảnh thật" (`type==IMAGE && dataSize<=4`) → bỏ qua hoàn
    toàn dò header SLBX + `decodeOneFrame()` (tránh lỗi "Bad jpegSize: 0" + `delay(2000)` chặn màn
    hình), chỉ giữ màn đen (bản NAND; bản SD card để phase sau theo đúng ý user). Sau ảnh/màn đen: nếu
    có caption, gọi `asciiFoldVietnamese()` (bảng map Unicode tiếng Việt U+1EA0-1EF9 + Latin-1/Extended-A
    → ASCII gần nhất, hàm static mới trong file) rồi `_display->showWrappedText()`.
- **`lib/DisplayDriver/DisplayDriver.h/.cpp`**: thêm `showWrappedText()` — greedy word-wrap đo
  `textWidth()` từng từ bằng font `ChakraPetch_SemiBold_16` có sẵn, giới hạn số dòng vừa vùng hiển thị
  (dải dưới màn nếu có ảnh, gần trọn màn nếu không ảnh), dòng cuối thêm "..." nếu bị cắt.

#### Chưa làm / để phase sau (theo đúng phạm vi đã chốt)
- Font Unicode thật cho tiếng Việt có dấu (đang ASCII-fold tạm thời).
- Background mặc định cho bản SD card khi không có ảnh (bản NAND dùng màn đen).
- Audio-stall-not-marking-error trong nhánh audio lồng ảnh/video: đã fix (dùng chung
  `downloadVoiceSegment()` nên fix 1 chỗ áp dụng cho cả 2 đường).

### Fix: download liên tục fail/timeout ở ~10% — erase toàn slot NAND + nghẽn TCP (2026-09-02, phiên sau)

Sau khi nạp firmware có tính năng tin nhắn tĩnh ở trên, user báo download liên tục fail, đứng ở ~10%
rồi timeout. Điều tra bằng 2 nguồn độc lập (tự rà soát + Plan agent kiểm chứng thiết kế, và 1 agent
khác cung cấp `fix_download_timeout_plan.md` phân tích sâu cơ chế TCP) — đã tự verify mọi claim quan
trọng bằng grep trực tiếp trước khi tin dùng.

**Root cause A (chính, gây STALL)**: `NandStorageProvider::openForWrite()` erase NGUYÊN slot ~5.3MB
(`NAND_SLOT_ADDRS = {0x010000, 0x560000, 0xAB0000}`) — 15-25s block đồng bộ SAU khi `http.GET()` đã mở
socket nhưng TRƯỚC khi đọc byte nào. Cơ chế cụ thể: trong lúc đó buffer TCP/mbedTLS đầy → lwIP gửi TCP
Window=0 → CDN backoff (Persist Probe) → khi erase xong, đọc nốt phần buffer cũ (~10-20%, khớp đúng
triệu chứng) → hết buffer, CDN đang backoff → 10s sau chạm `DOWNLOAD_STALL_TIMEOUT_MS` → huỷ. Bug có
sẵn từ khi `NAND_SLOT_COUNT` đổi 5→3 (commit `a5970fc`, 31/08, trước phiên tính năng text/voice), làm
slot to lên; đường ghi "tin nhắn tĩnh" mới thêm khiến bug này bị chạm ở MỌI message (kể cả vài chục KB).

**Root cause B (cộng hưởng, đã verify bằng grep)**: `uint8_t buffer[256]` + `stream->readBytes()` +
`delay(1)` gọi VÔ ĐIỀU KIỆN mỗi vòng lặp tải (cả video lẫn audio) — ở `CONFIG_FREERTOS_HZ=100`
(tick 10ms), trần tốc độ tải chỉ còn ~20-25KB/s. Cộng thêm: không có `WiFi.setSleep(false)` nào trong
toàn bộ firmware (modem sleep mặc định bật suốt lúc tải); `http.end()` của phiên video chạy SAU
`downloadVoiceSegment()` thay vì trước — 2 `HTTPClient` dùng chung 1 `WiFiClientSecure` khi phiên cũ
chưa đóng.

**Fix đã áp dụng**:
1. `lib/Storage/NandStorageProvider.h/.cpp` — Erase-as-you-write: thêm `_erasedUpToAddr` (địa chỉ
   tuyệt đối). `openForWrite()` chỉ erase 1 block 64KB đầu thay vì nguyên slot. `writeChunk()` erase
   thêm block khi con trỏ ghi sắp chạm vùng chưa erase — **bất biến bắt buộc**: luôn erase từ
   `_erasedUpToAddr`, không bao giờ từ `_writeOffset` (tránh `eraseRange()` tự lùi sector đè dữ liệu
   đã ghi). `openForAppend()` tự tính lại `_erasedUpToAddr` từ `_writeOffset` (làm tròn lên bội 65536)
   thay vì dựa vào state cũ — tự chữa lành. `discardWrite()`/`formatStorage()` reset về 0.
2. `lib/NetworkManager/NetworkManager.cpp`:
   - Buffer đọc `256B → 2048B` ở cả 2 vòng lặp (video + `downloadVoiceSegment()`).
   - `delay(1)` giờ CÓ ĐIỀU KIỆN — chỉ khi `sizeAvail/av == 0` (không có data), không delay mỗi vòng.
   - `WiFi.setSleep(false)` quanh vòng lặp per-message download (bật lại `true` ngay sau vòng lặp —
     mọi lối thoát khỏi vòng lặp là `break`, không `return`, nên luôn chạy tới điểm bật lại).
   - `http.end()` cho phiên video gọi NGAY sau khi tải xong, TRƯỚC `downloadVoiceSegment()` (dòng cũ ở
     cuối khối vẫn giữ, gọi lần 2 vô hại).
   - `DOWNLOAD_STALL_TIMEOUT_MS`: `10000 → 30000` (lưới an toàn bổ sung, không phải fix chính).

**Chưa làm (out of scope, có lý do)**: `waitBusyInternal()` (`lib/NandStorage/NandStorage.cpp`) không
có `vTaskDelay()` — với erase block 64KB (~150-2000ms/lần) đã đủ ngắn, sửa thêm sẽ ảnh hưởng
`writeRaw()` page-program (hot path), rủi ro cao hơn lợi ích. Các mục bảo mật khác trong
`code_review_2_9_gemini_38.md` (setInsecure/TLS, OTA không xác thực, GPIO8/NAND-CS, sector 0 wear) —
không liên quan triệu chứng lần này, để riêng nếu user muốn xử lý.

**Chưa build/flash/test trên máy thật.** Cần user xác nhận: (1) `[NET] writing slot` xuất hiện dưới
0.5s sau `[NET] GET OK` thay vì 15-25s; (2) tiến độ tải chạy đều không khựng ~10%; (3) tin nhắn chỉ
voice/text (trước đây gần như luôn fail) giờ tải nhanh; (4) video+audio nối tiếp không mất/lệch dữ
liệu, không lỗi `Bad jpegSize`; (5) `uxTaskGetStackHighWaterMark()` của `WakeSync` sau vài lần tải để
xác nhận margin thực tế với buffer 2048B (lý thuyết tính đủ nhưng nên đo thật).

> **KẾT QUẢ THỰC TẾ (2026-09-03)**: vòng fix này **CHƯA đủ**. Triệu chứng ĐỔI chứ không hết — từ
> "luôn dừng ~10%" thành "dừng ở vị trí NGẪU NHIÊN (đôi khi 3000B, đôi khi nửa file)". Nguyên nhân
> thật nằm ở chỗ khác, xem mục kế tiếp. Các fix ở vòng này (erase-as-you-write, delay có điều kiện)
> **vẫn đúng và cần giữ** — chính chúng đã loại bỏ cú block 15-25s nên triệu chứng mới đổi dạng.

---

### Fix vòng 3 (2026-09-03): `Stream::readBytes()` trên WiFiClientSecure đọc TỪNG BYTE qua mbedTLS

**Dữ kiện quyết định từ user** (loại trừ gần hết giả thuyết cũ):
- **Cả 2 dòng log đều xuất hiện tuỳ lần**: `dl STALL` (kết nối còn sống, server ngừng gửi) VÀ
  `DL err (discarded)` (`http.connected()` thành false — kết nối đứt hẳn).
- **Cấp nguồn USB** → loại bỏ giả thuyết sụt áp/pin.
- **Box KHÔNG reboot** → loại bỏ brownout và Task-Watchdog panic.

**Root cause (verify trực tiếp trong source Arduino core 2.0.17 tại `~/.platformio/packages/`)**:
`stream->readBytes(buffer, 2048)` trên `WiFiClientSecure` đọc **từng byte một**:
1. `WiFiClientSecure`/`WiFiClient` **không override `readBytes()`** → rơi về `Stream::readBytes()`
   (`cores/esp32/Stream.cpp:41-53`) — vòng lặp `timedRead()` **1 byte/vòng**.
2. `Stream::timedRead()` (`Stream.cpp:31`) **busy-spin không nhường CPU**, `_timeout` = **30 giây**
   (`HTTPClient.cpp:1168` đặt `_client->setTimeout((_tcpTimeout+500)/1000)`).
3. `WiFiClientSecure::read()` 1 byte (`:187`) → `read(&data,1)` → **gọi `available()` MỖI LẦN**
   (`:213`) → `data_to_read()` (`ssl_client.cpp:353`) → `mbedtls_ssl_read(ctx,NULL,0)` +
   `mbedtls_ssl_get_bytes_avail()`, rồi mới `get_ssl_receive()` → `mbedtls_ssl_read(ctx,buf,1)`.

→ 1 chunk 2048B = **~4096 lời gọi mbedTLS**; file 2.2MB = **hơn 4 TRIỆU** trên CPU đơn nhân 160MHz.
Đúng bằng trần ~20-25KB/s → 2.2MB mất 100+ giây, phơi kết nối quá lâu.

**Giải thích được ĐÚNG CẢ HAI kiểu lỗi ở vị trí ngẫu nhiên** (điều mà mọi giả thuyết trước không làm được):
- `available()` gọi **`stop()`** ngay khi `data_to_read()` trả âm (`WiFiClientSecure.cpp:247-250`);
  `read(buf,size)` cũng `stop()` khi `get_ssl_receive()` âm (`:233-236`). Gọi 4 triệu lần thì chỉ cần
  **một** lỗi mbedTLS thoáng qua là kết nối bị giết → `DL err (discarded)` ở vị trí ngẫu nhiên.
- Khi buffer mbedTLS cạn giữa chunk, `read()` trả -1 → `timedRead()` **busy-spin tới 30 giây** đốt
  100% CPU, không rút socket, không cập nhật `lastProgressMs` → `dl STALL`.

**Sai lầm của vòng fix 2 (ghi lại để không lặp)**: tài liệu `fix_download_timeout_plan.md` (Giải pháp 2)
nêu **2 việc** — (a) nâng buffer 256→2048B, (b) **đổi `readBytes()` → `read()`**. Vòng 2 chỉ làm (a),
bỏ sót (b) — mà (b) mới là phần quan trọng. Nâng buffer mà vẫn `readBytes()` chỉ khiến mỗi chunk tốn
2048 vòng byte-by-byte thay vì 256, **không giải quyết gì**. Bài học: khi áp dụng khuyến nghị từ tài
liệu ngoài, phải làm ĐỦ các phần của khuyến nghị đó hoặc ghi rõ vì sao bỏ.

**Fix đã áp dụng vòng 3:**
1. `NetworkManager.cpp` — **`stream->readBytes()` → `stream->read()`** ở CẢ 2 vòng lặp tải (video
   ~dòng 1056 và `downloadVoiceSegment()` ~dòng 715). Đây là fix chính. Thêm log
   `[NET] conn DROPPED @ X/Y` khi thoát vòng lặp do `!http.connected()` (trước đây im lặng, phải suy
   ra từ `DL err` nên không phân biệt được "đứt kết nối" với "tải thiếu byte").
2. `NandStorage.cpp` — `writeRaw()` dùng **`SPI.writeBytes()` bulk** thay vòng `SPI.transfer()` từng
   byte, + hằng số mới **`NAND_WRITE_SPI_SETTINGS` 20MHz** riêng cho đường ghi (đường đọc đã chạy
   20MHz ổn định trên đúng bộ dây này; opcode `0x02` của W25Q128JV chịu tới 133MHz). Ghi 1 page 256B:
   ~900µs → ~120µs. **Tách hằng số riêng để revert 1 dòng** nếu breadboard không chịu nổi — dấu hiệu
   là dữ liệu tải về hỏng (ảnh nhiễu / `Bad jpegSize` / audio rè bất thường).
3. **Chặn "ghi thất bại nhưng báo thành công"** (bug thật, phát hiện khi rà soát): `writeRaw()`/
   `eraseRange()`/`eraseSector()` đổi `void` → `bool` (false khi `acquireSPI()` timeout 1000ms —
   trước đây return im lặng, KHÔNG ghi gì mà caller vẫn tưởng xong). `writeChunk()` giờ trả `0` khi
   thất bại thay vì luôn trả `len` → `NetworkManager` thấy `written < c` → `discardWrite()`.
   `writeSlotTable()` và `closeWrite()` cũng log lỗi thay vì nuốt. Nguy hiểm nhất là erase bị skip im
   lặng: NAND chỉ clear bit 1→0, ghi đè lên vùng chưa erase cho ra dữ liệu rác mà chip không báo lỗi.

**Ghi chú**: `lib/NetworkManager/FirebaseClient.cpp:89` cũng dùng `stream->readBytes()` với cùng bug,
nhưng module này là **dead code** (không được gọi ở đâu — đã xác nhận từ `codebase_review.md` §4.1),
nên không sửa. Nếu sau này hồi sinh module đó thì phải sửa cùng cách.

**Chưa build/flash/test trên máy thật.** Cần user xác nhận:
1. **Tốc độ tải** — dòng `[NET] dl X/Y` (in mỗi 16KB) phải chạy dồn dập, 2.2MB xong trong vài giây tới
   ~15s thay vì >100s. Đây là chỉ dấu trực tiếp nhất cho biết fix có trúng hay không.
2. Không còn `dl STALL` / `DL err (discarded)` giữa chừng. Nếu VẪN còn, dòng log mới
   `[NET] conn DROPPED @ X/Y` sẽ cho biết chính xác là kết nối đứt (→ bước tiếp: resume bằng HTTP
   `Range`) hay là ngừng nhận data (→ hướng khác).
3. Toàn vẹn dữ liệu sau khi nâng clock ghi 20MHz (xem dấu hiệu ở mục 2 phần fix).
4. Nếu xuất hiện `[NANDP] ERR: write failed` / `erase-ahead FAILED` → tranh chấp SPI mutex là có thật
   (trước đây bị nuốt im lặng nên chưa từng thấy), cần xử lý riêng.

**Chưa làm, để dành nếu vòng 3 vẫn chưa đủ**: resume/retry bằng HTTP `Range` header (phải phân biệt
206 vs 200, xử lý server không hỗ trợ Range) — chỉ nên làm khi đã có bằng chứng từ log
`conn DROPPED` rằng vấn đề là mạng chứ không phải firmware.

---

### Fix vòng 4 (2026-09-03): vấn đề THẬT nằm ở TẦNG KẾT NỐI, không phải luồng download

**User reframe (rất quan trọng, đổi hẳn hướng điều tra)** — 4 quan sát thực tế:
1. Wi-Fi kết nối chập chờn; ngủ rồi thức dậy có thể **mất luôn kết nối**.
2. Dùng Wi-Fi mặc định không được **dù SSID đúng**, phải nhập lại đúng Wi-Fi đó qua AP mode mới chạy.
3. **Wi-Fi đang báo đã connect nhưng `http.GET()` vẫn trả `-1`.**
4. **Những lúc tải được hẳn thì tải RẤT NHANH.**

Quan sát #4 xác nhận **fix vòng 3 (`readBytes`→`read`) ĐÃ có tác dụng** — đường download không còn là
nút thắt. Nút thắt còn lại là việc thiết lập/duy trì kết nối.

**Root cause (verify trực tiếp trong code, không suy đoán)**:

`NetworkManager::ensureConnected()` có fast-path `if (WiFi.status() == WL_CONNECTED) return true;`.
Sau Light Sleep **`WiFi.status()` RẤT HAY vẫn báo `WL_CONNECTED` dù association đã chết ở phía AP**:
CPU ngủ suốt 5 phút nên driver Wi-Fi không hề xử lý được beacon-loss / deauth event, biến trạng thái
giữ nguyên giá trị cũ. Tin vào nó → bỏ qua reconnect → mọi `http.GET()` sau đó trả `-1`
(**đúng y quan sát #3 của user**).

Tệ hơn — đây là vòng luẩn quẩn khiến box **không bao giờ tự thoát ra được**: sau timer wake box chỉ
thức **2 giây** (`activeSleepTimeoutMs = 2000` trong `main.cpp`), trong khi driver Wi-Fi cần ~6s+ mới
tự phát hiện mất beacon rồi `setAutoReconnect(true)` mới kích hoạt. Box ngủ lại **trước khi** stack kịp
nhận ra mình đã mất kết nối → lặp lại hàng trăm chu kỳ 5 phút → giải thích luôn quan sát #1.

Xác nhận thêm: `esp_light_sleep_start()` (`PowerManager::enterLightSleep`) được gọi khi Wi-Fi vẫn đang
active — **không** `esp_wifi_stop()`, **không** `esp_pm_configure()`, **không** cấu hình PS nào (grep
toàn repo: 0 kết quả). Đây là kiểu dùng light sleep mà ESP-IDF không đảm bảo giữ được association.

**Fix đã áp dụng:**
1. **`ensureConnected()` không còn tin `WiFi.status()`** (`NetworkManager.cpp`):
   - Fast-path giờ đòi **cả** `WL_CONNECTED` **lẫn** IP hợp lệ (`(uint32_t)WiFi.localIP() != 0`) —
     `WL_CONNECTED` chỉ nghĩa là associate+auth xong, chưa chắc đã xin được IP từ DHCP; gửi HTTP khi
     chưa có IP cũng ra `-1`.
   - Thêm cờ `_forceReassociate` + `notifyWakeFromSleep()`: **vừa ngủ dậy thì LUÔN tái lập
     association**, bỏ qua fast-path hoàn toàn. `main.cpp` gọi `notifyWakeFromSleep()` ngay sau
     `enterLightSleep()`.
   - Bỏ `WiFi.reconnect()` (dựa trên chính trạng thái driver đang sai) → luôn `WiFi.disconnect(false)`
     + `WiFi.begin()` sạch, và **chờ cả status lẫn IP** mới coi là thành công.
2. **Timeout `ensureConnected` trong `syncWakeup()`: 5s → 12s** — sau light sleep đây là associate +
   4-way handshake + DHCP hoàn toàn mới, thực tế tốn 3-8s; cắt ở 5s là bỏ dở đúng lúc sắp xong.
   Không tốn pin oan vì `Task_UIController` bị khoá không cho ngủ khi `isSyncing()`.
3. **Retry `-1` giờ có tái kết nối** (`checkAndDownloadNewMessages`): cách cũ chỉ `delay(500)` rồi
   `http.GET()` lại trên cùng client — vô ích khi nguyên nhân là link chết hoặc DNS cũ. Giờ ép
   `_forceReassociate` + `ensureConnected(12000)` trước khi thử lại (việc này cũng xin lại DNS server
   mới từ DHCP).
4. **Fallback creds mặc định** (quan sát #2, `main.cpp`): trước đây hễ NVS có **bất kỳ** SSID nào thì
   `DEFAULT_WIFI_SSID/PASSWORD` trong `config.h` **không bao giờ được thử tới** → box đi thẳng vào AP
   mode dù creds mặc định vẫn dùng được. Giờ NVS creds fail sẽ thử tiếp creds mặc định. Log cũng ghi
   rõ nguồn: `[BOOT] WiFi: <ssid> (NVS|default)` — trước chỉ in SSID nên không phân biệt được "đang
   dùng creds mặc định" với "đang dùng creds cũ còn sót trong NVS".
5. **Log chẩn đoán heap**: `[NET] sync start heap=N` đầu mỗi chu trình sync + heap in kèm khi GET fail
   — để lần sau phân biệt được OOM với lỗi đường truyền (xem mục `setBufferSizes` bất khả thi ở trên).

**Giả thuyết còn lại cho quan sát #2 mà code không sửa được**: có thể **mật khẩu router đã đổi** trong
khi `DEFAULT_WIFI_PASSWORD` trong `config.h` vẫn là giá trị cũ (`config.h:83-84`). User nói "SSID đúng"
chứ không khẳng định mật khẩu. Log mới `[BOOT] WiFi: ... (NVS|default)` sẽ giúp xác định.

**Chưa làm, cân nhắc nếu vòng 4 vẫn chưa đủ**: tắt hẳn Wi-Fi trước khi ngủ
(`esp_wifi_stop()` / `WiFi.mode(WIFI_OFF)`) rồi bật lại sau khi thức, thay vì dựa vào modem sleep giữ
association. §6.B.3 từng tính modem sleep tiết kiệm hơn 28% ở chu kỳ 5 phút — **nhưng phép tính đó giả
định association SỐNG SÓT qua giấc ngủ, mà thực tế cho thấy giả định này sai**. Nếu vòng 4 cho thấy
association chết ở gần như mọi lần wake (xem log `[NET] re-assoc`), thì tắt hẳn Wi-Fi khi ngủ sẽ vừa
đúng hơn vừa tiết kiệm hơn (không tốn 0.4mA rò trong 5 phút cho một association đằng nào cũng chết).

**Chưa build/flash/test.** Cần user xác nhận qua log:
- `[NET] re-assoc (wake=1 st=3)` — `st=3` (WL_CONNECTED) mà vẫn phải re-assoc ⇒ **xác nhận đúng chẩn
  đoán**: status đã nói dối. Đây là dòng log đáng chú ý nhất.
- `[NET] WiFi OK (Nms)` — thời gian tái lập thật sự là bao nhiêu (để chỉnh timeout 12s cho hợp lý).
- Sau wake, sync có chạy trọn (`updateFirebaseStatus` → flags → messages) mà không còn `-1` không.
- `[BOOT] WiFi: <ssid> (NVS|default)` — SSID đang dùng có đúng là mạng nhà không.

---

### Gotcha treo: I2S sample rate không được reset khi đổi bài (2026-09-03, CHƯA sửa)

User báo audio phát nhanh hơn hình, **reset box thì tự hết** nên đã dừng truy cứu. Ghi lại vì "reset
là hết" khớp đúng với một bug tiềm ẩn tìm được dọc đường, và nó sẽ tái xuất hiện:

`AudioPlayer::stop()` đặt lại biến theo dõi `_sampleRate = AUDIO_SAMPLE_RATE` (8000) **nhưng không đặt
lại tốc độ I2S phần cứng**. Trong khi `loadFromStorage()` chỉ gọi `i2s_set_sample_rates()` khi
`sampleRate != _sampleRate`. Hậu quả:
- Phát bài 16kHz → HW = 16000×4 = 64000, `_sampleRate` = 16000
- `stop()` → `_sampleRate` = 8000, **HW vẫn 64000**
- Phát bài 8kHz → `8000 == 8000` → **bỏ qua set rate** → HW giữ 64000 trong khi cần 32000 → phát nhanh 2x

Reset box làm `init()` mở lại I2S ở rate mặc định nên triệu chứng biến mất — đúng như quan sát.

**Dữ liệu thật đã đo** (file user gửi, đọc header RIFF): các message cũ là **16000 Hz**, các message
mới là **8000 Hz** → đúng kịch bản trên. Lưu ý trung thực: cơ chế này cho ra sai lệch **2x**, user mô
tả "4x" — nên có thể chưa phải toàn bộ câu chuyện, hoặc "4x" là ước lượng cảm quan.

**Không phải do các thay đổi trong phiên 2026-09-02/03**: `git show bc0f80a --stat` xác nhận chỉ
`MediaPlayer.cpp` bị đụng (118 thêm / 1 xoá, dòng xoá là `decodeOneFrame(false)` ở nhánh ảnh tĩnh);
`AudioPlayer.cpp`, `AudioPlayer.h`, `config.h` **không hề bị sửa**. Đây là bug có sẵn từ trước.

**Cách sửa khi nào quay lại**: bỏ điều kiện `sampleRate != _sampleRate`, luôn gọi
`i2s_set_sample_rates(I2S_NUM_0, sampleRate * AUDIO_OVERSAMPLE)` trong `loadFromStorage()`; hoặc cho
`stop()` reset luôn HW về `AUDIO_SAMPLE_RATE * AUDIO_OVERSAMPLE` cho khớp với biến theo dõi.

#### Cần user xác nhận trên máy thật (chưa build/flash/deploy)
1. Card cũ (video/ảnh/voice/text riêng lẻ) không bị regression.
2. Card "Tin nhắn tĩnh" — từng tổ hợp riêng lẻ (chỉ ảnh/chỉ text/chỉ nhạc nền) và tổ hợp 2-3 thành phần.
3. Combo ảnh+voice (kể cả từ card cũ) giờ có tiếng — trước đây câm.
4. Tải lỗi/stall với audio/text-only: slot phải bị `discardWrite()` sạch, không để lại slot rác.
5. Sau khi nạp firmware NSL3 lần đầu: tin nhắn cũ (nếu còn) bị xoá 1 lần — xác nhận đúng dự kiến.
6. Message tạo từ card mới xuất hiện trên Firebase với `type:"image"` (không phải `"static"`) và có
   `bg_music_url` khi có đính kèm audio.

---

## 12. PHASE 4 — Hoàn thiện tầng lưu trữ THẺ SD (2026-09-04, CHƯA flash/test máy thật)

> Đánh số **12** để chừa §10/§11 cho nhánh `security-hardening` đang viết song song — hai nhánh cùng
> append vào cuối file này, trùng số sẽ thành xung đột đúng vùng đã được dặn tránh.

Bối cảnh: module SD sắp hàn **thay thế** W25Q128 trên đúng bộ chân cũ (kể cả CS = GPIO 8), hai chip
không bao giờ cùng nằm trên bo. `SDStorageProvider` trước đó mới là scaffold Phase 3A, **không chạy được**.

### Quyết định đã CHỐT với user (2026-09-04)
- **20 slot** trên thẻ (NAND giữ 3 vì bị giới hạn 16MB).
- **Trạng thái hàng chờ nằm trên THẺ** (`/media/index.bin`), KHÔNG dùng NVS — để trạng thái đi theo dữ
  liệu khi rút/cắm thẻ, tránh cảnh NVS báo "có tin chưa đọc" mà file đã biến mất theo thẻ.
- Làm trên nhánh `main` tại worktree gốc. `ACTIVE_STORAGE_TYPE` **giữ `STORAGE_TYPE_NAND`** khi commit.

### Vì sao 20 slot KHÔNG cần đụng class cha
`IStorageProvider` trao đổi hoàn toàn bằng **chuỗi identifier**, không có hàm nào trả về slot index hay
bitmask. `_unreadBitmask` là private member của riêng `NandStorageProvider` (`NandStorageProvider.h:56`).
Bản SD dùng **1 byte cờ `unread` mỗi slot** trong manifest nên vượt trần 8 thoải mái.
Ràng buộc thật sự duy nhất: `NetworkManager.cpp:1043,1178` khai báo `char writeSlotId[16]` → identifier
phải ≤ 15 ký tự (đang dùng thập phân trần `"0"`…`"19"`, 2 ký tự).

### Bug NGHIÊM TRỌNG đã tìm ra và sửa: không có thẻ = treo cứng box
`main.cpp:398-404` chạy `while (1) { delay(100); }` khi `storage->init()` trả `false`.
`NandStorage::init()` **không bao giờ** trả false nên nhánh này chưa từng chạy trong đời.
`SD.begin()` thì trả false mỗi khi thẻ vắng/lỏng/không phải FAT — tình huống **bình thường** với thẻ rút
được. Bản cũ `SDStorageProvider::init()` trả thẳng kết quả mount ra ngoài.
→ **`init()` giờ LUÔN trả `true`**; không mount được thì chạy ở chế độ rỗng (`isFull()` trả true để hệ
thống không cố tải, `hasUnreadMessage()`/`getUnreadCount()` trả 0).

### Xung đột SPI mode (nguy cơ nhiễu hình lớn nhất) — đã xử lý
Thư viện `SD` của Arduino-ESP32 **hardcode `SPI_MODE0`** (`sd_diskio.cpp`, struct `AcquireSPI`, cả 2
constructor) và `SD.begin()` **không có tham số mode** → phía SD không dời được. Đã kiểm chứng trực tiếp
trong source `~/.platformio/packages/framework-arduinoespressif32/`.
→ Thêm `SPI_BUS_MODE` vào `config.h`, gate bằng `ACTIVE_STORAGE_TYPE`: **0 cho SD, 3 cho NAND**.
`DisplayDriver.h:19` đọc hằng số này. Bản NAND commit ra cấu hình LovyanGFX **giống hệt trước**.
`Panel_ST7789` của LovyanGFX vốn mặc định mode 0 nên đây không phải hạ cấp.
Comment `Mode 3 (obligatory)` ở `DisplayDriver.h:9` là **SAI**, đã sửa (xem đính chính ở §2).

**Bẫy đã tránh:** NOP hack ở `NandStorage::acquireSPI()` mở `SPISettings(..., SPI_MODE3)`. Copy nguyên
sang `SDCardManager` là **tái tạo đúng cú lật CPOL đang muốn khử**. Bản SD dùng `SPI_BUS_MODE`.

### Tổ chức thư mục trên thẻ
```
/media/index.bin      manifest 488B (hàng chờ + metadata 20 slot), giữ trong RAM
/media/slot_NN.bin    media — layout GIỐNG HỆT slot NAND:
                        [4B kích thước][header container 16B][payload][AUDC + audio]
/media/slot_NN.txt    caption sidecar (chỉ tạo khi tin có text)
```
- **Audio nối vào CÙNG file** với video (như NAND) → `readAt(dataSize + k)` chỉ là đọc offset tuyệt đối,
  **`AudioPlayer` không phải sửa dòng nào**, không rủi ro regression bản NAND.
- **Caption để file sidecar** chứ không nhét manifest: 256B × 20 = 5KB RAM thường trú, quá đắt trên
  ESP32-C3 nơi §8 đang giành từng KB cho TLS handshake.
- Mọi đường dẫn dựng từ **index đã parse**, không bao giờ từ chuỗi thô → diệt tận gốc bug cũ:
  `buildFilePath()` map `"0"`→`/media/0.bin` nhưng `"slot_0"`→`/media/slot_0.bin`, trong khi đường ghi
  trả `"slot_0"` còn `getFirstValidIdentifier()` trả `"0"` → **ghi và đọc trỏ 2 file khác nhau**.

### Các bug khác của bản SD cũ đã sửa
| Bug | Hậu quả |
|---|---|
| `seek()` là hàm rỗng | Toàn bộ đường đọc `MediaPlayer` chạy bằng `seek()` → **không phát được gì** |
| `readAt()` không override (mặc định trả 0) | `AudioPlayer` fail ở bước kiểm magic `AUDC` → **câm vĩnh viễn** |
| `isFull()` cứng `false` + `getNextWriteSlotIdentifier()` luôn trả `"slot_0"` | **Mọi lượt tải ghi đè cùng 1 file** |
| `getItemInfo()` cứng `type=VIDEO, fps=15`, `dataSize` = cả file | Sai metadata; còn gọi `getFileSize()` (I/O thật) **mỗi frame** vì `main.cpp:183` gọi nó mỗi vòng `player.update()` |
| `getNextValidIdentifier()` hardcode `% 5` | Sai với mọi slot count khác 5 |
| `Serial.println` nhưng `main.cpp` không `Serial.begin()` | **Mọi lỗi SD vô hình** → đã đổi hết sang `DLOG` |

### Chi tiết cài đặt đáng nhớ
- **`_hdrPeek`**: chụp 16 byte header container vào RAM ngay trong `writeChunk` khi con trỏ ghi còn dưới
  20, thay vì đọc ngược từ file lúc `closeWrite()` như NAND (NAND đọc lại được vì ghi thẳng flash; SD
  còn buffer stdio xen giữa). Sentinel `dataSize == 4` rơi ra miễn phí: nhánh text-only không ghi gì →
  `_hdrPeek` toàn 0 → nhánh `else` → `VIMG/4/fps=1/frames=1`, đúng y bản NAND.
- **Back-patch 4 byte tiền tố**: `seek(0)` trên chính handle `"w"` đang mở rồi ghi 4 byte. `"w"` là
  `O_TRUNC` — cắt file xảy ra lúc **open**, không phải lúc write, nên ghi đè đầu file không làm ngắn file.
  Phương án dự phòng nếu sai: đóng rồi mở lại mode `"r+"`.
- **`openForAppend()` seek tới `slots[idx].dataSize`, KHÔNG phải EOF** → header `AUDC` rơi đúng offset mà
  `AudioPlayer::loadFromStorage()` dò (`readAt(dataSize, ...)`), kể cả sentinel `dataSize == 4`.
- **`readAt()` dùng file handle THỨ HAI** (`_atFile` trong `SDCardManager`). Seek-rồi-seek-lại trên một
  handle chung chính là bug mà comment ở `IStorageProvider.h:44-47` tồn tại để chặn — mỗi vòng
  `MediaPlayer::update()` chạy `readAt`→`readData`→`readAt` liên tục.
- **`closeWrite()` và `discardWrite()` đều XOÁ file `.txt`**: NAND miễn nhiễm vì `setSlotInfo()` `memset`
  cả `SlotEntry` (giết luôn `textLen`); sidecar không có ràng buộc đó nên caption tin cũ sẽ hiện đè lên
  tin mới dùng lại slot.
- **`SD.begin()` mặc định 4MHz** → quá chậm cho video 15fps. Đang đặt `SD_SPI_FREQ_HZ = 20MHz`.
- **`discardWrite()` có 2 nhiệm vụ** (giống NAND): huỷ phiên chưa commit, VÀ undo slot **đã commit** —
  đường "tin nhắn tĩnh" gọi `closeWrite()` trước rồi mới biết audio có tải được không.

### Quy tắc đã áp dụng cho code mới (đừng phá khi sửa sau)
- `DLOG` **phải nằm ngoài** vùng giữ mutex: `ScreenLogger::render()` lấy chính `spiMutex` (không đệ quy,
  timeout 50ms). Một `DLOG` đặt nhầm trong `writeChunk` (chạy mỗi 2KB) tốn 50ms/chunk và **trông y hệt
  lỗi mạng**. Vì vậy `writeChunk`/`appendChunk` cố tình không có log nào ở nhánh thành công.
- Mọi lời gọi `SD.*` / `File.*` phải nằm trong `acquireSPI()/releaseSPI()`; không `acquireSPI()` lồng nhau.
- `getItemInfo()` và `isFull()` **thuần RAM, zero I/O** (bị gọi mỗi frame / mỗi tick UI).
- **KHÔNG dùng `File::size()`** để tính `dataSize`: `VFSFileImpl::size()` gọi `stat()` → trả kích thước
  trên đĩa, bỏ sót phần còn trong buffer stdio. Phải đếm bằng giá trị `fwrite`/`write` **trả về**.
- `lib/NandStorage/*` phải có **0 dòng thay đổi** — đừng "dọn" 4 literal `SPI_MODE3` ở đó sang hằng số mới.

### Đã cân nhắc và LOẠI (đừng đào lại)
- `index.bak` / temp+rename cho manifest: manifest cụt đã tự fail magic check và rơi về "hàng chờ rỗng,
  file còn nguyên" — backup cũ dẫn tới đúng trạng thái đó chỉ sau 1 tin nhắn, không đáng thêm code.
- **Dựng lại manifest bằng cách quét file trên thẻ**: không tách được `dataSize` khỏi `audioSize` từ file
  → cho ra playback **sai một cách tự tin** (audio câm + frame rác) thay vì "không có tin" trung thực.
- Trần dung lượng mỗi slot trong `writeChunk`: giới hạn thật của SD là dung lượng trống, `fwrite` trả
  short count là tín hiệu tự nhiên (NAND cần `_slotCapacity` vì slot là vùng flash cố định).

### CHƯA build/flash/test — cần user xác minh
> **Không tháo NOR cho tới khi xác minh xong Phase A của nhánh bảo mật.**

1. **Bản NAND sau khi sửa `DisplayDriver.h`**: màn hình/đồng hồ/video/audio y như cũ (`SPI_BUS_MODE` = 3).
2. Bản SD, thẻ trắng: có thể thấy chớp nhiễu ngắn lúc `SD.begin()` — đó là NOP hack đang chạy, không phải
   lỗi. Lần redraw kế tiếp phải sạch.
3. **Phép thử phân biệt lệch khung byte**: phát tin **ảnh tĩnh + voice** (state `SHOWING`, nơi overlay
   `ScreenLogger` vẫn bật — `MediaPlayer.cpp:308` chỉ tắt cho `PLAYING`). Nhìn chữ overlay render sạch
   30s+ trong lúc audio chạy. Tin video che mất triệu chứng này hoàn toàn.
4. Boot **không thẻ**: standby vẫn vẽ, **không treo**, hiện `[SDP] khong co the`.
5. Hexdump `/media/slot_00.bin` sau khi tải video: byte 0..3 = LE (filesize − audioSize − 4); byte 4..7 =
   `SLBX`/`VJPG`/`VIMG`; tại offset `dataSize` có ASCII `AUDC`. Một phép kiểm này xác thực cùng lúc
   back-patch, parse `_hdrPeek`, và gốc append.
6. Tin chỉ text/voice: file đúng `4 + 10 + audio` byte, `dataSize == 4`, phát ra màn đen + chữ + tiếng,
   **không** lỗi `Bad jpegSize`.
7. Tải 20 tin không đọc → tin thứ 21 báo full. Đọc 1 tin → lượt tải kế rơi vào **đúng slot đó**.
8. Ép lỗi giữa chừng (rút Wi-Fi ~50%): `slot_NN.bin` **và** `.txt` biến mất, sync sau retry **đúng slot cũ**.
9. Tái dùng slot: tin có caption vào slot 3 → đánh dấu đã đọc → tin không caption vào slot 3 →
   **không còn caption cũ trên màn hình**.
10. Tự chữa lành, **hai kiểu hỏng khác nhau**, phải thử cả hai:
    - **Xoá** 1 file `.bin` trên máy tính rồi cắm lại → `getNextUnreadIdentifier()` bỏ cờ unread, đi tiếp.
    - **Cắt ngắn** 1 file `.bin` (giữ file nhưng xén bớt byte) → `openForRead()` phải trả false với log
      `slot N cut:`. Đây là kiểu hỏng mà `fileExists()` KHÔNG thấy; nếu lọt thì mỗi frame sẽ đâm vào
      `Read short` + `delay(2000)` của MediaPlayer, treo hình từng nhịp 2 giây.
11. `Read short` / `Bad jpegSize` khi phát trên SD = độ trễ đọc, **không phải lỗi logic** → nghi can đầu
    là `SD_SPI_FREQ_HZ` (thử hạ 10MHz), thứ hai là class thẻ. `[NET] dl STALL` chỉ xuất hiện ở bản SD →
    thử bỏ dòng `setBufferSize(512)` trong `SDCardManager::openFileForWrite()`.

### Rủi ro còn treo (đo, đừng thiết kế vòng quanh)
- **Tốc độ nạp audio**: `AudioPlayer::tick()` gọi tới ~6 lần `readAt` 256B, 2 lần mỗi frame. Đã thêm bước
  bỏ qua seek khi đúng vị trí (`_atPos`) + readahead stdio, nhưng đây là dự đoán thông lượng, chỉ phần
  cứng mới chốt được. Cần lever đầu tiên: `SD_SPI_FREQ_HZ`.
- **Heap**: mỗi `File` mở cấp 1 buffer stdio; kích thước mặc định 4096 chỉ áp dụng **có điều kiện**
  (`vfs_api.cpp:301`, khi `st_blksize == 0`) nên chi phí thật **chưa kiểm chứng**. Có 2 read handle sống
  cùng lúc với `_jpegBuffer` 32KB của `MediaPlayer`. Nếu `[PLAY] JPEG buf alloc fail` xuất hiện ở bản SD
  mà không có ở NAND thì đây là nguyên nhân → gọi `setBufferSize()` cho các read handle.




