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
- [x] Chuẩn hóa tên thứ trong tuần sang ASCII không dấu (`CHU NHAT`, `THU HAI`, ...) tương thích với bộ font mặc định LovyanGFX.
- [x] Tích hợp font Google **Chakra Petch** (size 36pt GFXfont) làm font mặc định hiển thị đồng hồ giờ Standby UI.
- [x] Fix lỗi biên dịch C++ Clang/GCC `param_default_argument_redefinition`: Loại bỏ giá trị tham số mặc định trùng lặp trong `.cpp`, chỉ giữ khai báo ở header `.h` (`turnOn(esp_sleep_wakeup_cause_t cause = ESP_SLEEP_WAKEUP_UNDEFINED)`).
- [x] Chuẩn hóa kiểu dữ liệu nguyên nhân thức dậy `esp_sleep_wakeup_cause_t` (từ `<esp_sleep.h>`) đồng bộ cho `DisplayDriver` và `PowerManager`.
- [x] Thêm `NetworkManager::ensureConnected()` chủ động gọi `WiFi.reconnect()` kích hoạt bộ thu phát RF ngay khi vừa tỉnh giấc từ Light Sleep.
- [x] Fix lỗi underflow tràn số nguyên không dấu `now - lastUserActivity` bằng biến điều khiển `activeSleepTimeoutMs` (30s khi Timer Wakeup, 15s khi Touch Wakeup).
- [x] Fix lỗi màn hình tối om sau khi OTA Reboot trong lúc màn hình đang tắt: Thêm `gpio_hold_dis((gpio_num_t)PIN_TFT_BLK)` vào đầu `DisplayDriver::init()` để giải phóng cờ khóa RTC hold phần cứng trên GPIO 3.
- [x] Nâng cấp script `ota_upload.py`: Tự động Polling `/api/ota/begin` trong 130 giây (>2 phút) kiên nhẫn chờ ESP32 thức dậy từ chu kỳ ngủ ngầm.

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

### B. Cơ chế đếm nối thời gian thực (RTC Time Sync trong Light Sleep)
- Khi gọi `configTzTime()`, ESP-IDF lấy mốc Unix Epoch từ NTP Server và liên kết với bộ đếm phần cứng **RTC Timer (RTC Slow Clock)**.
- Khi vào Light Sleep, CPU dừng nhưng RTC Timer vẫn đếm liên tục.
- Ngay khi tỉnh dậy, trình quản lý nguồn (`esp_clk`) của ESP-IDF tự động tính $\Delta T = RTC_{\text{wakeup}} - RTC_{\text{sleep\_start}}$ và cộng bù trực tiếp vào đồng hồ hệ thống C POSIX (`timeofday`).
- Nhờ đó, gọi `getLocalTime()` sau khi ngủ dậy luôn trả về đúng giờ/phút/giây thực tế mà không hề bị lệch hay mất giây nào.

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
- **Giải pháp**: Giữ `lastUserActivity = millis()` chuẩn thời gian thực và thay đổi giá trị điều kiện so sánh `activeSleepTimeoutMs` (15s cho Touch Wakeup, 30s cho Timer Wakeup).

---

### Sẽ thực hiện tiếp (Phase 2.5 & Phase 3)
- [ ] Tháo/xả bỏ đèn LED đỏ báo nguồn phần cứng trên bo ESP32 DevKit và Module NAND Flash (giảm dòng rò từ 5mA xuống 1mA khi ngủ).
- [ ] Test thực tế playback video VJPG từ NAND kết hợp chuyển đổi Standby UI trên phần cứng.
- [ ] Bổ sung I2S Audio Module (MAX98357A) cho âm thanh video.
- [ ] Tối ưu hóa chu kỳ Deep Sleep ngầm kết hợp kiểm tra Firebase.
- [ ] Nâng cấp OTA: firmware từ Firebase Storage, trigger từ web client.





