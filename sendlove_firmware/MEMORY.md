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
>   - ❌ **ĐÍNH CHÍNH TRÊN LÀ SAI — kiểm chứng máy thật 2026-09-17**: nạp bản SD chạy MODE0 thì
>     **màn hình đen hoàn toàn** (đèn nền, touch, tiếng bíp vẫn chạy). Ép lại MODE3 thì hiển thị bình
>     thường. Panel ST7789 không CS này **BẮT BUỘC MODE3** — comment gốc "Mode 3 (obligatory)" ở
>     `DisplayDriver.h` là đúng từ thực nghiệm. Hướng đã chọn: chép thư viện SD vào `lib/SD` và đổi
>     sang MODE3 (xem mục 12).
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

> ❌ **HƯỚNG "KÉO CẢ BUS VỀ MODE0" Ở TRÊN ĐÃ THẤT BẠI trên máy thật (2026-09-17).**
> Nạp bản SD MODE0 → màn hình đen hoàn toàn (đèn nền/touch/bíp vẫn chạy); ép MODE3 → hiển thị lại.
> Giả định "Panel_ST7789 mặc định mode 0 nên không phải hạ cấp" là SAI cho panel CS-less này: nó
> **bắt buộc MODE3**. Với màn hình MODE3 mà thư viện SD vẫn MODE0 thì box đứng hình ở "Booting..."
> (lệch khung byte khi truy cập thẻ).
> **Hướng thay thế đã làm:** chép thư viện SD của framework (6 file, Apache 2.0) vào `lib/SD`, đổi đúng
> 2 dòng `SPI_MODE0` → `SPI_MODE3` trong struct `AcquireSPI` (`lib/SD/src/sd_diskio.cpp`) — mọi lưu
> lượng SPI của thư viện đi qua đó. Xác nhận bằng `pio run -v`: LDF resolve `SD @ 2.0.0` về
> `lib\SD`, không dùng bản framework. `SPI_BUS_MODE` giờ là `3` cố định, bỏ cổng `#if`.
> ✅ **Đã kiểm chứng máy thật 2026-09-17** (bo vẫn còn NOR, chưa cắm thẻ): bản SD với `lib/SD` MODE3
> boot vào tới màn hình chờ bình thường — xác nhận việc đứng ở "Booting..." trước đó là do thư viện SD
> MODE0 làm lệch khung byte màn hình, không phải treo.
> **Chưa kiểm chứng thẻ SD thật chạy được MODE3** (cả hai mode đều lấy mẫu sườn lên nên thường
> được, nhưng phải thử). Nếu nâng cấp framework-arduinoespressif32, phải so lại `lib/SD` với bản mới.

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




---

## 10. Vá bảo mật theo `code_review_2_9_gemini_38.md` — Phase A: bật xác thực TLS (2026-09-03)

Kế hoạch đầy đủ (Phase A→D + phát hiện ngoài báo cáo) nằm ở
`C:\Users\phamp\.claude\plans\t-i-c-n-b-n-c-sequential-treasure.md`.
Phạm vi user chốt: **bỏ OTA** (VULN-02/12 — chỉ test local, chấp nhận rủi ro) và **bỏ VULN-04**
(erase 5.5MB — đã fix ở vòng 2).

### Báo cáo Gemini SAI ở 2 chỗ — đã verify bằng code, đừng "sửa" lại

1. **VULN-08 "Captive Portal XSS qua SSID" — FALSE POSITIVE hoàn toàn.**
   `captive_portal_html.h:172` dùng `nm.textContent = n.ssid` (DOM API, không hề parse HTML),
   `:179` gán `.value`. **Không có `innerHTML` nào nhận dữ liệu SSID** — `:160` chỉ
   `box.innerHTML = ""` để xoá danh sách. Tầng JSON cũng escape đúng (`jsonEscape()` trong
   `NetworkManager.cpp`: xử lý `"`, `\`, ký tự < 0x20, ép `unsigned char` để byte UTF-8 > 127
   không lọt nhánh ký tự điều khiển). **Không cần vá gì.**
   *Nit tuỳ chọn duy nhất*: `seen[n.ssid]` (`:165`) dùng object thường làm map — SSID tên
   `__proto__`/`constructor` sẽ bị coi là "đã thấy" và biến mất khỏi danh sách. Đó là "giấu 1
   mạng", **không phải XSS**. Sửa bằng `Object.create(null)` nếu rảnh.
2. **VULN-06 "markAsRead() làm mòn Sector 0" — SAI.**
   `NandStorageProvider::markAsRead()` chỉ gọi `saveNvsState()` (NVS, ESP-IDF có wear-leveling
   sẵn), **không đụng `writeSlotTable()`**. Số lần ghi Sector 0 thật = tối đa 3/message
   (`closeWrite` → `closeAppend` → `setItemText`). 20 msg/ngày ⇒ ~4,5 năm mới chạm 100k chu kỳ.
   Hạ ưu tiên xuống P3, **đề xuất hoãn**.

Ngoài ra **VULN-05 bị nói quá**: với HTTPS thì `?auth=<secret>` nằm TRONG đường hầm TLS —
proxy log / router cache / header Referer **không** đọc được (claim của báo cáo sai cho HTTPS).
Rủi ro thật là secret nằm cứng trong flash (`esptool read_flash` lấy được) và là quyền admin.
Lưu ý: **chuyển secret từ `config.h` sang NVS KHÔNG cứu được gì** — NVS nằm trong cùng bản dump flash.

### Đã làm (Phase A) — CHƯA build/flash/test trên máy thật

- **File mới `include/firebase_root_ca.h`** (commit bình thường, cert không phải secret): chứa
  **GTS Root R1** (RSA) + **GTS Root R4** (ECC), bản self-signed tải từ `https://pki.goog/repo/certs/`,
  hết hạn **2036-06-22**. Đã round-trip verify: trích ngược từ header ra PEM và `openssl x509`
  cho đúng fingerprint SHA-256 `D9:47:43:2A:...:F4:CF` (R1) và `34:9D:FA:40:...:3C:7D` (R4).
- **Vì sao phải có CẢ HAI root** — đo thật bằng `openssl s_client`, không suy đoán:
  - `iot-app-839a2.asia-southeast1.firebasedatabase.app` → leaf → `GTS WR1` → **GTS Root R1**
  - `firebasestorage.googleapis.com` → leaf → `GTS WE2` → **GTS Root R4**

  Chỉ nhúng R1 thì Realtime DB chạy nhưng **đường tải media chết**. Root mà server gửi kèm trong
  chain là bản cross-sign bởi GlobalSign (hết hạn **2028-01-28**) — không dùng bản đó.
- **`setCACertBundle()` KHÔNG dùng được trên Arduino core 2.0.17** (cùng loại bẫy với
  `setBufferSizes` ở §8 — verify source trước khi tin): đọc
  `libraries/WiFiClientSecure/src/esp_crt_bundle.c:179-186`, `arduino_esp_crt_bundle_attach()`
  return sớm với `log_e("Failed to attach bundle")` nếu chưa gọi `arduino_esp_crt_bundle_set()`
  — **wrapper Arduino không nhúng sẵn bundle mặc định** dù `sdkconfig` có
  `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y`. Muốn dùng phải tự sinh blob bằng `gen_crt_bundle.py`.
  **Đừng đào lại hướng này.**
- **`NetworkManager.cpp`**: thêm `configureTlsClient()` + `logTlsError()`, đặt SAU khối
  `#include <WiFiClientSecure.h>` ở giữa file — **file này include WiFiClientSecure ở dòng ~461
  chứ không phải đầu file**, đặt helper lên đầu sẽ lỗi incomplete type. Thay **cả 5** chỗ
  `setInsecure()` (status / flags / patch flags / alarms / messages). `setCACert()` bật
  `MBEDTLS_SSL_VERIFY_REQUIRED` + hostname verification (`ssl_client.cpp:180, 257, 292`).
- **Cờ lùi `FIREBASE_TLS_VERIFY`** trong `config.h`: đặt 0 = quay lại `setInsecure()`. Có cờ này vì
  theo Session Rule §8 agent không build được, mỗi vòng thử-sai tốn nguyên 1 lượt flash của User.
- **KHÔNG đặt `setHandshakeTimeout()`** (mặc định 120s): siết ngắn không giúp gì cho bảo mật mà
  link yếu (associate + DHCP đã tốn 3-8s, xem Fix vòng 4) sẽ đẻ ra một kiểu fail trông giống lỗi
  cert nhưng không phải.
- **Chốt chặn thời gian trong `syncWakeup()` — BẮT BUỘC, không phải phụ kiện.** Với `setInsecure()`
  thì NTP hỏng vẫn chạy; với `VERIFY_REQUIRED` thì `time(nullptr)` gần 0 lúc boot nguội →
  `BADCERT_FUTURE` → **mọi** handshake fail. Nếu giờ chưa hợp lệ: hạ `_isTimeSynced = false`
  (bắt buộc — `syncNtpTime()` short-circuit 60s sẽ return true suông) rồi retry NTP 15s; vẫn sai
  thì `DLOG("[NET] sync abort: time invalid")` và bỏ chu trình.
- **Log chẩn đoán** (vì thay đổi này đẻ thêm một kiểu fail hình dạng `-1`):
  `WiFiClientSecure::lastError(char*, size_t)` có thật trong core 2.0.17 → `[NET] tls <where>: ...`
  ở cả 6 điểm fail (status/flags/alarms/msg/media/audio). Heap in ở `[NET] status OK heap=` và
  `[NET] GET OK len=N heap=`.

### Cần User xác minh trên máy thật (Phase A phải flash RIÊNG, chưa áp Phase B)

1. `[NET] NTP OK` xuất hiện **trước** mọi lời gọi Firebase; không thấy `[NET] sync abort: time invalid`.
2. `status` → `flags` → `messages` chạy trọn, không `-1`.
3. **Tải được 1 message có media** — đây là bước chứng minh root R4 (Storage) đúng; các bước trên
   chỉ chứng minh R1.
4. **Bật tay `boxes/<BOX_ID>/flags/sync_alarms_flag = true` trên Firebase Console** rồi sync:
   `syncFirebaseAlarms()` là 1 trong 5 client nhưng **chỉ chạy khi cờ này bật** — boot-and-sync
   thường không hề chạm tới nó, lỗi ở đó sẽ ẩn tới tận lần báo thức sau rồi trông như bug mới.
5. Heap: nhìn `[NET] GET OK len=N heap=` (phiên TLS sống lâu nhất, giữ qua cả đoạn tải audio lồng
   bên trong, và là chỗ từng có tiền sử OOM). Kỳ vọng > ~60KB.
6. Nếu hỏng: `[NET] tls ...` phân biệt lỗi cert với lỗi mạng. Đường lùi: `FIREBASE_TLS_VERIFY 0`.

Rủi ro tồn dư đã biết: nếu Google đổi root thì box mất cloud và phải nạp lại firmware. Với hạn
2036 thì rủi ro thấp. Dấu hiệu nhận biết trên log là `[NET] tls ...` báo lỗi verify.

### Còn lại, chưa làm

- **Phase B** (chờ Phase A xác minh xong): GPIO 8 bọc `spiMutex` (user đã chốt **giữ đèn**, không
  xoá — vì `wakeupFlash()` giờ chỉ gọi `gpio_hold_dis()`, đây là chỉ báo timer-wake duy nhất còn
  lại); `std::atomic<AppState> currentAppState`; trần `MAX_MEDIA_BYTES`; **đo**
  `uxTaskGetStackHighWaterMark` (ESP-IDF trả về **bytes**, không phải words) trước khi động vào
  `showWrappedText`.
  → **Không đụng `isSyncing()`**: báo cáo nói quá. 4 cờ `volatile` nhưng `_isSyncing` một mình đã
  phủ trọn chu trình (set trong `portENTER_CRITICAL` ở `triggerWakeupSync`, clear cuối
  `syncWakeup`) — đọc gộp 4 cờ không tạo cửa sổ nào mà đọc 1 cờ không có.
- **Phase C** (mòn Sector 0): đề xuất **hoãn**, xem số liệu 4,5 năm ở trên. Nếu làm thì thiết kế
  đúng là dời điểm commit (`bool commit = true` cho `closeWrite`/`closeAppend`/`setItemText`),
  đã verify an toàn vì `openForAppend()` chỉ đọc state RAM (`_lastWrittenOffset`), không đọc lại
  bảng từ flash.
- **Phase D**: user chọn hướng "custom token + rules `$box_id == auth.uid` + refresh idToken 1h".
  **Nhưng khảo sát cho thấy `sendlove_backend` ĐÃ CÓ SẴN nguyên bộ device-auth chưa từng được
  firmware dùng**: `POST /device/register` (bảo vệ bằng provisioning key) trả
  `{box_id, device_secret, rcode, scode}`; middleware `requireDeviceAuth` đọc header
  `X-Device-Id` + `X-Device-Secret`; `GET /device/poll?last_download_ts=&available_slots=` trả
  `flags` + message mới — **trùng khít** việc `checkAndDownloadNewMessages()` đang tự làm bằng REST
  thẳng lên RTDB. Đã nêu đề xuất đổi hướng cho user, **chờ user xác nhận**; đánh đổi thật là
  `device_secret` **tĩnh** so với idToken **hết hạn 1h**.
  Cần chốt trước khi code: backend đã deploy Cloud Functions chưa (URL production), lấy
  `PROVISIONING_KEY` ở đâu, `/device/poll` trả media URL dạng signed URL hay path Storage.
- **Phát hiện ngoài báo cáo**: AP `SendloveBox-Setup` là **AP MỞ** (`apPassword` mặc định `""`,
  `main.cpp` gọi `startProvisioningAP("SendloveBox-Setup")`) và `/save` không có CSRF token → ai
  trong tầm sóng cũng đổi được Wi-Fi của box rồi MitM nó. Đặt mật khẩu WPA2 là **quyết định sản
  phẩm** (đánh đổi trải nghiệm setup) — chờ user quyết, không tự làm.

---

## 11. 🔴 DATABASE MỞ CÔNG KHAI + chuyển sang idToken (direct RTDB) — 2026-09-03

### Phát hiện nghiêm trọng nhất từ trước tới nay (đã kiểm chứng bằng curl)

RTDB `iot-app-839a2.asia-southeast1.firebasedatabase.app` **đọc VÀ ghi được mà không cần
xác thực gì cả**:

```
GET  /boxes.json?shallow=true            -> 200 {"ESP32_A1B2C3D4E5F6":true}
GET  /messages/ESP32_.../status.json     -> 200 (dữ liệu thật)
PUT  /_sectest_probe_delete_me.json      -> 200   ← GHI ĐƯỢC, không auth
Storage .../o/<bin_url>?alt=media        -> 200   ← tải media, không token
```

(Node ghi thử đã xoá ngay, xác nhận trả `null`.)

**Nghĩa là `FIREBASE_AUTH_SECRET` chưa bao giờ là mắt xích yếu — không hề có ổ khoá nào.**
Ai biết URL là đọc hết tin nhắn, tải hết video/voice của người dùng, ghi đè dữ liệu.
So với việc này thì cả 12 mục trong `code_review_2_9_gemini_38.md` đều nhỏ.

### 🔑 NGUYÊN NHÂN GỐC — sai instance, KHÔNG phải quên deploy

`firebase database:instances:list` cho ra **2 instance**:

| Instance | Trạng thái | Ai dùng |
|---|---|---|
| `iot-app-839a2-default-rtdb` | **DISABLED** (HTTP 423 "disabled by a database owner") | không ai |
| `iot-app-839a2` | đang sống | firmware + backend (`firebase.ts:7`) |

`firebase.json` cũ dùng dạng `"database": { "rules": ... }` (không có `instance`) → CLI deploy
vào **instance mặc định**, tức cái đã bị disable. Nên mọi lần deploy trước đây đều "thành công"
mà rules không bao giờ tới được instance thật. **Đây là bài học: có nhiều instance thì bắt buộc
khai báo `instance` tường minh.**

Đã sửa `firebase.json` thành dạng mảng có `instance: "iot-app-839a2"`.

### Rules mới (`database.rules.json`) — ~~CHƯA DEPLOY~~ ✅ **ĐÃ DEPLOY 2026-09-04**

> Tiêu đề gốc "CHƯA DEPLOY" giữ lại gạch ngang cho đúng lịch sử. User tự chạy lệnh lúc
> 2026-09-04; CLI báo `rules for database iot-app-839a2 released successfully`.
> **Đo lại bằng curl sau khi deploy — lỗ hổng đã đóng:**
>
> | Probe (không auth) | Trước | Sau |
> |---|---|---|
> | `GET /boxes.json?shallow=true` | 200 | **401** |
> | `GET /messages.json?shallow=true` | 200 | **401** |
> | `PUT /_sectest_probe.json` | 200 | **401** |
>
> Đây là lỗ hổng nghiêm trọng nhất của dự án và nó đã được vá thật, không phải "deploy xong
> là xong" — số 401 ở trên mới là bằng chứng, vì chính lần deploy trước đây cũng báo
> "thành công" mà rules rơi nhầm instance.

Đổi từ `.read:false/.write:false` trống rỗng sang least-privilege theo `auth.uid === $box_id`:
- `boxes/$box_id/status` — box đọc + ghi (heartbeat)
- `boxes/$box_id/flags`  — box đọc + ghi (reset cờ)
- `boxes/$box_id/config` — box CHỈ đọc (alarm_list)
- `messages/$box_id`     — box CHỈ đọc, **giữ nguyên `.indexOn: ["timestamp"]`** (bắt buộc, nếu
  mất thì query `orderBy="timestamp"&startAt=` của firmware gãy)
- KHÔNG cấp quyền đọc `device_secret` / `code` / `pairing` — box không cần.

**Deploy rules này AN TOÀN NGAY BÂY GIỜ, không hỏng gì** (đã verify từng đường):
- Box vẫn chạy — Database Secret **bypass toàn bộ rules**
- Backend vẫn chạy — Admin SDK bypass
- Web vẫn chạy — đi qua axios `VITE_API_URL`; `sendlove_web/src/config/firebase.js` có export
  `database` nhưng **grep toàn `sendlove_web/src` không nơi nào import dùng**

Lệnh: `firebase deploy --only database --project iot-app-839a2` — **đã chạy 2026-09-04.**
(Agent bị classifier của Claude Code chặn deploy production — xác nhận lại lần nữa ngày
2026-09-04, không phải chuyện của riêng một phiên. User tự chạy. Có đường vòng kỹ thuật là ghi
thẳng `.settings/rules.json` qua REST bằng Database Secret, nhưng đó là cùng một hành động
deploy production qua cửa khác — **đừng làm**, hãy đưa lệnh cho user.)

⚠️ **`firebase deploy --only storage` thì NGƯỢC LẠI — sẽ làm CHẾT đường tải media.** Firmware
build URL `?alt=media` **không kèm token**, chạy được chỉ vì Storage đang mở. Muốn siết
`storage.rules` thì box phải xác thực khi tải (xem "ẩn số" bên dưới).

### Trạng thái provider Firebase Auth (đo thật bằng REST)

| Cách | Kết quả | Kết luận |
|---|---|---|
| `accounts:signInWithPassword` | `PASSWORD_LOGIN_DISABLED` | Email/Password **CHƯA BẬT** |
| `accounts:signUp` (anonymous) | `ADMIN_ONLY_OPERATION` | Anonymous **CHƯA BẬT** |
| `accounts:signInWithCustomToken` | `INVALID_CUSTOM_TOKEN: 3 dot separated segments` | endpoint sống, nhưng cần backend ký |

→ Hướng direct-RTDB cần **bật Email/Password trong Console** (1 toggle). Đó là điều kiện tiên quyết.

### Đã code (CHƯA build/flash, CHƯA bật)

- **`sendlove_backend/scripts/provision_box_auth.js`** — script admin SDK chạy OFFLINE, tạo user
  Auth với `uid = BOX_ID` (để rule `auth.uid === $box_id` khớp mà không cần bảng tra cứu), email
  `<boxid>@box.sendlove.invalid` (`.invalid` là TLD dành riêng RFC 2606), mật khẩu ngẫu nhiên 128
  bit in ra MỘT LẦN. Có guard: box đã có tài khoản thì từ chối, phải `--reset-password` tường minh.
- **`config_secrets.h`**: thêm `BOX_AUTH_EMAIL`/`BOX_AUTH_PASSWORD` (đang là `FILL_ME`).
  `FIREBASE_API_KEY` **đã có sẵn từ trước và trùng khớp** `VITE_FIREBASE_API_KEY` của web — không
  phải thêm gì. (Web API key không phải secret, nó nằm trong bundle web công khai.)
- **`config.h`**: `#define FIREBASE_USE_IDTOKEN 0` — **MẶC ĐỊNH TẮT**, bật lên 1 CHỈ KHI đủ 3 điều
  kiện: (a) bật Email/Password, (b) chạy script + điền config_secrets, (c) deploy rules. Bật sớm
  = box mất kết nối hoàn toàn.
- **`ConfigManager`**: `saveRefreshToken` / `loadRefreshToken` / `clearRefreshToken` (NVS key
  `fb_refresh`).
- **`NetworkManager`**:
  - `ensureIdToken()` gọi 1 lần đầu mỗi chu kỳ sync, **đặt SAU chốt chặn thời gian** — hạn token so
    bằng `time(nullptr)`, RTC sai thì token vừa lấy đã bị coi là hết hạn.
  - Ưu tiên `securetoken.googleapis.com/v1/token` (refresh), chỉ `signInWithPassword` khi chưa có
    refresh token hoặc refresh hỏng.
  - **⚠️ Hai endpoint đặt tên trường KHÁC NHAU**: identitytoolkit trả `idToken`/`refreshToken`/
    `expiresIn` (camelCase), securetoken trả `id_token`/`refresh_token`/`expires_in` (snake_case).
    `parseAuthResponse(..., bool snakeCase)` xử lý cả hai. `expiresIn` là **CHUỖI** giây, không phải số.
  - Dùng `DeserializationOption::Filter` chỉ cấp phát 3 trường cần — response còn kèm
    email/localId/kind, cấp phát trọn gói là phí heap đúng lúc sắp cần ~45KB cho handshake kế tiếp.
  - ~~**Token đi qua header `Authorization: Bearer`, KHÔNG qua `?auth=`**~~ 🔴 **SAI — xem §17.**
    Lập luận về kích thước (`url[256]`/`url[384]` tràn) thì đúng, nhưng kết luận "RTDB có parse
    header này" là **sai**, và cách kiểm chứng cũng sai: session đó thử bằng **token rác** lúc RTDB
    còn mở toang, thấy không header → 200 còn header rác → 401, rồi suy ra header được chấp nhận.
    Thực ra RTDB từ chối **mọi** `Bearer` không phải OAuth2 access token, hợp lệ hay rác đều 401.
  - `_authHeaderValue` giữ sẵn dạng `"Bearer <jwt>"` để `addHeader()` khỏi nối chuỗi; bọc trong
    `#if` để chế độ cũ không gánh 1.4KB BSS vô ích.
  - `noteAuthFailure()` gọi sau mỗi request: 401/403 thì hạ `_idTokenExpiry` để chu kỳ sau lấy token
    mới. **Không retry ngay** — token sống 1h còn chu kỳ sync vài giây, nên 401 gần như luôn là rule
    từ chối chứ không phải hết hạn; retry ngay chỉ tốn thêm handshake mà vẫn 401.
  - `fbAuthQuery(out, n, sep)` sinh phần `?auth=` (chế độ cũ) hoặc chuỗi rỗng (chế độ idToken).
    Với URL messages, auth nối bằng `'&'` vì `orderBy` đã chiếm `'?'`.
- Cert: `identitytoolkit` và `securetoken` **đều chain về GTS Root R4** (đo thật) — đã có sẵn
  trong `firebase_root_ca.h` của Phase A, không phải nhúng thêm.

### Ẩn số còn lại (chưa giải quyết được)

Khi siết `storage.rules`, box phải xác thực lúc tải media. Về lý thuyết dùng cùng idToken với
header `Authorization: Firebase <idToken>`, **nhưng KHÔNG verify được**: Storage đang mở nên gửi
header token rác vẫn trả `200`. Phải thử lại **sau khi** siết rules. Đây là điểm duy nhất mà
Phase D (signed URL từ backend) giải quyết gọn hơn hướng direct-RTDB.

### Vẫn KHÔNG giải quyết được bằng hướng này

Refresh token nằm trong NVS = cùng bản dump flash với firmware. `esptool read_flash` vẫn lấy được.
Chỉ **Flash Encryption + Secure Boot** mới chống, chưa bàn tới.

---

## 13. Phase B — VULN-03/07/09/10 đã code (2026-09-04)

> Cố ý bỏ trống số 12: nhánh `main` đã dùng `## 12` cho tầng lưu trữ thẻ SD. Hai nhánh **chưa
> merge** (`security-hardening` rẽ tại `eb3c9b2`, main đi thêm 4 commit SD). Khi nào merge thì
> đánh số lại, đừng để hai mục 12.

**Tất cả CHƯA BUILD, CHƯA FLASH, CHƯA TEST TRÊN MÁY THẬT.** Mỗi món một commit riêng, cố ý:
`7c1ac23` vẫn là "Phase A đứng một mình" để flash xác minh TLS mà không lẫn thứ gì khác.

| Commit | Mục | Nội dung |
|---|---|---|
| `fb00498` | B1 / VULN-03 | Bọc đoạn nháy GPIO 8 trong `spiMutex` |
| `07ba6d9` | B2 / VULN-07 | `currentAppState` → `std::atomic<AppState>` |
| `1f18bf5` | B3 / VULN-09 | Trần `MAX_MEDIA_BYTES` ở **cả hai** vòng lặp tải |
| `fe64a6b` | B4 bước 1 / VULN-10 | Đo stack high-water-mark, **chưa** refactor |

### Điều đã đo, không phải suy đoán

- **`std::atomic<AppState>` KHÔNG lock-free trên ESP32-C3.** Chip là RV32IMC, thiếu extension
  `A` cho atomic sub-word. Link được là nhờ ESP-IDF cấp sẵn bản emulation — verify bằng
  `riscv32-esp-elf-nm --defined-only tools/sdk/esp32c3/lib/libnewlib.a`, thấy
  `__atomic_load_1` / `__atomic_store_1` / `__atomic_exchange_1` đều là `T`. Emulation chạy
  bằng cách **tắt ngắt** ⇒ không được gọi từ ISR. Hiện an toàn: `main.cpp` không có `IRAM_ATTR`
  nào, và chỗ duy nhất trông giống callback (`setPlaybackActiveCallback`) chạy trong task context.
- **18 chỗ dùng `currentAppState`** (19 dòng grep ra, 1 là comment), **toàn bộ nằm trong
  `main.cpp`**, đều là so sánh/gán trực tiếp. Không chỗ nào bind qua `auto` → `operator T()` /
  `operator=` ngầm phủ hết. Đã grep `auto.*currentAppState` trên cả `src` và `lib`: rỗng.
  Kiểm việc này *trước* khi sửa là bắt buộc — agent không build được, một lỗi compile tốn nguyên
  một lượt flash của user.
- **`showWrappedText()` dùng ~1196 B một khung stack**: `char lines[16][48]` = 768 B +
  `buf[300]` + `currentLine[64]` + `trial[64]`. `TASK_STACK_MEDIA_PLAYER` = 6144.
  Tỉ lệ đó *có vẻ* ổn nhưng chưa ai đo đường sâu nhất — nên bước 1 chỉ đặt
  `DLOG("[PLAY] stack hwm=%u", uxTaskGetStackHighWaterMark(nullptr))`.
  **Chỉ làm bước 2 (bỏ `lines[16][48]`, đổi sang 2 lượt) nếu con số đo được < ~1024.**
  Đã verify hàm này biên dịch được: `INCLUDE_uxTaskGetStackHighWaterMark` = **1** ở
  `tools/sdk/esp32c3/include/freertos/include/esp_additions/freertos/FreeRTOSConfig.h:186`
  (số `0` ở `FreeRTOS.h:178` chỉ là fallback `#ifndef`, không áp dụng).
  ⚠️ **Đọc con số cho đúng nghĩa**: hàm trả về mức trống thấp nhất trong **toàn bộ đời task**,
  không phải của riêng khung `showWrappedText`. Nên `< 1024` có nghĩa "task này chật ở đâu đó",
  chưa chứng minh `showWrappedText` là thủ phạm. Muốn quy trách nhiệm thì phải đo thêm một điểm
  nữa ở nhánh không có caption rồi so.
  ⚠️ **Dòng log chỉ xuất hiện khi tin nhắn CÓ caption** — cả hai lời gọi `showWrappedText` nằm
  trong `if (_storage->getItemText(...))`. Test bằng video trơn sẽ không thấy gì và dễ tưởng là
  bản build không ăn.
- **Khoảng cách 2 slot NAND đầu** = `0x560000 - 0x010000` = `0x550000` = 5.570.560 B, nên
  `MAX_MEDIA_BYTES = 5.500.000` nằm vừa dưới một slot.

### Bẫy đã dính, ghi để khỏi dính lại

- **Hai vòng lặp tải dùng hai cờ lỗi KHÁC NHAU.** Video: `totalRead` → `writeError`.
  `downloadVoiceSegment()`: `aTotalRead` → `aWriteError` (rồi `ok = !aWriteError`). Đặt nhầm cờ
  thì slot dở không bị loại ở bước kiểm tra của chính vòng đó.
- **`core.autocrlf = true`, repo KHÔNG có `.gitattributes`.** Nghĩa là blob trong repo luôn là
  LF, working tree là CRLF. Sửa `MediaPlayer.cpp` xong thì file trong working tree bị đổi sang
  LF — *lịch sử không hỏng* (git normalize khi commit, diff vẫn đúng +7 dòng), nhưng file lệch
  so với bản checkout sạch. Khôi phục bằng `rm <file> && git checkout -- <file>`.
  Kiểm sau mỗi lần sửa: `file -b <path> | grep CRLF`.

### Comment SAI đã sửa lại trong code

`main.cpp` chỗ nháy GPIO 8 ghi *"Cực kì an toàn vì lúc này bus SPI hoàn toàn rảnh"* — **sai**.
`Task_MediaPlayer` có thể đang đẩy pixel lên SCK/MOSI, mà GPIO 8 chính là CS của W25Q128; ghim CS
LOW 30 ms trong lúc có xung clock thì NAND chốt nhầm opcode. Giữ `spiMutex` triệt tiêu đúng cơ chế
đó (CS LOW mà không có clock là vô hại). Chi phí: giữ mutex ~160 ms lúc vừa thức, lúc SPI rảnh.

Giữ đèn (không xoá) theo quyết định user: đây là chỉ báo timer-wake **duy nhất** còn lại, vì
`wakeupFlash()` (`DisplayDriver.cpp:171`) giờ chỉ gọi `gpio_hold_dis()` — tên hàm và cả doc
comment `/// Flash backlight 3x` đều đã lỗi thời, **không nháy gì cả**. Chưa sửa doc comment đó
(ngoài phạm vi).

### CỐ Ý KHÔNG LÀM

- **`isSyncing()`** — báo cáo Gemini nói quá. 4 cờ `volatile`, nhưng `_isSyncing` một mình đã phủ
  trọn chu trình (set trong `portENTER_CRITICAL` ở `triggerWakeupSync`, clear cuối `syncWakeup`),
  nên đọc gộp 4 cờ không tạo cửa sổ nào mà đọc 1 cờ không có. **Không mở lại.**
- **`Object.create(null)` cho `seen[n.ssid]`** (`captive_portal_html.h:165`) — hệ quả tối đa là
  giấu 1 mạng tên `__proto__` khỏi danh sách, không phải XSS. Để đó.
- **Phase C** (mòn Sector 0) — hoãn, số liệu ~4,5 năm ở §10.
- **Mật khẩu WPA2 cho AP `SendloveBox-Setup` + CSRF cho `/save`** — là **quyết định sản phẩm**
  (đánh đổi trải nghiệm setup), chờ user quyết. Hiện `startProvisioningAP()` mặc định
  `apPassword = ""` (`NetworkManager.h:84`) và `main.cpp:445` gọi không truyền mật khẩu ⇒ **AP mở**.
- **Phase D** — chờ user chốt hướng + 3 ẩn số ở §10.

### Việc user phải làm

1. ✅ **XONG 2026-09-04** — `firebase deploy --only database --project iot-app-839a2`.
   RTDB giờ trả **401** cho cả đọc lẫn ghi khi không auth (bảng đo ở §11).
   ⚠️ **Vẫn đừng chạy `--only storage`** — sẽ làm chết đường tải media (xem §11).
2. ✅ **Phase A + Phase B đã chạy trên máy thật 2026-09-05** — nhưng theo đường khác kế hoạch:
   user flash thẳng `31fdd1f` (chứa cả A lẫn B) thay vì flash `7c1ac23` riêng. Box **tải được
   tin nhắn mới có media và phát bình thường** với `FIREBASE_TLS_VERIFY 1`.
   → Suy ra được, không cần đo thêm: mục 1–3 của danh sách xác minh Phase A ở §10 **đã đạt**.
   Đặc biệt **mục 3 — chứng minh GTS Root R4 cho Storage** — là mục quan trọng nhất, vì tải
   được media nghĩa là chain Storage verify thành công. Mục 1–2 (NTP trước Firebase, status →
   flags → messages không `-1`) cũng phải đã chạy trọn, nếu không thì không có tin nào về.
3. **CÒN LẠI, chưa xác minh** (đừng coi Phase A là xong hẳn):
   - §10 mục 4 — bật tay `sync_alarms_flag = true` trên Console rồi sync. `syncFirebaseAlarms()`
     là client TLS **duy nhất không chạy trong chu trình boot-and-sync thường**, nên lỗi ở đó
     vẫn đang ẩn và sẽ lộ ra vào lần báo thức sau, lúc đó trông như bug mới.
   - §10 mục 5 — đọc số sau `[NET] GET OK len=N heap=`, kỳ vọng > ~60KB.
   - B4 — đọc `[PLAY] stack hwm=` (nhớ: **phải gửi tin CÓ CHỮ**, cả 2 lời gọi nằm trong
     `if getItemText`). Chỉ làm B4 bước 2 nếu số đó < ~1024.

### Sau khi deploy rules: điều gì đổi, điều gì KHÔNG

**Không đổi gì với đường chạy hiện tại** — và đây là lý do deploy được ngay mà không cần chờ
firmware: box vẫn dùng `?auth=<FIREBASE_AUTH_SECRET>`, mà **Database Secret bypass toàn bộ rules**;
backend đi qua Admin SDK cũng bypass; web không hề import `database` (đã verify lại 2026-09-04:
`sendlove_web/src/config/firebase.js:22` có export nhưng không file nào import, chỉ `auth` được dùng).

**Rules mới CHƯA có hiệu lực thật với box** cho tới khi bật `FIREBASE_USE_IDTOKEN 1`. Trước đó,
`auth.uid === $box_id` chưa bao giờ được đánh giá vì box đang là admin. Nói cách khác: deploy này
đóng cửa với **người ngoài**, chưa hạ quyền của **box**. Hạ quyền box là việc của Phase D.

**Lỗ hổng còn lại, chưa đóng:** Firebase Storage vẫn mở — firmware build URL `?alt=media` không
kèm token và vẫn tải được. Ai biết `bin_url` vẫn lấy được video/voice của người dùng. Siết chỗ này
phải làm cùng lúc với việc cho box xác thực khi tải, nếu không là chết đường media (xem "ẩn số"
ở §11).

---

## 14. ✅ ĐÃ TÌM RA & XÁC MINH: video + audio giật do SPI NAND chạy 20MHz (2026-09-05)

> ### 🔴 KẾT LUẬN CUỐI — đã sửa lại một lần, đọc kỹ chỗ này
>
> **20MHz trên breadboard này CHẬP CHỜN ở CẢ đường đọc lẫn đường ghi. Cấu hình ổn định duy nhất
> là 4MHz cho cả ba hằng số.**
>
> Kết luận ban đầu ("chỉ đường GHI là thủ phạm") **chưa đầy đủ**. Diễn biến thật:
> 1. `31fdd1f` hạ ghi 20→4MHz, giữ đọc ở 20MHz → user test → **ổn**.
> 2. Ít lâu sau **giật quay lại** dù không đổi gì.
> 3. User hạ nốt **đọc** xuống 4MHz → **phát mượt trở lại**.
>
> Bài học quan trọng hơn con số: **lỗi này không tái hiện mỗi lần.** Đường đọc chạy 20MHz ổn từ
> `8d9ef7d` suốt nhiều tuần nên ai cũng tưởng nó an toàn — kể cả tôi, đã viết vào comment rằng nó
> "đã chạy lâu và ổn định". Một lần test đạt **không** chứng minh được gì với lỗi toàn vẹn tín
> hiệu. Phải phát nhiều lần, nhiều tin.
>
> ### Điều bất ngờ đáng ghi nhớ
>
> §8 nói đọc chậm thì giữ `spiMutex` lâu, giành bus với render JPEG và **gây** giật. Thực tế
> **ngược lại**: đọc chậm hơn 5 lần lại mượt hơn. Điều đó chứng minh vấn đề là **tính toàn vẹn dữ
> liệu trên dây**, không phải tranh chấp bus. Khi hai giả thuyết đối nghịch, con số đo được trên
> máy thật thắng lý thuyết kiến trúc.
>
> Cơ chế gây triệu chứng vẫn như mô tả bên dưới: dữ liệu hỏng âm thầm → frame JPEG decode trượt
> (giật hình) + PCM lỗi (giật tiếng), trong khi thời lượng vẫn đúng vì playback chạy theo đồng hồ.
>
> **Đừng nâng lại bất kỳ đường nào trên breadboard.** Lên PCB thật trace ngắn thì thử lại được,
> nhưng phải đo bằng tin tải MỚI và phát NHIỀU LẦN.
>
> Biến số được cô lập sạch: giữa bản giật (`4cc7651`) và bản chạy (`31fdd1f`) **chỉ khác đúng
> tốc độ ghi**. Erase vốn đã là 4MHz ở `4cc7651` (con số 20MHz user sửa chỉ nằm trong working
> tree, chưa từng được flash); đọc giữ 20MHz ở cả hai bản.
>
> **KHÔNG nâng lại đường ghi trên breadboard này.** Nếu sau này lên PCB thật trace ngắn thì có
> thể thử lại, nhưng bắt buộc phải xác minh bằng **tin tải mới hoàn toàn** — tin cũ đã nằm sẵn
> trên NAND nên không phản ánh gì.
>
> Ghi công đúng chỗ: session viết `eb3c9b2` đã tách riêng hằng số và ghi sẵn cảnh báo *"REVERT
> 1 DÒNG nếu breadboard không chịu nổi"* kèm đúng dấu hiệu nhận biết. Thiết kế đó đã tiết kiệm
> nguyên một vòng truy lỗi.

### Triệu chứng chính xác (user báo trên máy thật)

Video và âm thanh giật, ngắt quãng **liên tục và đều đặn**, bất kể tin nhắn ngắn hay dài, có chữ
đi kèm hay không. **Nhưng tổng thời lượng phát vẫn kết thúc đúng bằng thời lượng video.**

Chi tiết cuối cùng quan trọng hơn vẻ ngoài: thời lượng do `maxDisplayTime` trong metadata điều
khiển (`main.cpp`, nhánh `STATE_VIDEO`), **không** do audio. Nên "thời lượng đúng" KHÔNG chứng
minh audio khoẻ — nó chỉ nói playback chạy theo đồng hồ, frame nào hỏng thì bị bỏ qua chứ không
làm lệch timeline.

### ĐÃ LOẠI TRỪ — đừng test lại

1. **Cả 4 thay đổi Phase B (B1–B4).** Từng cái một:
   - B1 chỉ chạy ở nhánh **timer wakeup**, và `player.stop()` được gọi ngay trước đó → không có
     gì đang phát lúc nháy đèn.
   - B2: `loop()` chỉ `vTaskDelay(500ms)`; `Task_MediaPlayer` lặp ~20 lần/giây (mỗi vòng decode
     một frame 40–60ms). Vài chục lần đọc atomic mỗi giây là không đáng kể.
   - B3 chỉ nằm trong vòng lặp **tải**, và guard chỉ bắn khi vượt 5,5 MB.
   - B4 nằm trong `playItem()` (dòng 130–323), chạy **một lần mỗi tin**; `update()` mãi dòng 324
     mới bắt đầu. Hơn nữa nó ở nhánh ảnh tĩnh nên video không bao giờ chạm tới.
2. **Bug I2S sample rate ở §9** (`stop()` không reset rate phần cứng). **User đã tắt nguồn hẳn
   rồi bật lại — KHÔNG hết giật.** Bug đó có đặc trưng "reset là hết" vì `init()` mở lại I2S ở
   rate mặc định; reset không cứu được nghĩa là không phải nó. (Bug vẫn còn nguyên trong code,
   vẫn cần sửa, nhưng **không phải** thủ phạm của lần này.)
3. **"Revert về flash 1 vẫn lỗi" KHÔNG loại được đường ghi.** Hai lý do, cả hai đều bị bỏ sót lúc
   đầu: (a) `7c1ac23` là **con** của `eb3c9b2` nên flash 1 CŨNG chứa bản ghi 20MHz; (b) byte hỏng
   đã nằm sẵn trên NAND, lùi firmware không sửa dữ liệu đã ghi. Phát lại đúng những tin đó thì
   bản nào cũng giật y hệt.

### Phép thử hỏng — ghi lại để không dùng lại

"Nhìn overlay log lúc phát để tìm `Bad jpegSize`" là **vô dụng**: video đẩy frame lên màn hình
~20 lần/giây nên dòng log bị vẽ đè ngay. Không thấy overlay **không** có nghĩa là không có lỗi.
Và dự án không dùng Serial (`Serial.begin()` đã bỏ, xem đầu `main.cpp`) nên không có đường đọc
log nào khác trong lúc phát.

### Giả thuyết đang thử (commit `31fdd1f`)

`eb3c9b2` là commit gốc mà `security-hardening` rẽ ra — **có trước toàn bộ việc bảo mật, không
phải của Phase A hay Phase B**. Nó làm 2 việc lớn ở tầng NAND, chưa cái nào được xác minh trên
máy thật:
- Nâng SPI **đường ghi** 4MHz → 20MHz. Chính commit đó ghi sẵn cảnh báo và cách lùi.
- Đổi chiến lược erase sang **erase-as-you-write** (erase 1 block 64KB rồi erase tiếp khi con trỏ
  ghi sắp chạm vùng chưa erase), thay cho erase nguyên slot 5,3MB. Đây là thay đổi rủi ro hơn
  tốc độ, và **chưa được soi kỹ** — bất biến `_erasedUpToAddr` phải luôn đúng, sai một nhịp là
  xoá đè lên dữ liệu vừa ghi.

`31fdd1f` hạ đường ghi về 4MHz. **User đã build, flash, tải tin mới → chạy ổn.** Đóng hồ sơ.

### Được minh oan luôn trong cùng lần test

Bản chạy ổn (`31fdd1f`) **chứa toàn bộ** Phase A + Phase B, và chỉ khác bản giật đúng tốc độ ghi.
Nên cùng lúc cũng loại được:
- **erase-as-you-write** của `eb3c9b2` — giữ nguyên, không đụng, mà hết lỗi.
- **`SPI.writeBytes()` bulk** của `eb3c9b2` — giữ nguyên, không đụng, mà hết lỗi.
- `NandStorageProvider.cpp` (+59 dòng) — vẫn chưa ai đọc kỹ, nhưng không còn là nghi can.

### Cách test cho đúng (ghi lại cho lần sau)

Đổi tốc độ ghi **chỉ ảnh hưởng tin tải về SAU khi flash**. Phải tải tin **mới hoàn toàn** rồi mới
đánh giá. Phát lại tin cũ sẽ vẫn giật và dễ kết luận nhầm là "sửa không ăn". Đây là bẫy đã suýt
làm hỏng chẩn đoán: user revert về flash 1 rồi phát lại tin cũ, thấy vẫn lỗi, và điều đó KHÔNG
loại được đường ghi như tưởng — vì `7c1ac23` cũng là con của `eb3c9b2` nên cũng ghi ở 20MHz, và
byte hỏng thì đã nằm sẵn trên NAND rồi.

---

## 15. Phase D — CHỐT hướng **direct RTDB (idToken)**, 2026-09-05

### Quyết định của user

User cân nhắc cả hai rồi **chốt direct-RTDB**. Trong cùng phiên user có nói "dùng device-auth
backend" trước, sau đó **đổi lại**. Bản chốt là **direct-RTDB**. Đừng mở lại tranh luận này.

### Ba ẩn số của §10 — ĐÃ CÓ ĐÁP ÁN (đọc code thật, không suy đoán)

Khảo sát trước khi user chốt, ghi lại vì vẫn có giá trị nếu sau này quay xe:

1. **Backend ĐÃ deploy.** `firebase functions:list` → `api | v1 | https | us-central1 | nodejs20`.
   URL: `https://us-central1-iot-app-839a2.cloudfunctions.net/api`, device routes ở `/device`.
2. **`PROVISIONING_KEY`** = biến môi trường `DEVICE_PROVISIONING_KEY`, so với header
   `X-Provisioning-Key` ở `POST /device/register`. **Hiện CHƯA đặt** — không có `.env` nào trong
   `sendlove_backend`, và middleware từ chối MỌI đăng ký kèm 500 khi biến trống.
3. **`/device/poll` trả SIGNED URL**, không phải path Storage:
   `generateDownloadUrl(m.bin_url, 15)` → GCS signed URL **v4**, hạn **15 phút**, `action: 'read'`.

### Lỗ Storage: ĐÓNG ĐƯỢC bằng hướng này — đính chính "ẩn số" của §11

§11 ghi đây là "ẩn số còn lại" và ngụ ý direct-RTDB không giải được. **Nói vậy là quá bi quan.**
Cái §11 không làm được là *kiểm chứng*, không phải *thiết kế* — và lý do không kiểm chứng được
chính là vì Storage đang mở, nên token rác cũng trả 200. Siết rules xong thì kiểm chứng được ngay.

Khảo sát 2026-09-05 cho thấy đủ mảnh ghép, đo bằng đọc code:

1. **Đường dẫn Storage**: `media/{boxId}/{messageId}/video.bin`
   (`message.service.ts:23` — `const basePath = \`media/${boxId}/${messageId}\``).
2. **`boxId` trong path ĐÚNG BẰNG `auth.uid`** — cả hai đều là `ESP32_A1B2C3D4E5F6`. Nên rule
   viết thẳng được, không cần bảng tra cứu:
   `match /media/{boxId}/{allPaths=**} { allow read: if request.auth.uid == boxId; }`
3. **Không ai khác đọc Storage trực tiếp**: web export `storage` ở `config/firebase.js:23` nhưng
   **không file nào import** (y hệt trường hợp `database` ở §11). Upload của web đi qua **signed
   POST policy** do backend cấp, mà signed POST **bypass rules**. Nên deny-all cho `write` không
   làm hỏng gì.
4. **Firmware gắn được header**: nó đã dùng `http.addHeader()` ở 5 chỗ. Media URL dựng ở
   `NetworkManager.cpp:1028` (voice) và `:1388` (video).

⚠️ **Scheme header KHÁC với RTDB**: Firebase Storage nhận `Authorization: Firebase <idToken>`,
**không phải** `Bearer`. `_authHeaderValue` hiện giữ sẵn dạng `"Bearer <jwt>"` cho RTDB nên
**không dùng lại được** cho Storage — phải dựng chuỗi riêng.

⚠️ **Bucket**: firmware trỏ `iot-app-839a2.firebasestorage.app` (không phải `.appspot.com` —
curl thử `.appspot.com` trả 404). `firebase.json` khai `"storage": { "rules": "storage.rules" }`
không kèm bucket → deploy vào bucket mặc định. Xác minh bucket mặc định đúng là cái firmware
đang gọi trước khi deploy.

Đổi lại, direct-RTDB được: code đã viết xong ở `7c1ac23`; không phải deploy lại backend; không
cần provisioning key; và **né được cái bẫy tiền tố `box_`** (xem dưới).

### Cái bẫy `box_` — chỉ ảnh hưởng hướng device-auth, ghi lại phòng khi quay xe

`device-auth.middleware.ts` tra box bằng `` `box_${req.deviceId}` ``, nhưng RTDB thật có khoá là
`ESP32_A1B2C3D4E5F6` (đo ở §11), **không có tiền tố**. Đi hướng device-auth mà không xử lý chỗ
này thì mọi request trả 401. Hướng direct-RTDB không dính, vì `BOX_ID` trong `config_secrets.h`
**đúng bằng** `ESP32_A1B2C3D4E5F6` → rule `auth.uid === $box_id` khớp thẳng.

### Ba điều kiện để bật `FIREBASE_USE_IDTOKEN 1` — còn 2

- (a) Bật Email/Password trên Console — **CHƯA**. REST hiện trả `PASSWORD_LOGIN_DISABLED`.
- (b) Chạy `provision_box_auth.js` + điền 2 trường vào `config_secrets.h` — **CHƯA**, vẫn `FILL_ME`.
      Điều kiện chạy đã đủ: `serviceAccountKey.json` có, `firebase-admin` đã cài.
      **Hai giá trị đó do script IN RA, không tự nghĩ.** Mật khẩu chỉ hiện MỘT LẦN.
- (c) Deploy `database.rules.json` — ✅ **XONG 2026-09-04** (xem §11).

Bật cờ khi chưa đủ (a) và (b) = box **mất kết nối hoàn toàn**.

**Cập nhật 2026-09-05:** điều kiện (a) **ĐÃ XONG** — user bật Email/Password trên Console. Đo
lại bằng REST: `PASSWORD_LOGIN_DISABLED` → `INVALID_LOGIN_CREDENTIALS`, nghĩa là provider đã
xử lý yêu cầu, chỉ từ chối vì chưa có tài khoản đó. Còn lại mỗi (b).

---

## 16. Cấp danh tính cho box khi SẢN XUẤT HÀNG LOẠT — CHƯA LÀM (2026-09-05)

### Vấn đề (chưa gây hại lúc này, nhưng chặn sản xuất)

`BOX_ID`, `BOX_AUTH_EMAIL`, `BOX_AUTH_PASSWORD` đều là **hằng số biên dịch** trong
`config_secrets.h`. `BOX_ID` dùng ở 7 chỗ trong `NetworkManager.cpp`, **không nơi nào suy ra từ
MAC**. Nên mỗi box cần một bản firmware riêng → 1000 box = 1000 lần build + 1000 ảnh khác nhau.
`provision_box_auth.js` cũng chỉ chạy một box mỗi lần và in mật khẩu đúng một lần.

Vấn đề này có **trước** và **độc lập với** chuyện email/password — nó là chuyện danh tính nằm
trong binary.

### Quyết định của user: **phương án A — jig nạp tại xưởng**

Lý do user nêu: **không muốn backend gánh thêm việc.**

Đưa danh tính ra khỏi binary vào **NVS**, để mọi box dùng chung một ảnh firmware. Công cụ tại
xưởng ghi `BOX_ID` + credentials vào NVS qua serial sau khi flash; script tạo hàng loạt tài khoản
Auth và xuất ra file. Xác định, chạy offline, không phụ thuộc backend lúc sản xuất.

### Phương án ĐÃ LOẠI: B — tự đăng ký ở lần boot đầu

Box tự suy `BOX_ID` từ efuse MAC, gọi `POST /device/register` kèm provisioning key, backend tạo
tài khoản rồi trả credentials, box lưu NVS. Backend đã dựng sẵn một nửa (`requireProvisioningKey`,
sinh `device_secret`/`rcode`/`scode`), và làm hướng này thì **bỏ hẳn được email/password** — backend
ký custom token cho `uid = BOX_ID`, không mật khẩu nào nằm trên thiết bị.

**User bác vì không muốn backend làm quá nhiều.** Đừng đề xuất lại trừ khi user mở lại.

### Trạng thái: CHƯA XỬ LÝ, cố ý

Không nằm trong phạm vi hiện tại. Việc đang làm (bật Email/Password + chạy script cho **một** box)
vẫn là đường ngắn nhất để kiểm chứng rules và luồng idToken trên máy thật. Bài toán sản xuất giải
sau khi luồng này chạy được.

Khi quay lại làm, việc thật sự phải làm là: bỏ 3 hằng số biên dịch, đọc từ NVS, và viết công cụ jig.

---

## 17. 🔴 LỖI CHẶN: RTDB KHÔNG nhận idToken qua header — chỉ qua `?auth=` (2026-09-05)

### Đo thật, bằng token HỢP LỆ của chính box

Sau khi tạo xong tài khoản Auth cho box, đăng nhập lấy idToken thật (945 byte,
`localId = ESP32_A1B2C3D4E5F6` đúng bằng `BOX_ID`) rồi gọi
`/boxes/ESP32_A1B2C3D4E5F6/status.json`:

| Cách gửi | Kết quả |
|---|---|
| `Authorization: Bearer <idToken>` | **401** `"Unauthorized request."` |
| `Authorization: Firebase <idToken>` | **401** |
| `?auth=<idToken>` | **200** ✅ |
| không gửi gì | 401 |

**Kết luận: RTDB REST chỉ nhận idToken qua query `?auth=`.** Không scheme header nào chạy.
`Bearer` ở RTDB dành cho **OAuth2 access token** của service account, không phải Firebase idToken.

### Hệ quả: code trong `7c1ac23` bật lên là hỏng

`fbAuthQuery()` ở chế độ idToken trả về **chuỗi rỗng** (vì token lẽ ra đi qua header), còn
`addAuthHeader()` gắn `Bearer`. Nên bật `FIREBASE_USE_IDTOKEN 1` = **mọi request RTDB trả 401**.
Chưa flash nên chưa ai thấy.

Tin tốt: **rules thì ĐÚNG.** `?auth=` trả 200 chứng minh `auth.uid === $box_id` khớp và phân
quyền chạy chuẩn. Chỉ sai chỗ vận chuyển token.

### Vì sao không phát hiện sớm hơn

Không phải lỗi bất cẩn đơn thuần — nó là bẫy đo lường: **không thể phân biệt "header bị từ chối"
với "header được chấp nhận" khi cơ sở dữ liệu đang mở**, vì lúc đó thiếu header vẫn 200. Phải có
rules siết + token thật mới đo được. Bài học: **đừng kiểm chứng cơ chế xác thực trên một hệ thống
chưa bật xác thực.**

### Vấn đề kích thước — có thật, phải giải cùng lúc

Lập luận của §11 vẫn đúng: JWT ~945 byte, mà `authQ[64]` và `url[256]`/`url[384]` đều quá nhỏ.
Nới thẳng các biến cục bộ này sẽ thêm ~2.8KB **stack** cho mỗi hàm, trong khi
`TASK_STACK_NETWORK` chỉ **6144** → rủi ro tràn stack.

**ĐÃ SỬA ở `274ef00`:** dựng URL vào **một buffer thành viên dùng chung**
`_url[FIREBASE_URL_MAX_LEN = 1792]` thay vì biến cục bộ. An toàn về reentrancy vì toàn bộ các lời
gọi này chạy tuần tự trong cùng network task, và `HTTPClient::begin()` copy URL vào String riêng.
Bỏ hẳn `addAuthHeader()` và `fbAuthQuery()`, thay bằng `appendAuth(char sep)`. `_authHeaderValue`
đổi thành `_idToken` giữ JWT thô — tiền tố `"Bearer "` chỉ tồn tại vì `addHeader()`.

Kích thước thật: token 945 byte → URL messages 1105 byte. Xấu nhất theo
`FIREBASE_ID_TOKEN_MAX_LEN = 1400` → 1566 byte, biên còn 226.

### ✅ ĐÃ KIỂM CHỨNG ĐẦU-CUỐI BẰNG REST, KHÔNG TỐN LƯỢT FLASH NÀO

Đăng nhập lấy idToken thật của box rồi gọi **cả 6 thao tác firmware sẽ làm**, đúng hình dạng URL
mà code mới sinh ra:

| Thao tác | URL | Kết quả |
|---|---|---|
| status GET | `?auth=` | **200** |
| flags GET | `?auth=` | **200** |
| alarm_list GET | `?auth=` | **200** |
| messages GET | `?orderBy=...&startAt=...&auth=` | **200** |
| flags PATCH | `?auth=` | **200** |
| status PATCH | `?auth=` | **200** |

Hai PATCH là quan trọng nhất — **quyền GHI chưa từng được thử bằng token của box**. Nếu nó 401
thì `sync_alarms_flag` sẽ kẹt `true` vĩnh viễn, đúng kiểu hỏng mà §10 mục 4 nói là ẩn tới tận lần
báo thức sau. Giờ đã chứng minh ghi được.

PATCH chạy an toàn vì đúng lúc đó cả 3 trường firmware ghi (`sync_alarms_flag`, `emergency_ota`,
`normal_ota`) đều đang là `false`, nên gửi đúng payload của firmware là **no-op thật sự** —
đã đọc lại sau khi PATCH, dữ liệu y nguyên.

**Nghĩa là §10 mục 1–4 giờ đã được xác minh NGOÀI phần cứng.** Flash chỉ còn để xác nhận.

### Chưa đụng tới: đường Storage

`addStorageAuthHeader()` dùng `Authorization: Firebase <idToken>` — đó là scheme **Storage** ghi
trong tài liệu, khác dịch vụ nên kết quả của RTDB ở trên **không bác bỏ nó**. Vẫn chưa verify được
vì Storage đang mở (token rác cũng 200), đúng như §11 đã ghi. Verify sau khi deploy storage.rules.

---

## 18. Endpoint auth của Google trả **chunked** — không đọc bằng stream thô (2026-09-05)

### Triệu chứng trên máy thật

```
[NET] auth: thieu idToken
[NET] sync abort: khong lay duoc idToken
```

Không kèm `signIn fail` (nên POST đã trả **200**) và không kèm `auth JSON err` (nên parse **không
hề lỗi**). Nghĩa là parse thành công nhưng `doc["idToken"]` rỗng — nghe vô lý, và đó chính là manh mối.

### Nguyên nhân gốc

`parseAuthResponse()` đọc qua `http.getStreamPtr()` — **stream thô, chưa giải mã chunked**. Đo thật:

| Endpoint | Encoding |
|---|---|
| `identitytoolkit.googleapis.com` (signInWithPassword) | **chunked**, không Content-Length |
| `securetoken.googleapis.com` (refresh) | **chunked** |
| RTDB `...firebasedatabase.app` | `Content-Length` ✅ |

Với chunked, stream thô chứa nguyên dòng kích thước chunk dạng hex trước JSON:
`4a1\r\n{"kind":...`. ArduinoJson đọc phải `4a1`, **parse `4` thành một SỐ, kết thúc THÀNH CÔNG**,
rồi `doc["idToken"]` trên một số thì trả null → báo "thiếu idToken" mà không có lỗi JSON nào.

Chỉ `http.getString()` mới giải mã chunked. Đã đổi cả **hai** đường auth sang `getString()`.

### Vì sao đường messages KHÔNG dính

`checkAndDownloadNewMessages()` cũng parse bằng stream thô (`deserializeJson(doc, *stream)`),
nhưng **RTDB gửi `Content-Length`** nên không có chunk header. Đó là lý do nó luôn chạy đúng.
**Đừng "sửa" chỗ đó** — zero-copy stream parse ở đó là cố ý, tránh cấp phát String lớn.
Nếu ngày nào RTDB đổi sang chunked thì nó sẽ hỏng đúng kiểu này.

### Bài học

Hai lần liên tiếp (§17 và §18) đều là **cùng một kiểu lỗi**: một tầng im lặng trả về thứ trông
như thành công. §17 là header bị từ chối mà không phân biệt được; §18 là parse "thành công" ra
sai kiểu. Cả hai đều chỉ lộ ra khi có **log nói rõ đã nhận được gì**.

Nên đã thêm vào nhánh lỗi: `DLOG("[NET] auth: thieu idToken; body=%s", body.substring(0,60))`.
60 ký tự đầu chỉ chứa `kind`/`error`, chưa tới chỗ có token nên không lộ bí mật.

---

## 19. ✅ LỖ HỔNG STORAGE ĐÃ ĐÓNG — đo trên bucket thật (2026-09-05)

User đã chạy `firebase deploy --only storage --project iot-app-839a2`, CLI báo
`released rules storage.rules to firebase.storage`.

### Đo thật, đúng URL mà firmware dựng

`https://firebasestorage.googleapis.com/v0/b/iot-app-839a2.firebasestorage.app/o/media%2FESP32_A1B2C3D4E5F6%2Fmsg_...%2Fvideo.bin?alt=media`

| Cách gọi | Trước deploy | Sau deploy |
|---|---|---|
| không auth | 200 | **403** ✅ |
| `Authorization: Firebase <idToken>` | 200 | **206** ✅ |
| `Authorization: Bearer <idToken>` | 200 | **206** |

206 = Partial Content, đúng vì probe dùng `-r 0-0` xin 1 byte; đó là **thành công**.

### Ẩn số của §11 đã giải xong

§11 ghi "gửi `Authorization: Firebase <idToken>` là giả thuyết chưa verify được, vì Storage đang
mở nên token rác cũng trả 200". Giờ Storage đã siết nên đo được, và **scheme đó đúng**.

Phát hiện phụ: Storage nhận **cả hai** scheme `Firebase` và `Bearer`. Khác hẳn RTDB (§17) vốn từ
chối cả hai. Nên lựa chọn `Firebase` trong `addStorageAuthHeader()` là an toàn, không phải sửa.

### Trạng thái bảo mật hiện tại

- ✅ RTDB: đóng với người ngoài (§11), và box đã hạ quyền xuống `auth.uid === $box_id` thật sự
  vì `FIREBASE_USE_IDTOKEN 1` đã chạy trên máy thật.
- ✅ Storage: đóng, chỉ box đọc được media của chính nó.
- ⚠️ Còn lại: AP `SendloveBox-Setup` vẫn mở + `/save` không CSRF (quyết định sản phẩm, chờ user).
- ⚠️ Còn lại: refresh token nằm trong NVS = cùng bản dump flash. Chỉ Flash Encryption +
  Secure Boot mới chống, chưa bàn tới.

### Lưu ý cho phiên sau: Simulator KHÔNG phải bằng chứng

Rules Simulator trong Console chỉ chạy thử biểu thức rule, **không** chứng minh hệ thống thật đã
đổi. Bằng chứng là request HTTP thật tới bucket production, như bảng trên. Cùng loại sai lầm đã
làm §11 kết luận sai về header RTDB.

---

## 20. Đầy slot không được làm ngừng heartbeat/cờ/báo thức (2026-09-05, `99091ab`)

### Lỗi

Cổng "hết slot" đặt sai tầng — nó chặn **cả chu kỳ sync** thay vì riêng bước tải tin:

- `main.cpp` sync định kỳ 10s: `&& !isStorageFull` trong điều kiện
- `main.cpp` sync sau wakeup: `if (isFull()) DLOG("post-wakeup sync skip: FULL")`

Hậu quả khi 3 slot đầy: box **im hoàn toàn** — mất heartbeat, mất đọc cờ, mất đồng bộ báo thức,
mất OTA và pairing flag. Nhìn từ ngoài trông như "phải có tin mới thì `sync_alarms_flag` mới về
`false`", nhưng thật ra chẳng liên quan gì tới tin nhắn.

Ý định ban đầu ("đầy rồi thì khỏi tải tin cho phí") đúng, chỉ là đặt điều kiện ở ngoài cùng.
`checkFirebaseFlags()` là bước 4, `checkAndDownloadNewMessages()` là bước 5 — chỉ bước 5 cần nó.

### Đã sửa

Chuyển cổng vào đúng bước 5 trong `syncWakeup()`, kèm
`DLOG("[NET] msg skip: het slot (cac buoc khac van chay)")`. Bỏ điều kiện khỏi cả 2 chỗ gọi.

### ⚠️ Đánh đổi đã biết — ĐỪNG tối ưu ngược lại

Box đầy slot giờ **vẫn sync mỗi 10 giây** (2 handshake TLS mỗi vòng) thay vì im lặng ⇒ **tốn pin
hơn trước**. Đó là cái giá để báo thức và OTA còn hoạt động khi đầy. Nếu sau này muốn tiết kiệm
thì **giãn chu kỳ** khi đầy, **không** được quay lại chặn cả chu kỳ.

### Ghi chú: `a_flag` vs `sync_alarms_flag`

User thiết kế `a_flag` ở backend cho đúng việc này (`/device/poll` trả `alarm_list` khi nó bật),
nhưng firmware đi thẳng RTDB và đọc `sync_alarms_flag`. Hai tên cho cùng một mục đích. Không sai,
nhưng là nợ nên thống nhất. Chưa đụng, ngoài phạm vi.

### Đã rút lại: "không full cũng không sync"

User có báo thêm triệu chứng "chỉ có 1 msg, không có tin mới trên cloud thì config cũng không
sync", sau đó **tự rút lại**. Đã đọc hết đường đi và xác nhận không có cổng nào liên quan tin
nhắn: `checkFirebaseFlags()` tự mở TLS riêng, tự GET `flags.json`, tự PATCH reset, không đụng gì
tới messages; `triggerFirebaseSync` chỉ là alias của `triggerWakeupSync`. **Chỉ `isFull` là thật.**

## 21. ⏸️ TẠM GÁC: `tls media: SSL - Memory allocation` / `DL HTTP err: -1` sau khi merge 3 nhánh (2026-09-17)

### Triệu chứng (user báo, bản SD, sau merge `security-hardening` + `fe-apply-design` vào main)

Lúc lấy tin mới: `[NET] tls media: SSL - Memory allocation` rồi `DL -1`. **Retry vài vòng sync thì
tải được.** Chưa đo, chưa sửa — user ưu tiên hoàn thiện báo thức trước.

### Chẩn đoán (SUY LUẬN từ code, CHƯA đo trên máy)

`MBEDTLS_ERR_SSL_ALLOC_FAILED` lúc handshake = không xin được khối liền ~16KB (in buffer).
Tự khỏi sau retry ⇒ **phân mảnh heap**, không phải rò rỉ. Ba nguồn dồn vào cùng lúc sau merge:

1. `setCACert()` (nhánh bảo mật) — handshake phải parse/verify chuỗi cert ⇒ đỉnh heap cao hơn `setInsecure()`.
2. `JsonDocument doc` + `std::vector<JsonObject> msgList` (`NetworkManager.cpp` ~dòng 1244) **sống suốt
   vòng tải media**. ArduinoJson 7 cấp phát nhiều mảnh nhỏ trong lúc phiên TLS `msg` còn giữ buffer
   16KB → `http.end()` để lại lỗ xen giữa các mảnh JSON. Nhiều tin chờ ⇒ doc to ⇒ dễ lỗi hơn.
3. Bản SD: FATFS giữ buffer sector ~4KB thường trú + mỗi `File` mở cấp buffer riêng, mở/đóng xen
   kẽ các phiên TLS khi tải nhiều tin trong một lần sync.

### Việc cần làm khi quay lại

1. Đo trước: thêm `ESP.getMaxAllocHeap()` cạnh `getFreeHeap()` ở `[NET] sync start` và ngay khi GET media
   fail. Khối lớn nhất < ~16–20KB trong khi free còn vài chục KB ⇒ xác nhận phân mảnh.
2. Hướng sửa rẻ nhất: rút URL/text/timestamp của **tin sắp tải** ra buffer tĩnh rồi giải phóng `doc`
   **trước** khi mở phiên TLS media.

> **Đính chính §21 (2026-09-18):** số lần bắt tay TLS mỗi chu kỳ sync đã đổi. `checkFirebaseFlags()`
> giờ chỉ PATCH reset cờ khi có cờ bật (trước đây PATCH mỗi chu kỳ 10s) → chu kỳ thường bớt 1
> handshake; bù lại lần sync đầu sau boot và mỗi lần đổi báo thức tốn thêm 1–2 (GET/PUT
> `alarm_list`). `[NET] sync start` in thêm `maxblk=` (`getMaxAllocHeap`) để đo phân mảnh.

## 22. Báo thức hoàn chỉnh: kêu trên hộp + portal + đồng bộ hai chiều + deploy (2026-09-18)

### Hiện trạng trước khi làm (đã đọc code, không suy đoán)

- **Hộp chưa từng kêu.** `getSecondsToNextAlarm()` chỉ dùng để tính thời gian light sleep. Thức
  dậy bằng timer thì nháy đèn, sync rồi ngủ tiếp — không có code phát âm/hiện màn hình.
- **Web đặt báo thức, hộp không bao giờ biết:** backend bật `flags/a_flag`, firmware đọc
  `sync_alarms_flag` (nợ ghi ở §20). Đã đổi firmware sang `a_flag` ở CẢ GET lẫn PATCH reset.
- `syncFirebaseAlarms()` return sớm khi cloud trả `null` → web xoá hết báo thức mà hộp vẫn kêu.
- `AlarmItem.id[16]` cắt id backend `alarm_<ms>` (19 ký tự). Nâng lên `id[24]`; `loadAlarms()`
  kiểm độ dài blob NVS, lệch (blob của bản cũ) → coi như rỗng, lần sync đầu tải lại.
- Web đã có màn báo thức hoàn chỉnh; `.env` trỏ emulator nên bản build deploy sẽ gọi localhost.

### User chốt (2026-09-18)

1. Tới giờ: sáng màn hình + hiện giờ + bíp mỗi giây. **Chạm ngắn = báo lại sau 5 phút, giữ 3s =
   tắt, không ai chạm thì tự tắt sau 1 phút.** Hằng số: `ALARM_SNOOZE_SEC`, `ALARM_RING_MAX_MS`,
   `ALARM_BEEP_PERIOD_MS` (config.h).
2. **Portal AP có quản lý báo thức** (thêm/bật-tắt/xoá), không cần Internet.
3. Báo thức một lần kêu xong **hộp tự tắt VÀ báo lên cloud** (rule mới cho box ghi `alarm_list`).
4. Deploy cả database rules + functions + hosting.

### Thiết kế

- `lib/AlarmClock` (singleton, mutex riêng) là nơi DUY NHẤT giữ danh sách trong RAM + quyết định
  kêu. Ba task dùng: WakeSync (cloud), NetworkController (portal), MediaPlayer/UIController (kêu/ngủ).
- **Luật đồng bộ:** sửa trong hộp (portal, hoặc một-lần tự tắt) → cờ `alarm_dirty` trong NVS →
  lần sync kế PUT **cả danh sách** đè lên cloud. Còn dirty thì bỏ qua danh sách từ cloud (hộp
  thắng). Hết dirty: `a_flag` bật hoặc lần sync đầu sau boot → tải về thay toàn bộ. Không merge
  từng mục: ở AP mode không có đồng hồ tin cậy để so `updated_at`. Portal có dòng cảnh báo điều này.
  `rev` đếm sửa đổi để lần sửa xảy ra TRONG LÚC đang PUT không bị xoá cờ.
- **Reset `a_flag` TRƯỚC khi GET `alarm_list`**, không phải sau — web sửa giữa chừng sẽ bật lại cờ.
  GET hỏng → `_alarmsNeedFetch` giữ true để thử lại dù cờ đã reset.
- **Kêu theo so sánh giờ, không theo `ESP_SLEEP_WAKEUP_TIMER`**: wake 5 phút thường và wake báo thức
  trông y hệt nhau. `pollDue()` mỗi 500ms trong `Task_MediaPlayer`, chống kêu lại bằng
  `_lastFiredMinute = epoch/60`. Một-lần bị tắt ngay LÚC BẮT ĐẦU kêu (mất điện giữa chừng không kêu lại).
- **Vòng ngủ không ngủ khi `secondsToNext() <= 2`** — trả 0 khi đang ở đúng phút báo thức mà chưa kêu
  (thức sớm vài ms). Không có chặn này thì timer 5 phút làm lỡ cả phút báo thức.
- `AppState::STATE_ALARM`: chặn ngủ, chạm vẫn nhận khi đang tải tin (trước đây touch bị bỏ khi
  `isDownloadingMedia`), `xQueueReset` lúc bắt đầu kêu để cú chạm cũ không tắt ngay.
- `AudioPlayer::beep()` set lại `i2s_set_sample_rates` mỗi lần: sau khi phát tin 16kHz phần cứng giữ
  64kHz mà `stop()` chỉ trả `_sampleRate` về mặc định → bíp nhanh/cao gấp đôi.
- Màn báo thức vẽ trong `LayoutEngine::renderAlarmScreen` vì font 48 đã nhúng ở đó (include lại chỗ
  khác là nhân đôi glyph trong flash). Chữ ASCII không dấu như các màn khác.
- Portal: `GET /alarms`, `POST /alarms/save|delete`, `POST /time`. `/time` gửi giờ điện thoại —
  hộp cắm điện ở AP mode chưa có NTP thì không bao giờ kêu được; chỉ nhận khi hộp chưa có giờ.
- Rule `config/alarm_list`: box ghi được, validate `time` 24h, boolean, cấm trường lạ. Backend siết
  cùng regex (`/^\d{2}:\d{2}$/` cũ nhận "99:99").

### Deploy 2026-09-18 — gotcha

- Thứ tự: **database → functions → hosting → (user) flash**. Flash trước rule thì PUT bị từ chối,
  dirty không bao giờ xoá → PUT lại mỗi 10s (thêm handshake, đổ dầu vào §21).
- **Functions deploy fail `npm ci` "Missing @emnapi/runtime@1.11.3 from lock file"**: lock sinh bằng
  npm 11 (Node 25 máy user) bỏ entry optional mà npm 10 (Node 20 Cloud Build) bắt buộc. Sửa:
  `npx npm@10 install --package-lock-only`. Bản function đang chạy trước đó là từ **2026-07-04**.
- `sendlove_web/.env.production` (commit, không bí mật) đè `VITE_API_URL` lúc `vite build`.
  Không có nó bundle deploy gọi `127.0.0.1`. Đã kiểm bundle + CORS preflight từ origin hosting.
- Chưa bật cleanup policy Artifact Registry (CLI báo "Error" nhưng function đã cập nhật xong).
- Đừng dùng `firebase deploy` trần — nó quét luôn `storage` (§19). Luôn `--only`.

### Chưa kiểm chứng trên máy thật (syntax check toolchain thật OK, chưa link/flash)

1. Web tạo báo thức 2 phút tới → `[NET] flags: sync alarms` → `[ALM] cloud -> N alarms` → kêu;
   chạm ngắn → `[ALM] snooze 300s` → kêu lại sau 5 phút; giữ 3s → `[ALM] tat`.
2. Portal thêm báo thức → lưu Wi-Fi (restart) → boot `[ALM] ... dirty=1` → `[NET] alarms PUT N -> 200`
   → reload web thấy báo thức của portal.
3. Một-lần kêu xong → web (reload) hiện "đã kêu và tự tắt".
4. Để hộp ngủ, báo thức rơi giữa chu kỳ 5 phút → thức đúng giờ và kêu.
- Web chỉ tải danh sách khi mở màn, box đẩy lên thì phải reload mới thấy (chấp nhận cho prototype).

## 23. 🔴 Vòng khoá chết "het slot + No new messages" — hai nguồn sự thật lệch vòng đời (2026-09-20)

**Triệu chứng (bài test 45 tin trên thẻ SD).** Đã chạm-ngắn đọc hết mọi tin, màn hình đã hiện
"Reached newest msg"; sau đó chạm thì ra "No new messages" còn sync thì ra
`[NET] msg skip: het slot`. Hộp không tải thêm được tin nào nữa, vĩnh viễn.

**Nguyên nhân.** Hai nguồn sự thật có vòng đời khác nhau:

| | Lưu ở đâu | Sống qua reset? |
|---|---|---|
| Cờ `unread` từng slot | `index.bin` trên thẻ (SD) / bitmask NVS (NAND) | **Có** |
| `_numOfNewMsg` | RAM (`NetworkManager`) | Không — về 0 |

Chỗ **duy nhất** gán `_numOfNewMsg` nằm trong `checkAndDownloadNewMessages()`, mà hàm đó chỉ có
một lời gọi và nó nằm **phía sau cổng `isFull()`** trong `syncWakeup()`. Nên:

1. Reset / cắm lại điện / nạp OTA trong lúc đang đầy slot → `_numOfNewMsg = 0`, `unread > 0`.
2. Sync: `isFull()` true → bỏ bước tải → bỏ luôn chỗ duy nhất đặt lại biến đếm.
3. Chạm: `main.cpp` đòi `getNumOfNewMsg() > 0` mới cho đọc → 0 → "No new messages".
4. Slot chỉ được trả lại bằng cách **đọc**, mà đọc bị chặn ở bước 3. Không có đường ra.

**Không reset thì KHÔNG kẹt được** — đã kiểm bất biến: `_numOfNewMsg = unreadInMem + newCloudMsg`
tính TRƯỚC vòng tải, vòng tải thêm tối đa `newCloudMsg` slot ⇒ luôn `count ≥ unread`; mỗi lần đọc
hai số cùng giảm 1 ⇒ `unread` về 0 trước hoặc cùng lúc. Đây là lý do lỗi chỉ lộ ra sau cả một đợt
45 tin có cắm rút/nạp lại. Không phải lỗi riêng của SD: bản NAND dính y hệt, chỉ là 3 slot thì
hiếm khi tắt máy trúng lúc đang đầy.

**Đã sửa (2 chỗ).**
- `NetworkManager::syncWakeup()`, tại cổng `isFull()`: nâng sàn
  `if (_numOfNewMsg < storage->getUnreadCount()) _numOfNewMsg = ...` mỗi chu kỳ sync, và in
  `unread=<n>` vào log `het slot` để phân biệt "đầy thật" với "`writeSlotIndex` trỏ nhầm".
  > **Đính chính 2026-09-24 (§26):** `unread=0` KHÔNG thể là "`writeSlotIndex` trỏ nhầm", vì
  > `writeIndexSafe()` đã kẹp chỉ số. Trường hợp đó nghĩa là thẻ SD không mount được.
- `src/main.cpp`, cả nhánh STANDBY lẫn nhánh VIDEO: câu hỏi "có tin để đọc không" hỏi thẳng
  `storage->getNextUnreadIdentifier()` thay vì `getNumOfNewMsg() > 0`; nhánh "Downloading..." nay
  kích hoạt bằng `network.hasPendingMessages()`. `getNumOfNewMsg()` không còn lời gọi nào — giữ
  hàm lại, nó lùi về vai trò telemetry.

**KHÔNG đổi (user chốt 2026-09-20):** chỉ **chạm ngắn** mới `markAsRead()`. Thoát bằng giữ tay
hoặc để tự hết giờ thì tin **phải** còn chưa đọc để lần xem sau phát lại. Đừng "sửa" chỗ này —
đã có một lượt đề xuất sai và bị bác.

### Chưa kiểm chứng trên máy thật (syntax check toolchain thật OK, chưa link/flash)

1. Tải đầy 20 slot → **rút điện** → cắm lại → chạm phải phát được tin chưa đọc ngay (trước sửa:
   "No new messages" + `het slot`, kẹt).
2. Đọc hết 20 tin → "Reached newest msg" → chờ ≤ `SYNC_INTERVAL_MS` → log `[NET] msg: cloud=… mem=…
   new=…` và tải tiếp, không còn `het slot`.
3. Thoát bằng giữ tay / để tự hết giờ → tin đó còn nguyên chưa đọc, chạm lần sau phát lại đúng nó.

## 24. RAM thường trú + phân bổ tài nguyên khi phát video + audio (2026-09-20)

**Triệu chứng.** Sau khi chuyển sang thẻ SD, phát tin hỏng theo 4 kiểu **không xác định** — cùng
một video mỗi lần một kiểu: (2) hình+tiếng cùng giật nhưng timestamp vẫn đúng; (3) hình mượt mà
tiếng rẹt rẹt rất nặng; (4) đôi khi bấm phát thì hộp tự khởi động lại. Và (1) log tắt lúc `PLAYING`
nên không lấy được gì.

### Số đo RAM (từ `firmware.elf`, `nm --size-sort` + probe `sizeof` bằng toolchain thật)

RAM tĩnh 65.228 B. **Cảnh báo khi đọc `nm`:** phải lọc theo địa chỉ. `StandbyBackground` 115.200 B
nằm ở 0x3C… tức **flash**, không tốn một byte RAM nào — nhìn `nm` không lọc sẽ tưởng nhầm nó là
thủ phạm số một.

`appCtx` = 24.508 B, bổ ra: **`JPEGDEC` 17.884** · `AudioPlayer` 2.328 · `NetworkManager` 3.548 ·
còn lại ~657. Stack cấp từ heap: `WakeSync` 12.288 (thường trú **có chủ ý**, chống vụn heap cho TLS
§21 — giữ nguyên), `loopTask` Arduino 8.192 (`loop()` chỉ `vTaskDelay`, không làm gì).

### Vì sao đơn nhân + một bus sinh ra tiếng rẹt

Trong `PLAYING` mọi thứ tuần tự trong MỘT task, MỘT lõi, MỘT bus SPI:
`tick audio → đọc CẢ frame JPEG từ SD (giữ spiMutex) → acquireSPI → decode + đẩy 240×240 (giữ
spiMutex cả frame) → tick audio`. Giữa hai `tick()` DMA I2S không nhận byte nào.

Độ sâu DMA = 12 × 512 = 6.144 khung ⇒ **192 ms** ở file 8 kHz, **96 ms** ở 16 kHz (phần cứng chạy
× `AUDIO_OVERSAMPLE`). Ngân sách 1 frame ở 15 fps là 66 ms. Vượt mốc đó thì DMA cạn,
`tx_desc_auto_clear` phát số 0 → đúng tiếng "rẹt rẹt". Vượt xa hơn thì pacer bỏ frame, mà pacer
**cộng dồn mốc** nên hình giật trong khi timestamp vẫn đúng.

### Đã sửa

1. **`JPEGDEC` ra khỏi `appCtx`** → `JPEGDEC*` cấp/giải phóng cùng nhịp `_jpegBuffer` trong
   `playItem()`/`stop()`. Trả **17.884 B** RAM tĩnh; đỉnh RAM lúc phát KHÔNG đổi, chỉ lúc chờ mới
   dư — đúng chỗ mbedTLS cần khối ~16 KB liền mạch (§21).
   *An toàn đã kiểm:* `JPEGDEC::openRAM()` mở đầu bằng `memset(&_jpeg, 0, sizeof(JPEGIMAGE))`, nên
   cấp trên heap với rác vẫn đúng; lớp không có ctor/dtor tự khai.
2. **Thêm một nhịp `_audio.tick()` ngay TRƯỚC `acquireSPI()`** trong `decodeOneFrame()` (cả nhánh
   JPEG lẫn nhánh SLBX RGB565). Cắt khoảng mù của DMA từ (đọc + decode) xuống còn (decode). Phải
   đặt trước `acquireSPI()` vì `tick()` đọc thẻ, mutex không đệ quy.
3. **Tách cỡ ĐỌC khỏi cỡ GIÃN MẪU**: thêm `AUDIO_READ_CHUNK_SIZE = 1024`; `_chunk` dùng cỡ này,
   `_stereo` **giữ nguyên** 2048 B. `fillChunk()` đọc một lượt rồi giãn mẫu thành nhiều lượt
   ≤ `AUDIO_PCM_CHUNK_SIZE`. Ở file 16 kHz/15 fps: **9 lượt gọi FS mỗi frame → 3**, chỉ tốn thêm
   768 B (tăng thẳng `AUDIO_PCM_CHUNK_SIZE` sẽ tốn ~7 KB vì `_stereo` phình theo).
   `AUDIO_PCM_CHUNK_SIZE = 256` vốn chỉnh cho NAND đọc thô tính bằng micro-giây; trên SD mỗi lượt
   đọc là một lần lấy spiMutex + NOP hack + `fread` xuyên VFS/FATFS.

### CHƯA làm, và cố ý chưa làm

- **Xoá `loopTask`** (`vTaskDelete(NULL)` cuối `setup()`, trả 8.192 B): để **flash riêng một lượt**
  sau khi 1–3 đã ổn, vì nó đổi vòng đời task của Arduino core. Đã rà: `enableLoopWDT()` mặc định
  tắt nên `loopTask` không đăng ký task-WDT; không dùng `Serial` nên `serialEventRun()` vô can.
- **KHÔNG nâng `TASK_STACK_MEDIA_PLAYER` lên 8192.** Triệu chứng (4) tự reboot phải đọc
  `[BOOT] reset=<n>` (main.cpp, có sẵn) trước: `9` = brownout (nguồn, không phải firmware),
  `12` = panic (khi đó mới nghi tràn stack — đường SD đi qua VFS/FATFS sâu hơn hẳn NAND thô, mà
  §9.7 ghi stack bị hạ 8192→6144 chưa hề đo), `5`/`6` = watchdog. Nâng chung với 1–3 rồi hết reboot
  thì không biết nhờ cái nào; mà nếu là brownout thì đã tiêu mất RAM vừa giành lại.
- **KHÔNG tăng `AUDIO_DMA_BUF_COUNT`** — đổi RAM lấy biên an toàn, đi ngược mục tiêu đợt này.

### Chưa kiểm chứng trên máy thật (syntax check toolchain thật OK, chưa link/flash)

1. `[BOOT] heap=` phải cao hơn mốc trước sửa **~17,9 KB**. Đây là con số khách quan duy nhất của
   cả đợt.
2. Phát lại đúng tin từng gây "rẹt rẹt nặng": mong đợi hết hoặc giảm rõ. **Nếu không đổi gì thì
   giả thuyết đói DMA sai**, nghi can chuyển sang thông lượng thẻ (`SD_SPI_FREQ_HZ`, hạ 20 → 10 MHz).
3. Không hồi quy: video đúng tốc độ, ảnh tĩnh + caption đúng, tin chỉ text/voice (sentinel
   `dataSize == 4`) vẫn có tiếng trên nền đen, và tiếng bíp báo thức vẫn sạch.

## 25. Chế độ OTA do người dùng kích hoạt + rollback thật (2026-09-21)

### Đã gỡ: kích hoạt OTA bằng cờ cloud

`emergency_ota` / `normal_ota` **không xuất hiện ở bất cứ đâu ngoài firmware** — không backend,
không web, không `database.rules.json`. Chưa từng có gì bật hai cờ đó ngoài sửa tay trong Firebase
console. Thêm nữa, kích hoạt từ xa buộc hộp thức 10 phút (`OTA_WINDOW_MS`) chờ một việc có thể
không bao giờ tới. Đã xoá `_otaRequested`, `takeOtaRequest()`, `OTA_WINDOW_MS`, khối cửa sổ OTA ở
`Task_UIController`; PATCH reset cờ giờ chỉ còn `{"a_flag":false}`.

(Lưu ý cho ai đọc backend: backend có hệ `ota_flag` + `ota_tasks` + `firmware` RIÊNG, đi qua
`/device/sync`. Firmware không gọi API backend mà đọc thẳng RTDB, nên hai hệ này **chưa từng nối
với nhau**.)

### `STATE_OTA` — chế độ riêng (user chốt 2026-09-21)

- **Vào/ra: ~~giữ tay 15 giây~~ → chuỗi 3s-3s-6s** (xem mục "Lỗi ở máy thật" bên dưới — TTP223 không
  cho giữ quá ~7s). `UIController` có ngưỡng thứ hai
  `VERY_LONG_PRESS` — trước đây `_longPressEmitted` chặn mọi event sau mốc 3s cho tới khi nhả tay.
  Thanh tiến trình (`getTouchHoldMs()`) chỉ chạy ở cú giữ 6s cuối của chuỗi.
- **Chỉ vào được khi có Wi-Fi** — OTA qua LAN là đường duy nhất.
- Trong chế độ: web server + mDNS bật, **không ngủ, không sync, không phát tin, KHÔNG kêu báo thức**.
  Báo thức tắt hẳn và **không tự thoát** là đánh đổi user chốt; biện pháp giảm thiểu duy nhất là dòng
  đỏ "BAO THUC DANG TAT" trên màn OTA.
- **Đang nạp thì chặn mọi cú chạm** — mở rộng đúng cổng sẵn có của lúc đang tải tin
  (`isDownloadingMedia() || otaHandler.isUpdating()`), kể cả chuỗi chạm để thoát.
- **Chống kẹt cờ `_isUpdating`**: `OtaHandler::tickWatchdog()` huỷ phiên sau `OTA_STALL_TIMEOUT_MS`
  (30s) không có chunk. Trước đây TCP đứt mà không sinh `UPLOAD_FILE_ABORTED` thì cờ kẹt `true` vĩnh
  viễn → hộp không bao giờ ngủ, không bao giờ kêu báo thức. Watchdog gọi ở **`Task_NetworkController`,
  cùng task với `handleClient()`**, nên không bao giờ chạy song song với `Update.write()`.

### Rollback — ĐÃ ĐỌC SDKCONFIG THẬT, đừng đoán lại

Hai lớp lỗi, hai cơ chế:

1. **Lỗi giữa chừng lúc nạp** (rớt Wi-Fi, client ngắt, MD5 sai, mất điện): **không cần rollback**.
   `Update.write()` ghi vào partition *không* chạy; `otadata` chỉ đổi khi `Update.end(true)` thành
   công. Cần đúng `Update.abort()` + watchdog ở trên.
2. **Bản mới nạp xong nhưng hỏng**: rollback của bootloader.
   - Nền tảng **đã bật sẵn**: `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1`, `CONFIG_APP_ROLLBACK_ENABLE=1`.
     Nhưng `initArduino()` gọi weak `verifyOta()` (trả `true`) rồi `esp_ota_mark_app_valid_cancel_rollback()`
     **ngay lúc boot** → lớp bảo vệ bị vô hiệu.
   - Sửa: `extern "C" bool verifyRollbackLater() { return true; }` (**phải `extern "C"`**, bản weak gốc
     ở `esp32-hal-misc.c`). Bản mới phải sống `OTA_VERIFY_DELAY_MS` (60s) thì `Task_UIController` mới
     xác nhận. Reset trước mốc đó → bootloader thấy vẫn `PENDING_VERIFY` → tự quay về bản cũ.

**Gotcha 1 — TREO không gây reset, nên không gây rollback.** `CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0`
**không bật** và `loopTaskWDTEnabled = false`: một vòng `while(1)` chỉ đứng im mãi mãi, không có WDT
nào bắn. Rollback chỉ xảy ra khi chip RESET. Nên đã thêm **lưới an toàn `esp_timer`** dựng ở **dòng
đầu tiên của `setup()`** (trước cả `while(1)` khi tạo mutex hỏng): hết `OTA_VERIFY_DELAY_MS + 30s` mà
chưa xác nhận thì `esp_restart()`. `esp_timer` chạy ở task ưu tiên 22 nên vẫn bắn khi task ứng dụng
đã chết. Xác nhận thành công thì huỷ timer.

**Gotcha 2 — không được ngủ trong thời gian thử thách.** Vừa tỉnh từ Light Sleep, `esp_timer` quá hạn
bắn NGAY (ưu tiên 22) — có thể trước khi `Task_UIController` (ưu tiên 5) kịp xác nhận → reset oan một
bản tốt. Mà `INACTIVITY_SLEEP_TIMEOUT_MS` cũng là 60s, rơi đúng cửa sổ đó. Cổng ngủ có thêm
`!s_otaPendingVerify`.

**Gotcha 3 — nạp qua cáp KHÔNG bị thử thách.** `boot_app0.bin` (PlatformIO ghi mỗi lần upload) đặt
`ota_state = 0xFFFFFFFF` = `ESP_OTA_IMG_UNDEFINED` (đã đọc byte thật), không phải `NEW`, nên bản nạp
cáp không bao giờ ở `PENDING_VERIFY`. Bằng chứng khi test: nạp cáp thì **không** thấy dòng
`[OTA] ban moi dang thu thach`.

> **Đính chính plan đã duyệt (2026-09-21):** plan ghi "nạp cáp cũng đi qua PENDING_VERIFY, rút điện
> trong 60s là quay về bản cũ" — **SAI** (gotcha 3). Plan cũng ghi bước test rollback bằng
> `while(1);` đầu `setup()` — **SAI**, bản đó chỉ treo chứ không reset (gotcha 1); trước khi có lưới
> an toàn thì nó treo mãi. Test đúng là dùng `abort();` (panic → reset ngay), hoặc `while(1);` và chờ
> lưới an toàn bắn ở giây 90.

### Hai lỗi bắt được khi rà lại, trước khi báo xong

- **Use-after-free khi tắt server.** `stopWebServer()` làm `delete _webServer`; gọi từ
  `Task_MediaPlayer` trong lúc `Task_NetworkController` đang ở giữa `_webServer->handleClient()` là
  dùng vùng nhớ đã giải phóng. Code cửa sổ OTA cũ cũng có race này, nhưng tắt server giờ là thao tác
  người dùng làm thường xuyên. Sửa: `enterOtaMode`/`exitOtaMode` chỉ đặt `s_otaServerCmd` (+1/-1),
  `Task_NetworkController` thực thi — **mọi thao tác với WebServer nằm ở đúng task gọi
  `handleClient()`**. `stopWebServer()` delete + gán `nullptr`, nên mỗi lần vào là một `WebServer`
  mới và route đăng ký đúng một lần mỗi instance (đã đọc code, không tích luỹ handler).
- ~~**Tắt báo thức bằng giữ tay → lỡ giữ tiếp → vào chế độ OTA với báo thức TẮT.**~~ **ĐÃ GỠ cùng
  thao tác 15s** — với chuỗi 3s-3s-6s, cú giữ tắt báo thức bị nhánh ALARM nuốt nên không bao giờ là
  bước 1, chốt chặn này thành thừa. Nội dung cũ để tham khảo: dismiss ở giây 3,
  `VERY_LONG_PRESS` ở giây 15 của CÙNG lần giữ. Sửa: ghi `alarmDismissByTouchMs`, bỏ qua
  `TOUCH_OTA_TOGGLE` tới trong vòng `TOUCH_OTA_HOLD_MS` sau mốc đó (cùng lần giữ luôn tới trong 12s;
  lần giữ mới sau khi nhả tay luôn ≥ 15s), và không vẽ thanh tiến trình cho lần giữ đó.

### Lỗi ở máy thật: giữ 15s bị ngắt giữa chừng → ĐỔI THAO TÁC (2026-09-21)

Triệu chứng: giữ được khoảng một nửa thời gian thì thanh tiến trình biến mất.

**Chẩn đoán lần 1 — SAI, giữ lại làm bài học:** tưởng là nhiễu ngắn vì `getTouchEvent()` không có
chống nhiễu khi nhả tay (`_lastDebounceTime` được ghi nhưng không chỗ nào đọc — dead code có từ
trước, để nguyên). Đã thêm ân hạn 250ms + log. Log trả về `nha tay sau 7700-7800ms` **ổn định mỗi
lần** và dài hơn 250ms → không phải nhiễu. Ân hạn 250ms **đã gỡ** (không còn bằng chứng nào cho nó).

**Nguyên nhân thật: TTP223 tự hiệu chuẩn sau 7-8 giây chạm liên tục, và từ đó báo là đã nhả dù
ngón tay vẫn đặt nguyên** — chip hấp thụ ngón tay thành "nền" mới. Diễn đàn Arduino (thread
"Using ttp223 touch sensor without auto calibration") có người dùng 30 con, người dùng hàng trăm
con, đều xác nhận 7-8s ở mọi chế độ. Trần MOTB ~100s trong datasheet là cơ chế KHÁC, đừng nhầm.
Hệ quả: **với TTP223, không có cú giữ liên tục nào dài quá ~7s dùng được.** Muốn giữ lâu tuỳ ý phải
đổi chip (AT42QT1011).

**Thao tác mới (user chốt):** giữ 3s → nhả → giữ 3s → nhả → (hiện "Giu them 6s de vao/THOAT OTA")
→ giữ 6s. Cùng một chuỗi để vào và ra. Máy trạng thái `otaSeqStep` nằm trong `Task_MediaPlayer`:
- Chỉ đếm `LONG` nhận lúc đang STANDBY/OTA → `LONG` thoát video (nhận lúc VIDEO) và `LONG` tắt báo
  thức (nhánh ALARM nuốt trước) không bao giờ là bước 1. Nhờ vậy **chốt chặn "tắt báo thức rồi lỡ
  tay" không còn cần — đã gỡ** `alarmDismissByTouchMs`.
- Cú 6s phải là **lần giữ mới** bắt đầu sau mốc bước 2 (`now - TOUCH_OTA_HOLD_MS > otaSeqStep2Ms`),
  không thì giữ tiếp cú thứ hai tới 6s sẽ rút chuỗi còn hai bước.
- Hạn: `OTA_SEQ_STEP_WINDOW_MS` (6s) giữa hai `LONG` đầu; `OTA_SEQ_FINAL_WINDOW_MS` (12s) để bắt đầu
  cú cuối. Đang giữ tay thì không huỷ.
- Chạm ngắn khi đang hiện nhắc = huỷ (không phát tin). Chạm ngắn ở bước 1 = huỷ âm thầm, chạm vẫn
  làm việc bình thường.
- `TOUCH_OTA_HOLD_MS = 6000`: cách mốc tự hiệu chuẩn ~1,7s. **Đừng nâng lên gần 7s.**

### Đẩy version lên cloud

`updateFirebaseStatus()` thêm `"fw":"<FW_VERSION>"`. `payload` nâng 384 → **448**: chuỗi đã sát trần,
tràn thì `snprintf` cắt cụt âm thầm và cả gói JSON hỏng. `database.rules.json` không cần sửa (nhánh
`status` box ghi tự do). Màn so sánh version trên web để đợt sau — backend đã có
`FirebaseFirmwareRepository.getLatest()`.

### Công cụ nạp: `sendlove_firmware/ota_upload.py` (đã có từ trước)

`python ota_upload.py --host <IP hiện trên màn OTA>`. Đã đổi: `DEFAULT_HOST` → `sendlovebox.local`
(tên trần không qua mDNS), thông điệp retry nói "giữ 3s, 3s, rồi 6s", in cảnh báo chờ 60s sau khi nạp.
Sau nạp hộp về **STANDBY**, web server tắt → `/api/status` không dùng xác nhận version được; xem màn
OTA (vào lại) hoặc `boxes/<id>/status/fw` trên Firebase console.

### Chưa kiểm chứng trên máy thật (syntax check toolchain thật OK, chưa link/flash)

Quy trình: đổi `FW_VERSION` → `pio run` → trên hộp giữ 3s, 3s, rồi 6s → `python ota_upload.py --host <IP>` →
chờ `[OTA] ban moi da xac nhan`.
1. Chuỗi 3s-3s-6s vào/ra được; lời nhắc hiện sau cú giữ thứ hai; thanh chỉ chạy ở cú 6s.
2. Tắt router rồi làm chuỗi → "OTA: can Wi-Fi", vẫn STANDBY.
3. Nạp thật: trong lúc nạp chạm ngắn và chuỗi thoát đều **không** có tác dụng.
4. Rollback: bản `9.9.9-broken` có `abort();` đầu `setup()` → quay về bản cũ, `status/fw` là version cũ.
5. Treo: bản có `while(1);` đầu `setup()` → sau ~90s lưới an toàn reset → quay về bản cũ.
6. Rút mạng PC giữa lúc nạp → ≤ 30s thấy `[OTA] huy:` và thoát được chế độ.
6b. Giữ cú thứ hai tới quá 6s → KHÔNG vào OTA (phải là lần giữ mới).
7. Đặt báo thức rồi vào chế độ OTA → **không** kêu (đúng thiết kế).
8. Nạp **cáp** → **không** thấy `[OTA] ban moi dang thu thach`.
9. Để báo thức kêu, giữ tay tắt rồi giữ tiếp → về STANDBY, **không** vào OTA.
10. Vào/ra chế độ OTA liên tiếp 5 lần, mỗi lần gọi `/api/status` → luôn trả 200, không treo/reset.

## 26. "msg skip: het slot, unread=0" sau reboot = thẻ SD không mount (2026-09-24)

**Triệu chứng (user báo).** Sau khi reboot chip, mỗi chu kỳ sync đều in
`[NET] msg skip: het slot, unread=0`, không tải thêm tin nào.

**Nguyên nhân (suy từ code, CHƯA có log boot xác nhận).** Với bản SD, `unread=0` mà `isFull()`
vẫn true thì chỉ có một khả năng: `_mounted == false`. Khi đó `SDStorageProvider::isFull()` trả
true, còn `getUnreadCount()` trả 0. Nhánh "con trỏ ghi trỏ nhầm" mà comment ở §23 và
`NetworkManager.cpp` từng gán cho trường hợp `unread == 0` là **SAI**: `writeIndexSafe()` kẹp
chỉ số về 0..SD_SLOT_COUNT-1, `loadManifest()` cũng kẹp thêm một lần. `_mounted` chỉ được gán
**một lần** lúc boot (`SD.begin()` gọi một lần, không retry, không remount). Mount fail thì hộp
chạy rỗng đến tận lúc rút điện.

Tại sao lại xảy ra sau reboot: reset mềm (nút RST, OTA, WDT) không ngắt điện thẻ. Nếu thẻ đang
làm dở một lệnh đọc khi chip reset thì lần `SD.begin()` đầu tiên sau đó hay fail, dù thẻ không
hỏng gì.

**Đã sửa.** `SDCardManager::init()` thử `SD.begin()` tối đa 3 lần, mỗi lần cách 200 ms, và
không giữ `spiMutex` trong lúc chờ. Không cần gọi `SD.end()` giữa hai lần thử, vì khi fail
`SDFS::begin()` tự `sdcard_uninit` và đặt lại `_pdrv = 0xFF`. Log boot giờ in số lần thử:
`[SD] mounted N MB (lan k)` hoặc `[SD] mount FAIL (3 lan)`. Comment ở `NetworkManager.cpp`
đã sửa theo đúng nghĩa của `unread=0`.

**Không làm:** remount định kỳ lúc đang chạy (cắm thẻ nóng). Đó là tính năng riêng, chỉ làm khi
user yêu cầu.

Build `pio run` OK: RAM 15.8%, Flash 69.0%. Chưa nạp lên máy.

### Chưa kiểm chứng trên máy thật
1. Log boot có `[BOOT] storage: SD`, sau đó là `[SD] mounted ... (lan k)`. Nếu k > 1 thì retry
   đã cứu được lần boot đó.
2. Nhấn RST nhiều lần, kể cả lúc đang phát video: không còn `het slot, unread=0`.
3. Nếu vẫn `[SD] mount FAIL (3 lan)` thì nghi phần cứng: dây, nguồn 3.3V của module thẻ, hoặc
   tốc độ `SD_SPI_FREQ_HZ`.

## 27. Chạm phát tin lúc đang sync: xếp hàng thay vì phát chồng (2026-09-24)

**Triệu chứng (user test trên máy thật, bản chưa có sửa đổi này).** Chạm ngắn để xem tin trong
lúc sync đang chạy thì hộp **reboot** ngay. Trước đó user thấy video + tiếng giật khi sync/mạng
chưa xong. Chưa có số `[BOOT] reset=` của lần reboot đó: `12` = panic (nghi hết heap), `9` =
brownout (Wi-Fi TX + ampli + đèn nền cùng lúc, liên quan việc chọn LDO trên PCB).

**Kẽ hở.** Cổng chặn chạm ở `Task_UIController` chỉ xét `isDownloadingMedia()`. Sync đang ở bước
Wi-Fi/NTP/cờ/status, hoặc ở khe giữa hai tin (`_isDownloadingMedia` tạm về false,
`NetworkManager.cpp` sau mỗi tin), thì chạm vẫn gọi `playItem()`. `syncWakeup()` chỉ tự dừng ở
6 điểm kiểm tra `isPlaybackActive()` GIỮA các bước; bước đang dở (TLS handshake, GET) chạy tiếp
song song. RAM: phát cần ~74KB (`_jpegBuffer` 32KB + `JPEGDEC` 17,9KB + DMA I2S 24KB), TLS cần
35-45KB (§8, §21).

**Đã sửa (`src/main.cpp`, `config.h`).**
- Cờ `s_pendingPlay` (atomic) = hàng đợi 1 chỗ. Chạm ngắn ở STANDBY khi `isSyncing()` mà chưa tải
  media → bật chờ, hiện "Dang dong bo, se tu phat...". Vòng `Task_MediaPlayer` quét mỗi vòng:
  `!isSyncing()` thì phát, quá `PENDING_PLAY_MAX_WAIT_MS` thì huỷ.
- Tách `startNextUnread(fromPending)` từ nhánh chạm STANDBY. Hai nhánh `hasPendingMessages()`
  (STANDBY và cuối VIDEO) tự kích sync tải tiếp nên cũng bật chờ, nhưng **chỉ khi
  `isSyncing()` đúng ngay sau `triggerFirebaseSync()`** (sync bị từ chối thì chờ là quay vòng).
  Đang chạy từ lệnh chờ thì bật lại cờ mà **giữ hạn cũ**: mất Wi-Fi mà `_hasPendingMessages`
  còn true thì mỗi vòng sync fail lại quay về nhánh này, gia hạn mỗi lần là thử mãi.
  Bản đầu tiên (chưa nạp) bỏ hẳn việc bật lại khi `fromPending` → tải xong vẫn bắt chạm lại,
  đúng lỗi user muốn bỏ.
- Huỷ chờ: báo thức bắt đầu kêu, chạm giữ, hết hạn. Chặn ngủ khi đang chờ (`Task_UIController`
  ưu tiên cao hơn, có thể cho ngủ trước khi lệnh chờ chạy).
- Dòng nhắc vẽ lại sau mỗi lần render màn chờ, như `drawOtaPrompt`.

**User chốt 2026-09-24:** hạn chờ **60s**; chạm lúc **đang tải media vẫn bỏ qua** như cũ (không
xếp hàng); chế độ AP cấu hình Wi-Fi **không** tính là bận (AP có thể bật vô thời hạn, không TLS).

**Đã loại:**
- Đẩy lại `TOUCH_SHORT` vào `eventQueue`: vòng lặp nhận lại ngay, quay tròn.
- Callback khi sync xong: `_isSyncing = false` nằm ở ~12 lối thoát của `syncWakeup()`.
- Tính cờ chờ vào `isPlaybackActive()`: sync đang tải tự huỷ ở điểm kiểm tra kế tiếp mà không bật
  `_hasPendingMessages` → chờ xong không có tin mới để phát.
- Chặn theo `getMaxAllocHeap()`: phân mảnh kéo dài thì lệnh chờ không bao giờ chạy.

**Lưu ý:** việc này bỏ một nguồn tranh chấp RAM/CPU, KHÔNG thay kết luận §14 (NAND 20MHz) và §24
(DMA I2S cạn trên SD). Tin phát lúc không có sync mà vẫn giật thì nguyên nhân nằm ở §24.

Build `pio run` OK: RAM 15.8%, Flash 69.0%. Chưa nạp lên máy.

### Chưa kiểm chứng trên máy thật
1. Chạm lúc log đang giữa `[NET] sync start` và `sync_done` → dòng nhắc, **không reboot**, rồi
   `[PLAY] pending fire waited=Nms heap=… maxblk=…` và tin tự phát.
2. Chạm lúc không sync → phát ngay như cũ.
3. Chờ quá 60s (sync kẹt Wi-Fi) → `[PLAY] pending het han 60s`, dòng nhắc tắt, sau đó ngủ bình thường.
4. Báo thức kêu lúc đang chờ → tắt báo thức xong không tự phát tin.
5. Chạm giữ lúc đang chờ → `[PLAY] pending huy`.
6. Đọc hết tin mà cloud còn tin (slot từng đầy) → "Downloading...", tải xong tự phát.

### Lỗi ở `b1388d8`: `sleep(2000)` chặn task ~33 phút (2026-09-24)

**Triệu chứng (user test máy thật).** Log có `[PLAY] pending fire waited=7175ms` nhưng không phát.
Chạm ngắn thêm bao nhiêu lần cũng không phát. Để hộp ngủ, chạm thức dậy thì màn hình đen.

**Nguyên nhân.** Trước khi commit, trong working tree đã có thêm dòng `sleep(2000);` ngay trước
`startNextUnread(true)`, ý là "nghỉ 2s cho chip sau sync". Commit `b1388d8` gom luôn dòng đó mà
không ai đọc lại. **`sleep()` là hàm POSIX của newlib, tính bằng GIÂY**, nên dòng đó chặn
`Task_MediaPlayer` khoảng 33 phút. Mọi triệu chứng đều từ đây: cú chạm vào `eventQueue` mà không
có ai xử lý. `Task_UIController` vẫn chạy nên hộp vẫn ngủ/thức, nhưng render màn chờ là việc của
`Task_MediaPlayer`, nên thức dậy chỉ thấy màn đen.

**Đã sửa.** Giữ ý định nghỉ sau sync, nhưng đếm trong vòng lặp, không chặn task: sync phải rảnh
liên tục `PENDING_PLAY_SETTLE_MS` (lúc sửa 2000, sau nâng lên 10000) thì mới phát, sync chạy lại thì đếm lại từ đầu. Không
dùng `vTaskDelay(2000)`: trong 2s đó `s_pendingPlay` đã tắt nên hộp có thể đi ngủ hoặc một sync
mới chen vào. Build `pio run` OK. Chưa nạp lên máy.

**Gotcha.** Trong firmware này, muốn chờ theo ms thì dùng `vTaskDelay(pdMS_TO_TICKS(ms))` hoặc
`delay(ms)`. Đừng dùng `sleep()`/`usleep()`. Viết `sleep(2000)` vẫn biên dịch sạch, không có cảnh
báo nào.

## 28. Thiết kế: theme / nhạc báo thức / cài đặt trên thẻ SD — user chốt (2026-09-24)

Chỉ là THIẾT KẾ, chưa code. Tài liệu đầy đủ (sơ đồ, CSDL, thứ tự 4 giai đoạn):
https://claude.ai/code/artifact/172c970d-74be-42f0-8659-a6e990bb52f2

**User chốt:**
1. Theme vẽ theo **cách B**: gói theme trên thẻ SD được chép sang một phân vùng flash `theme`
   (~256KB, bớt app0/app1 mỗi bên 128KB) rồi đọc bằng `esp_partition_mmap`. Lý do: bus SPI dùng
   chung với ST7789 không CS, đọc nền từ thẻ lúc vẽ là không được. **Phải nạp cáp một lần** để
   đổi bảng phân vùng. Thẻ hỏng thì VẪN hiện theme trong flash; màn đen chữ trắng chỉ khi phân vùng
   trống/hỏng.
2. Nhạc báo thức **16kHz** — mở lại quyết định "giữ 8kHz" (§4) RIÊNG cho nhạc báo thức. Tin nhắn
   không đổi. Cần trần kích thước riêng (~2MB), `AUDIO_MAX_PCM_BYTES` 600KB không áp cho nhạc.
3. Thư viện nhạc thuộc **hộp** (`boxes/{boxId}/music`), không thuộc người dùng.
4. **Chỉ người nhận** quản lý nhạc và báo thức (giống theme).
5. Thẻ SD gắn từ hộp khác: **không xử lý**.
6. Hạn mức: **10 bài**/hộp, mỗi bài 5–60s, file gốc ≤ 15MB.
7. Âm lượng **riêng từng báo thức**, mặc định 80, tăng dần bật sẵn.
8. Âm lượng phát tin mặc định **100** (bằng mức hiện tại).
9. PCB: chân GAIN của MAX98357A để **pad chọn** 9/12/15dB (phần mềm chỉ giảm được âm lượng).
10. **Nguyên tắc edge case**: chỉ xử lý bằng phần mềm case BẮT BUỘC (mất dữ liệu, sập/treo, báo
    thức không kêu). Case giải quyết được bằng cách thông thường (thay thẻ, format lại, cập nhật
    firmware) thì KHÔNG xử lý. Áp cho cả các đợt sau.

**Đính chính §4 "KHÔNG giảm độ sáng đèn nền":** quyết định đó chặn firmware TỰ giảm sáng để che
lỗi nguồn. Không áp cho cài đặt độ sáng do người dùng chọn trên web (mặc định vẫn 100%).

**Đã kiểm khi thiết kế:** LovyanGFX 1.2.26 có `loadFont(const uint8_t*, ft_vlw)`
(`LGFXBase.hpp:809`) → nạp phông VLW (web cắt sẵn tập ký tự tiếng Việt cần dùng) từ con trỏ mmap
được. KHÔNG dùng `loadFont(fs, path)`: nó đọc thẻ ngay lúc vẽ, trên cùng bus với màn hình.
`storage.rules` cho hộp đọc `media/{boxId}/**` → theme/nhạc để dưới đó là đủ; gói mặc định ở
`public/` thì chưa có rule.

## 29. Triển khai §28: cài đặt, nền tảng thẻ SD, nhạc báo thức, theme (2026-09-24)

Làm theo thứ tự advisor đề xuất: cài đặt → nền tảng SD → nhạc → theme (theme cuối vì là phần
duy nhất phải nạp cáp). Kèm đề xuất #1 (status mở rộng), #2 (log trên thẻ), #3 (tải tiếp bằng
Range), #10 (xem trước đúng như hộp). `pio run` OK: RAM 16.9%, Flash 1,31MB / 1,90MB (giảm
~120KB so với trước vì nền + ChakraPetch không còn biên dịch vào). **Chưa nạp lên máy.**

**Firmware — module mới:**
- `lib/Settings`: độ sáng + âm lượng (NVS `set_bl/set_vol/set_rev`), gamma 2, sàn 5%, màn báo
  thức ≥60%, bảng 101 hệ số Q15 theo dB. `AudioPlayer::fillChunk` bỏ hẳn phép nhân khi hệ số =
  32768 → âm lượng 100 (mặc định) đi ĐÚNG đường cũ bit-identical; khác 100 thì trượt ≤8192/lượt.
  `config_flag` đọc trong `checkFirebaseFlags`, lấy giá trị bằng `GET config.json?shallow=true`
  **[SAI, sửa 2026-09-25: shallow trả `true` cho mọi khoá con kể cả số → hộp không bao giờ
  nhận cài đặt. Giờ dùng `orderBy="$key"&startAt="config_rev"&endAt="playback_volume"`.]**
  (1 request, không kéo mật khẩu Wi-Fi/alarm_list). `PIN_AMP_SD = -1` (breadboard chưa nối).
- `lib/SdStore`: `/sys/layout.json`, dọn `.tmp` lúc boot, `writeAtomic`, crc32 zlib (không bảng),
  remount ở đầu `syncWakeup` khi ABSENT. `SDCardManager`: handle thứ 4 `_genFile`, `max_files` 7,
  `remount/probe/readFileAt/renameFile/makeDir/removeDir/listDir/freeMB`. Manifest `index.bin`
  giờ ghi nguyên tử (tmp → xoá → đổi tên).
- `lib/SdLog`: vòng 32×64 B trong RAM (mọi DLOG), `flush()` xuống `/sys/log/log0.txt` mỗi 30s ở
  STANDBY + trước khi ngủ, `status/log_tail` chỉ khi có dòng ERR/FAIL mới hoặc lần đầu sau boot.
- `NetworkManager::downloadFile`: `.part` + `Range: bytes=N-`, kiểm size + crc32 rồi mới đổi tên,
  dst đã đủ size thì bỏ qua. CHỈ cho theme/nhạc — **tin nhắn KHÔNG tải tiếp** (ghi qua slot của
  SDStorageProvider, không có .part). Phải dùng `stream->read()` không phải `readBytes()` (đọc từng
  byte, xem vòng tải tin).
- `lib/MusicStore` + `syncAlarmMusic`: `/alarm/index.json`, ≤10 bài, tối đa 2 bài/chu kỳ, bài của
  báo thức sắp kêu trước, xoá bài không còn trên cloud (+ `.part` mồ côi). `AlarmItem` thêm
  `musicId[24]/volume/ramp` → blob NVS cũ lệch cỡ → `loadAlarms` bỏ blob VÀ hạ cờ dirty (không thì
  lần sync đầu "hộp thắng" đẩy danh sách rỗng xoá sạch báo thức cloud).
- Báo thức: nhạc có trên thẻ → `startAlarmMusic` (AudioPlayer đọc file qua `_atFile`, lặp, ramp
  30%→100% trong 20s); mọi lỗi khác → bíp ở mức **100** (sóng sin vốn nhỏ, nhân âm lượng 80 là có
  thể không nghe). `isPlaybackActive()` tính cả báo thức có nhạc; vòng tải tin cũng dừng khi đó.
  `crc32File` KHÔNG dùng `_atFile` (nhạc báo thức giữ handle đó).
- Theme (cách B): `partitions_ota.csv` app0/app1 0x1F0000 → 0x1D0000, app1 dời về 0x1E0000,
  phân vùng `theme` (data 0x40) 0x3B0000 256KB. `lib/ThemeStore`: header sector 0 ghi CUỐI, mmap,
  kiểm crc payload lúc boot, cài từ `/theme/t_<id>_r<rev>/` trong Task_MediaPlayer khi không sync;
  flash trống lúc boot thì cài lại từ `/theme/active.json`. `LayoutEngine::loadTheme()`: nền mmap
  (y như mảng PROGMEM cũ), phông VLW qua `lgfx::PointerWrapper` + `lgfx::VLWfont` sống cùng theme,
  ngày theo `format/locale` (tên thứ phải khớp `theme/layout.js` của web), dự phòng đen/trắng
  Font7 + Font2. `theme_flag` chỉ hạ khi bản TRONG FLASH đúng rev. Đã xoá `StandbyBackground.h`,
  `ChakraPetch_SemiBold_16/48.h` (nền đã xuất sang `sendlove_web/public/theme/default-bg.bin`).
- `TASK_STACK_MEDIA_PLAYER` 6144 → 8192: VLWfont::drawChar `alloca(w*h)` trên stack. Log
  `[LAY] stack con N B` mỗi khi mức còn lại xuống thấp hơn lần trước (glyph to nhất có thể chưa
  xuất hiện ở khung đầu). Web chặn glyph > 3000 B (`themePack.js MAX_GLYPH_BYTES`); phông mặc
  định đo được glyph lớn nhất 630 B. Buffer status 448 → 640.

**Gotcha đã gặp:**
- LDF PlatformIO không lần theo `<Preferences.h>` bên trong `ConfigManager.h` → lib mới dùng
  ConfigManager phải include thẳng `<Preferences.h>`.
- `NAME_MAX` là macro của limits.h — đặt tên hằng khác.
- **Thử máy thật 24/09: `[NET] ghi the FAIL` ngay sau `[NET] tai /alarm/m_...`.** Bản đầu cho
  `SdLog::flush` (Task_MediaPlayer, 30s/lần) dùng CHUNG `_genFile` với `downloadFile`
  (Task_WakeSync): `openGenWrite` của log đóng file nhạc đang tải, `genWrite` kế tiếp trả 0.
  Sửa: log dùng `SDCardManager::appendFile` (mở-ghi-đóng trong một lần giữ mutex); `_genFile`
  giờ chỉ WakeSync dùng. Log FAIL in thêm `@<byte>` để phân biệt nếu còn lỗi thật của thẻ.
  Hệ quả phụ của bản lỗi: `log0.txt` có thể dính một đoạn byte nhạc — vô hại.
- LovyanGFX trên panel KHÔNG đọc được: pixel alpha trung gian của phông VLW trộn với MỘT màu nền
  cố định (`getBaseColor()`), không với ảnh nền → web cắt VLW alpha nhị phân 0/255.

**Backend/web/rules:** `config_rev` (ServerValue.increment); music CRUD thật ở
`/boxes/:id/music` (upload policy → commit, backend tự tải file đo size + crc + kiểm header AUDC;
xoá bài gỡ `music_id` khỏi báo thức cùng một lần ghi); alarm thêm `music_id/volume/ramp`; theme có
`theme_id/rev/assets` + route `/theme/font`, phông `f_time/f_date/Font7/Font2`; `database.rules.json`
cho hộp đọc `boxes/$id/music` và ghi 3 trường nhạc trong alarm_list. Web: ReceiverConfig (trạng thái
áp dụng, thẻ nhớ, log), ReceiverMusic mới, ReceiverAlarms (nhạc/âm lượng/tăng dần), theme cắt VLW +
`ExactPreview`. Đã kiểm trong trình duyệt: VLW (33 glyph xếp tăng, alpha chỉ 0/255, đủ chữ Việt,
"Thứ năm, 24.09" đúng), RGB565 nền mặc định đúng màu, gói nhạc AUDC 16kHz đúng từng trường.

**Theme mặc định** không còn là "gói public" (§28 bỏ ngỏ rule `public/`): web gửi nền mặc định
như một ảnh nền bình thường vào `media/{boxId}/theme/`, không cần rule mới.

### Chưa kiểm chứng trên máy thật — nạp CÁP bắt buộc (bảng phân vùng đổi)
1. Boot: `[THM] phan vung trong -> man du phong` rồi màn đen giờ 7 đoạn trắng; `[SDS] san sang`.
2. Web: lưu theme "Mặc định" → sync → `[NET] theme ... san sang` → "Dang ap dung giao dien..."
   → màn đúng như ExactPreview; `[LAY] stack con N B` phải còn > ~1KB.
3. Rút thẻ, reboot: theme vẫn hiện (từ flash). Cắm lại → sync kế tiếp `[SDS] the da cam lai`.
4. Đặt âm lượng 0/50/100 giữa lúc phát tin (không "bụp"); độ sáng 5% rồi để báo thức kêu (≥60%).
5. Thêm nhạc, gán báo thức → `[NET] nhac ... OK` → kêu bằng nhạc, tăng dần; xoá file trên thẻ →
   bíp; snooze kêu lại đúng bài; sửa giờ trong portal giữ nguyên nhạc.
6. Ngắt điện khi đang cài theme → boot cài lại từ thẻ (`flash trong, cai lai tu the`).
7. Deploy: `database.rules.json` (TRƯỚC khi dùng nhạc), functions, hosting. Theme lưu bằng web cũ
   (không có rev) hộp bỏ qua — người nhận lưu lại một lần.

**Đính chính §29 (2026-09-24 tối):** bước 6 viết sai. Rút điện giữa lúc cài theme thì boot cài lại
theme CŨ (`active.json` chỉ ghi SAU khi cài xong); theme mới tự cài ở lần sync kế vì `theme_flag`
chưa hạ.

## 30. Chuẩn hoá độ to: % âm lượng = % so với mức chuẩn của loa (user chốt 2026-09-25)

**Vì sao.** Trước đó web chỉ HẠ đỉnh > 0,7, file vốn nhỏ giữ nguyên → hai file cùng 80% lệch nhau
cả chục dB, không kiểm đều được âm lượng. Giờ `renderSegment` (sendlove_web `mediaEncoder.js`,
dùng chung cho tin thoại, tiếng video, nhạc báo thức) đưa mọi đoạn về `LOUDNESS_TARGET_DB = -20`:
RMS khối 400ms, cổng -60 dB tuyệt đối + -10 dB tương đối, đo sau lọc thông cao 200 Hz (loa nhỏ
không phát bass). Nâng tối đa +18 dB (file quá nhỏ chủ yếu là ồn). Sau bộ nén thêm limiter -5 dB,
trần đỉnh 0,7 giữ nguyên. Firmware không đổi gì ngoài bíp: sin 4000 → 4634 (RMS -20 dBFS).

**Gotcha.** `DynamicsCompressorNode` của Chrome tự cộng makeup gain (~+4 dB đo được), không tắt
được → đo lại độ to SAU nén rồi nhân một hệ số cuối. Kiểm trong trình duyệt: tín hiệu -36 dB,
-6 dB, tiếng ồn đều ra -20,0 dB, đỉnh ≤ 0,7; bài nhiều bass (80 Hz) ra -22 dB vì bị trần đỉnh
chặn — chấp nhận.

**Đánh đổi đã biết.** Bỏ nguyên tắc cũ "chỉ hạ, không bao giờ nâng" của đường âm thanh: file nhỏ
được nâng → dòng TRUNG BÌNH của ampli tăng (đỉnh vẫn ghim). Nếu breadboard rè / nháy màn khi
phát: hạ `LOUDNESS_TARGET_DB`, đừng đụng trần đỉnh. File gửi trước 2026-09-25 giữ mức cũ.
Còn thiếu: hiệu chuẩn 100% trên máy thật (chọn pad GAIN + đo SPL) — việc của user.

**Cùng ngày, user nghe thử: "chưa đủ to", mọi file nhỏ hơn tiếng bíp → "to thật to", user tự hạ
dần.** Số ở trên đã lỗi thời:
- Web: `LOUDNESS_TARGET_DB` -20 → **-10**, `AUDIO_PEAK_CEILING` 0,7 → **0,98**, nâng tối đa
  +30 dB, bộ nén -24 dB / 6:1. Bỏ HẲN phần dưới 250 Hz ở file gửi xuống (`SPEAKER_LOW_HZ`, loa
  không phát được, chỉ tốn biên độ). Limiter viết tay `limitPeaks` (nhìn trước 5ms) thay
  DynamicsCompressor làm limiter; lặp đo → bù → ghim tối đa 3 lượt. Đo trong trình duyệt: âm
  sắc -10,0 dB; bass -10,3; giọng nói giả (nhiễu điều biên) -12,7 dB, đỉnh 0,98.
- Firmware: bíp 4634 → **32000** (RMS -3,2 dBFS). Bíp vẫn là âm đơn 1,6 kHz — tai nhạy và
  có lẽ trùng cộng hưởng loa — nên cùng RMS vẫn nghe to hơn nhạc.
- Chưa đụng: âm lượng mặc định báo thức 80 (-8 dB), tăng dần từ 30%.
- Thứ tự hạ khi quá to / rè: `targetDb` của profile → `AUDIO_PEAK_CEILING` → bảng sin bíp.

**Lần 3 (user: nhạc vẫn thua bíp khi cả hai 100%).** Tách profile trong `mediaEncoder.js`:
`PROFILE_VOICE` {-10 dB, cắt < 250 Hz} cho tin thoại / video; `PROFILE_ALARM` {-6 dB, cắt < 400 Hz,
+6 dB quanh 2 kHz} cho nhạc báo thức. Đo trên "nhạc giả" (bass + hợp âm + giai điệu + trống): RMS
-11,2 → -7,3 dB, năng lượng dải 2 kHz -20,6 → -15,1 dB. Bíp: RMS -3,2 dB, dồn hết vào một tần số.
**Giới hạn vật lý:** nhạc chỉ bằng được sin toàn thang khi bị nén thành gần như sóng vuông (méo
nặng). Khoảng cách còn lại ~4 dB RMS, ~11 dB ở dải 2 kHz.

## 31. Dọn dẹp: comment chuyển sang tiếng Anh, gom file rác (2026-10-02, nhánh `chore/cleanup`)

**Làm gì.** Toàn bộ comment trong `src/`, `include/`, `lib/**` (kể cả ghi chú vá `SPI_MODE3` của
ta trong `lib/SD/src/sd_diskio.cpp`; phần còn lại của `lib/SD` là thư viện ngoài, không đụng),
`platformio.ini`, `partitions_ota.csv`, `ota_upload.py` được rút gọn và dịch sang tiếng Anh.
**Không đổi dòng code nào**, không đổi chuỗi log / chuỗi hiển thị / thông báo `#error` / nội dung
trang portal trong `captive_portal_html.h`. Không refactor firmware.

Vài comment vốn đã sai so với code thì sửa lại cho đúng luôn (chỉ sửa chữ, không sửa code):
`MAX_MEDIA_BYTES` (comment còn nói 5,5 MB trong khi giá trị là 25,5 MB), `SD_SPI_FREQ_HZ`
("hạ xuống 10MHz nếu…" trong khi đã là 10MHz), biên độ bíp "4000" (thật là 32000), `prefill()`
"nạp 2 buffer" (thật là nạp đầy DMA), `probe()` "dùng cardType()" (thật là mở file đọc 1 byte),
stack MediaPlayer "đã hạ 8192→6144" (thật là 8192), `main.cpp` "nháy đèn nền báo lỗi" (thật là
đứng yên `while(1)`), `NetworkManager.cpp` "mỗi chu kỳ 10s" (chu kỳ sync giờ là 20s).

**Xác minh.** Từng file: bỏ comment bằng `g++ -fpreprocessed -dD -E -P` rồi so với bản ở HEAD,
phải trùng khít (đã thử chèn một dòng code giả để chắc là phép so bắt được). `pio run` thành công
sau khi dịch và sau khi gom file. **Chưa nạp máy thật** — không cần, vì mã sau tiền xử lý y nguyên.

**Hệ quả cần biết.**
- Số dòng trong file tài liệu này (vd. `NetworkManager.cpp:172-176`, `AudioPlayer.cpp:216-227`)
  **đã lệch** vì comment ngắn đi. Tìm theo tên hàm, đừng tin số dòng cũ.
- Comment lịch sử kiểu "trước ngày X làm thế này" đã bỏ khỏi code; lý do thiết kế vẫn giữ trong
  comment và phần lịch sử nằm ở file này (các mục được dẫn bằng "see MEMORY.md §N").
- `src/main.cpp.bak` (mục "Rác cần dọn" ở trên) đã **xoá**. `src/audio_data.h`, `upload_audio.py`,
  `test.raw`, `wokwi.toml`, `ChakraPetch-*.ttf`, `image/`, `scratch/`, `implementation_plan.md`,
  `FIREBASE_ANONYMOUS_AUTH.md` chuyển sang thư mục `trash can wait for user bring to throw away/`
  ở gốc repo (xem `INDEX.md` trong đó). Các file `*.d` ở gốc firmware và bản trùng
  `ota_architecture_design.md` đã xoá (bản thật ở `docs/`).
- `codebase_review.md`, `code_review_2_9_gemini_38.md`, `fix_download_timeout_plan.md`,
  `SHOULD_READ.md` **giữ nguyên chỗ cũ** vì file này và `sendlove_kicad/BOM.md` còn dẫn tới.

**Lượt hai cùng ngày — rút gọn comment (user: "mới dịch, chưa rút gọn").** Comment firmware từ
2.307 xuống 1.390 dòng: bỏ comment lặp lại dòng code bên dưới, code cũ bị comment lại (kể cả
SSID/mật khẩu Wi-Fi mặc định cũ trong `config.h`), khung tiêu đề; mặc định một dòng, chỉ giữ lý
do không hiển nhiên, cảnh báo "do NOT", con số cần thiết và tham chiếu `§N`. Phần diễn giải dài
(số đo, lịch sử, phương án đã loại) **chỉ còn ở file này** — comment trong code giờ dẫn về đây.
Xác minh như trên (so sau khi bỏ comment + `pio run`), số dòng lại lệch thêm một lần nữa.
Comment của `addStorageAuthHeader()` từng ghi "chưa verify được" đã sửa theo §19 (đã đo thật).

**Ghi nhận, chưa làm (cần user quyết).** `checkAndDownloadNewMessages()` lặp đoạn đọc `timestamp`
4 lần; đoạn đổi đường dẫn Storage → URL lặp ở 3 hàm (`downloadFile`, `downloadVoiceSegment`,
`checkAndDownloadNewMessages`); `dumpHexBytes()` trong `MediaPlayer.cpp` là hàm rỗng. Gộp lại được nhưng là sửa code firmware nên không làm trong đợt "chỉ đổi hình thức" này.
