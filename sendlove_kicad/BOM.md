# SendLove Box — danh sách linh kiện (rút từ firmware v2.1.0)

Nguồn: `sendlove_firmware/include/config.h`, `platformio.ini`, `lib/*`. Schematic `main/main.kicad_sch` hiện trống.

Giả định:
- Dùng **module** ESP32-C3, không vẽ SoC rời + thạch anh + anten.
- Bộ nhớ media là **thẻ microSD** (`ACTIVE_STORAGE_TYPE = STORAGE_TYPE_SD`).
- **Không đo pin** ở bản này. GPIO3 giữ nguyên cho đèn nền, không phải đổi chân.

## 1. Sơ đồ chân

| GPIO | Chức năng | Nguồn | Ghi chú mạch |
|---|---|---|---|
| 0 | I2S BCLK → MAX98357A | config.h:163 | |
| 1 | I2S LRC → MAX98357A | config.h:164 | |
| 2 | I2S DOUT → MAX98357A DIN | config.h:165 | **Strapping**: phải HIGH lúc boot → kéo lên 10k |
| 3 | TFT BLK (PWM 44.1kHz, 9-bit) | config.h:15, DisplayDriver.cpp:9 | Giữ LOW khi ngủ bằng `gpio_hold_en` |
| 4 | SPI SCK (chung TFT + SD) | config.h:9 | SPI MODE3 bắt buộc |
| 5 | SPI MISO (chỉ SD dùng) | config.h:8 | Kéo lên 10k (DAT0 thẻ SD) |
| 6 | SPI MOSI | config.h:7 | TFT 40MHz, SD 10MHz |
| 7 | TFT DC | config.h:13 | |
| 8 | SD CS | config.h:22, 202 | **Strapping** → kéo lên 10k (cũng là pull-up CS) |
| 9 | TFT RST | config.h:14 | **Strapping/BOOT**: LOW lúc boot = vào chế độ nạp → kéo lên 10k. Nút BOOT nối qua R 1k (xem mục Vi điều khiển) |
| 10 | Touch TTP223 OUT (active HIGH, đánh thức light sleep) | config.h:26, PowerManager.cpp:98 | Firmware bật pull-down nội |
| 18/19 | USB D-/D+ | | Nạp code + Serial (USB CDC) |
| 20 | **Bật/tắt ampli** → MAX98357A SD_MODE | *mới, chưa có trong firmware* | HIGH = phát, LOW = shutdown |
| 21 | **LED thông báo** | *mới*, config.h:27 (`PIN_LED` dự trữ) | Active HIGH. Là U0TXD nên LED chớp nhẹ lúc ROM in log khởi động, chấp nhận được |

Không còn GPIO trống.

## 2. BOM

### Vi điều khiển
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | Module ESP32-C3, flash 4MB | ESP32-C3-MINI-1-N4 | `partitions_ota.csv` cần 4MB |
| 1 | Tụ 10µF | 0603 | Sát chân 3V3 module |
| 1 | Tụ 100nF | 0402 | Sát chân 3V3 module |
| 1 | R 10k + C 1µF | | Mạch RC cho chân EN |
| 2 | Nút nhấn SMD (hoặc 2 cặp pad hàn) | | RESET: EN→GND. BOOT: GPIO9→R 1k→GND |
| 1 | R 1k nối tiếp nút BOOT | | GPIO9 đang xuất HIGH cho TFT RST; nhấn BOOT không có R này là chập chân ra xuống GND |
| 3 | R 10k pull-up | | GPIO2, GPIO8, GPIO9 |

Nạp code qua USB GPIO18/19 hầu như không cần nút BOOT; nút chỉ để cứu chip.

### Màn hình
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | LCD IPS 240×240 ST7789, **không có chân CS** | 1.3" hoặc 1.54" | `invert=true`, `rgb_order=true`, offset 0 (DisplayDriver.h:44-52) |
| 1 | Header 1×7 2.54mm hoặc đầu FPC theo panel | | Module: GND VCC SCL SDA RES DC BLK |
| 1 | N-MOSFET đèn nền | AO3400 / 2N7002 | Chỉ cần nếu dùng panel trần (dòng LED vượt khả năng GPIO) |
| 1 | R 100Ω gate + R 100k gate→GND | | Kéo xuống để đèn tắt lúc boot |

### Bộ nhớ media
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | Khe microSD push-push | Hirose DM3AT / Molex 503398 | SPI 10MHz, 20 slot tin nhắn |
| 4 | R 10k pull-up | | CMD(MOSI), DAT0(MISO), DAT1, DAT2 |
| 1 | Tụ 10µF + 100nF | | Cấp nguồn thẻ |

Không cắt nguồn thẻ SD khi ngủ: SCK/MOSI dùng chung với màn hình và nghỉ ở HIGH (MODE3), thẻ sẽ bị cấp điện ngược qua diode bảo vệ. Thẻ chờ (CS HIGH) ăn vài chục–vài trăm µA tuỳ loại, nên đo thực tế khi chọn thẻ.

Phương án thay thế (không lắp cùng lúc, chung CS GPIO8): W25Q128JVSIQ SOIC-8 + 100nF — chỉ 3 slot.

### Âm thanh
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | Ampli I2S class-D | MAX98357AETE+ (TQFN-16) | VDD nối thẳng VBAT/VSYS (2.5–5.5V), không qua LDO 3.3V |
| 1 | R 680k: GPIO20 → SD_MODE | | Cùng R kéo xuống 100k bên trong ampli: HIGH → ~0.42V = chế độ (L+R)/2 (dải 0.16–0.77V); LOW → shutdown <1µA. Tiết kiệm ~2mA khi không phát |
| 1 | R_GAIN: footprint 0603 từ chân GAIN xuống GND | Không lắp / 0Ω / 100k | Chọn độ lợi bằng linh kiện lắp vào: **không lắp = 9dB**, **0Ω = 12dB**, **100k = 15dB**. Chốt 2026-09-24 (xem ghi chú dưới bảng) |
| 1 | Tụ 10µF | | Sát VDD ampli |
| 1 | Tụ 100nF | | Sát VDD ampli |
| 1 | Loa 8Ω 1W, 28–36mm | | Cần buồng kín phía sau, không thì mất bass (SHOULD_READ.md:58) |
| 1 | Đầu JST-PH 2 pin | | Nối loa |

Firmware xuất stereo `RIGHT_LEFT` với hai kênh giống nhau, nên chế độ kênh nào của ampli cũng phát đúng.

**Vì sao để pad chọn GAIN (chốt 2026-09-24):** firmware sắp có cài đặt âm lượng và nhạc báo thức, nhưng âm lượng phần mềm chỉ **giảm** được (nhân mẫu PCM với hệ số ≤ 1). Mức 100% trên web chính là mức của chân GAIN. Nếu 9dB không đủ to cho báo thức thì chỉ việc lắp 0Ω hoặc 100k, không phải sửa mạch. Chọn mức thấp nhất vẫn đủ to: 15dB ở âm lượng 100% có thể vượt 1W của loa khi pin đầy. Bảng nối chân theo trang pinout của Adafruit cho board MAX98357: 100k xuống GND = 15dB, nối GND = 12dB, để hở = 9dB, nối Vin = 6dB, 100k lên Vin = 3dB.

### LED thông báo
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | LED 0603/0805 | | GPIO21 → R → LED → GND |
| 1 | R hạn dòng 1k | | ~1–2mA là đủ sáng trong hộp; LED sáng liên tục ăn pin ngang cả hộp đang ngủ |

### Cảm ứng
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | IC cảm ứng điện dung | TTP223-BA6 (SOT-23-6) | TOG=0 (direct), AHLB=0 (active HIGH). Firmware đo thời gian giữ nên **không** được dùng chế độ toggle |
| 1 | Tụ Cs 0–50pF | | Chỉnh độ nhạy |
| 1 | Tụ 100nF | | Nguồn IC |
| 1 | Pad đồng cảm ứng trên PCB | | |

### Nguồn
| SL | Linh kiện | Gợi ý mã | Ghi chú |
|---|---|---|---|
| 1 | Cổng USB-C 16 pin (USB 2.0) | | D+/D- → GPIO19/18 |
| 2 | R 5.1k | | CC1, CC2 xuống GND |
| 1 | ESD cho USB | USBLC6-2SC6 | |
| 1 | IC sạc LiPo 1 cell | TP4056 / MCP73831 | Chân CHRG chỉ nối LED sạc (không còn GPIO để đọc) |
| 1 | LED sạc + R 1k | | Chỉ sáng khi cắm USB, không ăn pin |
| 1 | Pin LiPo 3.7V ~1000mAh có mạch bảo vệ | | MEMORY.md tính theo 1000mAh |
| 1 | (Nếu pin không có PCM) DW01A + FS8205A | | |
| 1 | P-MOSFET + Schottky làm power-path | AO3401 + SS14 | Cắm USB thì chạy nguồn USB |
| 1 | LDO 3.3V, ≥500mA, Iq thấp | RT9080-33 (Iq 2µA) / ME6211C33 | Wi-Fi TX đỉnh ~350mA |
| 1 | Công tắc trượt nguồn | | |
| 2 | Tụ 10µF | | In/out LDO |

Không vẽ LED báo nguồn nối cứng (MEMORY.md mục C: làm pin 1000mAh từ 52 ngày còn 9 ngày).

## 3. Firmware phải sửa theo mạch này

1. **Bật USB CDC**: bỏ comment `-D ARDUINO_USB_CDC_ON_BOOT=1` và `-D ARDUINO_USB_MODE=1` trong `platformio.ini`. Không bật thì Serial vẫn chiếm GPIO20/21 (UART0).
2. **Chân ampli** (GPIO20): kéo LOW ngay đầu `setup()`, chỉ HIGH trong lúc phát âm thanh (AudioPlayer), giữ LOW khi ngủ bằng `gpio_hold_en`.
3. **LED thông báo** (`PIN_LED` = 21): code LED đã bị xoá 2026-09-18 (UIController.h:16), phải viết lại. Muốn LED "thở" lúc light sleep thì LEDC phải chạy clock RC_FAST; nếu không, chỉ sáng tĩnh bằng `gpio_hold_en`.
4. **Pin**: `getBatteryPercentage()` vẫn trả cứng 60, `isCharging()` trả `false` (PowerManager.cpp:37). Icon pin và số % gửi lên Firebase là giả, cần ẩn trên UI/web hoặc ghi chú rõ.

## 4. Giữ giờ: bản này KHÔNG có RTC ngoài (chốt 2026-09-22)

Dùng bộ đếm RTC nội của ESP32-C3 (dao động RC ~136kHz, không có thạch anh 32.768kHz). Chấp nhận:
- Mất giờ khi tắt nguồn, pin cạn, brownout. Có giờ lại sau lần sync NTP đầu tiên sau boot.
- Giờ trôi trong lúc ngủ, được bù ở mỗi lần thức sync NTP.

Bản sau muốn giờ độc lập: chip RTC I2C (PCF8563T + thạch anh 32.768kHz CL 12.5pF, hoặc RV-3028-C7) + pin CR1220 qua BAT54C. Cần 2 GPIO: SCL = GPIO9 (nối TFT RST vào net EN để giải phóng), SDA = GPIO21 (LED thông báo chuyển sang CLKOUT của RTC).

## 5. Nếu sau này muốn đo pin

Chuyển đèn nền sang GPIO20 (LEDC ra được mọi chân), dùng GPIO3 (ADC1, không phải strapping) cho cầu chia, và giải phóng GPIO9 cho chân bật ampli bằng cách nối TFT RST vào net EN. Linh kiện thêm: 2 R 1MΩ 1% (BAT+ → GPIO3 → GND) + tụ 100nF tại GPIO3. Lấy áp ở BAT+, không lấy ở VSYS.
