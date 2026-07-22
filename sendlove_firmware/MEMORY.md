# SENDLOVE BOX - PROJECT MEMORY & CONTEXT

*Tài liệu này lưu trữ toàn bộ bối cảnh dự án, kiến trúc phần cứng, phần mềm và lịch sử các task để phục hồi context cho Agent trong các session tương lai.*

## 1. MÔ TẢ DỰ ÁN
**Sendlove Box** là một hộp quà tặng thông minh/video player dựa trên ESP32-C3.  
- **Core Functionality**: Trình chiếu video có âm thanh, hiển thị hình ảnh, điều khiển chạm, hiển thị đồng hồ và kết nối qua mạng (Firebase/WiFi).
- **MCU**: ESP32-C3-DevKitM-1 (160MHz, 320KB RAM, 4MB Flash).
- **USB CDC**: Kích hoạt trên GPIO 18/19 (Nạp code và in Serial).

## 2. KIẾN TRÚC PHẦN CỨNG (PHASE 1)
- **Màn hình**: TFT LCD 1.54"/1.3" ST7789 240x240 IPS. (Không có chân CS).
- **Lưu trữ Media**: W25Q128 (16MB NAND Flash SPI).
- **Điều khiển**: Touch Sensor TTP223.
- **Sơ đồ chân (Pinout)**:
  - **Shared Hardware SPI2 Bus**: SCK (GPIO 4), MOSI (GPIO 6).
  - **TFT ST7789**: DC (7), RST (9), BL (3 - PWM). *CS = -1 (Không có)*.
  - **NAND W25Q128**: CS (8), MISO (5).
  - **Touch TTP223**: GPIO 10 (Active HIGH, INPUT_PULLDOWN).

> **Hardware Challenge (Đã giải quyết)**: 
> Màn hình ST7789 không có chân CS nên luôn nhận xung nhịp SPI. Việc chia sẻ chung bus SPI2 với NAND Flash gây hiện tượng nhiễu hình ảnh khi đọc dữ liệu NAND (Tùy chọn B - Cắm dây dùng chung SPI).
> **Giải pháp đang áp dụng**:
> - Đồng bộ cùng `SPI_MODE3` cho cả LGFX và NAND để không lệch bit do SCK Idle nhảy.
> - Thêm lệnh Hack NOP (`0x00`) với TFT_DC=0 rồi kéo TFT_DC=1 trước mỗi phiên lấy SPI Mutex của NAND, giúp màn hình ST7789 bỏ qua dữ liệu giao tiếp với Flash.

## 3. KIẾN TRÚC PHẦN MỀM (FIRMWARE)
- **Platform**: PlatformIO (Arduino Framework).
- **Libraries**:
  - `LovyanGFX` (Display driver chính, hỗ trợ bus_shared = true).
  - `JPEGDEC` (Giải mã VJPG / JPEG trực tiếp từ buffer).
- **Core Logic (FreeRTOS)**:
  - `Task_MediaPlayer` (Priority 3): Giải mã hình ảnh từ NAND và render liên tục qua `pushImage()`.
  - `Task_UIController` (Priority 1): Polling touch sensor (10ms/lần) và xử lý debounce, đẩy event.
  - Hàng đợi `eventQueue` để gửi thông báo (VD: `TOUCH_NEXT_SLOT`) giữa các tasks.

## 4. QUẢN LÝ DỮ LIỆU NAND (SLOT TABLE)
- NAND giữ nguyên format từ *test project* (không bị Format).
- **Cấu trúc**: 5 Slot lưu trữ (Video VJPG / Image VIMG).
- Header nằm ở Sector 0 (Địa chỉ `0x000000`). Bắt đầu bằng Magic string `"NSLT"`.
- Module `NandStorage` chỉ thực hiện Read-Only để bảo tồn dữ liệu gốc đã được flash.

---

## 5. LỊCH SỬ CÔNG VIỆC (TASK LOG)

### Phase 1: Màn hình + NAND Read + Touch (Current)
- [x] Quyết định từ bỏ Hardware UART để ưu tiên USB CDC (D+/D-).
- [x] Quy hoạch lại GPIO sử dụng Shared HW SPI2 cho TFT và NAND.
- [x] Lập trình `config.h`, `platformio.ini`.
- [x] Lập trình `DisplayDriver` bọc LovyanGFX.
- [x] Lập trình `NandStorage` API (Hardware SPI) cho việc đọc Slot Table và frames.
- [x] Lập trình `MediaPlayer` với `JPEGDEC` giải mã luồng từ `NandStorage`.
- [x] Cấu hình lại `main.cpp` chạy event-driven đa luồng (FreeRTOS).
- [x] Fix lỗi xung đột SPI giữa ST7789 và W25Q128 (Đồng bộ SPI_MODE3, NOP Hack, Fix USB CDC boot delay).

### Sẽ thực hiện (Future / Phase 2)
- [ ] Test chạy thành công hình ảnh VJPG từ NAND Flash ra ST7789 trên board thực.
- [ ] Bổ sung I2S Audio Module (MAX98357A) cho âm thanh video.
- [ ] Quản lý trạng thái Pin / Báo sạc (Battery ADC).
- [ ] Tính năng LED Breathing (PWM).
- [ ] Kết nối WiFi tĩnh và sau này chuyển sang Provisioning / Firebase.
- [ ] OTA Update qua WiFi.
