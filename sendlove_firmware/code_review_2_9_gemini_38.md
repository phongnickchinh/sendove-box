# BÁO CÁO KIỂM TOÁN MÃ NGUỒN VÀ BẢO MẬT FIRMWARE (CODE REVIEW)
**Ngày thực hiện:** 02/09/2026  
**Thực hiện bởi:** Gemini 3.8 Flash (Autonomous Code Security Audit)  
**Mục tiêu:** Quét toàn bộ codebase `sendlove_firmware` (ESP32-C3), phân tích thuần túy trên mã nguồn C/C++, phát hiện các lỗ hổng bảo mật, lỗi logic phần cứng, xung đột bộ nhớ và rủi ro vận hành.

---

## 1. TỔNG QUAN ĐÁNH GIÁ (EXECUTIVE SUMMARY)

Hệ thống firmware được xây dựng theo mô hình Event-driven trên nền FreeRTOS với sự phân tầng rõ ràng (Storage Abstraction `IStorageProvider`, Display Driver LovyanGFX, Audio/Video Media Player). Tuy nhiên, qua quá trình kiểm tra sâu mã nguồn, phát hiện 12 điểm yếu kỹ thuật thuộc nhiều mức độ nghiêm trọng:

| Mức độ | Số lượng | Vấn đề trọng tâm |
|---|:---:|---|
| 🔴 **CRITICAL / HIGH** | 5 | Insecure TLS (MitM), OTA WebServer mở không xác thực, Xung đột chân NAND CS với LED báo hiệu, Lệnh Erase 5.5MB gây nghẽn Watchdog (TWDT), Lộ Database Secret qua URL params |
| 🟡 **MEDIUM** | 4 | Race condition trạng thái hệ thống, Captive Portal XSS/HTML Injection, Nguy cơ mòn sớm Sector 0 (Flash Endurance), Tải stream chunked không trần |
| 🟢 **LOW / CODE SMELL** | 3 | Tràn Stack frame cục bộ ở hàm text wrap, Mức pin bị gán cứng (mock), Thiếu bắt buộc kiểm tra MD5 trong OTA |

---

## 2. CHI TIẾT CÁC LỖ HỔNG NGHIÊM TRỌNG (CRITICAL & HIGH SEVERITY)

### 🔴 VULN-01: Vô hiệu hóa hoàn toàn xác thực chứng chỉ TLS (`client.setInsecure()`) — Lỗ hổng Man-in-the-Middle (MitM)
* **Vị trí file:** `lib/NetworkManager/NetworkManager.cpp` (Dòng 521, 547, 584, 603, 664)
* **Mô tả kỹ thuật:**
  Tất cả các phiên giao tiếp HTTPS gửi tới Firebase Realtime Database (`boxes/...`, `messages/...`) và Firebase Storage đều khởi tạo `WiFiClientSecure client` và ngay lập tức gọi:
  ```cpp
  client.setInsecure();
  ```
  Lệnh này tắt toàn bộ việc xác thực CA Root Certificate, tính hợp lệ của chứng chỉ SSL/TLS và bỏ qua Hostname Verification.
* **Nguy cơ & Tác động:**
  Bất kỳ kẻ tấn công nào trong cùng mạng Wi-Fi (hoặc thông qua kỹ thuật giả mạo DNS, phát trạm Wi-Fi giả mạo / Rogue AP) đều có thể chặn bắt gói tin (sniffing), giải mã toàn bộ nội dung tin nhắn, lấy cắp `FIREBASE_AUTH_SECRET`, hoặc can thiệp sửa đổi dữ liệu JSON trả về để ép thiết bị tải các file độc hại.
* **Khuyến nghị khắc phục:**
  Tích hợp chứng chỉ gốc Google Trust Services (GTS Root R1) hoặc ít nhất sử dụng cơ chế SSL Fingerprint / Public Key Pinning trong `WiFiClientSecure`.

---

### 🔴 VULN-02: Endpoint Cập nhật Firmware OTA mở hoàn toàn, không có xác thực (Unauthenticated OTA)
* **Vị trí file:** `lib/OtaHandler/OtaHandler.cpp` (Dòng 74–80)
* **Mô tả kỹ thuật:**
  Module `OtaHandler` đăng ký 2 route HTTP POST công khai trên WebServer cổng 80:
  - `POST /api/ota/begin`
  - `POST /api/ota/upload`
  Các route này xử lý nạp firmware trực tiếp vào flash thông qua thư viện `<Update.h>` nhưng **không hề kiểm tra bất kỳ thông tin nhận dạng nào** (không mật khẩu, không API Key, không Session Token). Đồng thời, header `Access-Control-Allow-Origin: *` được gán cố định, cho phép mọi domain bên ngoài gửi request tới.
* **Nguy cơ & Tác động:**
  Bất kỳ máy tính hoặc điện thoại nào trong cùng mạng LAN (hoặc thậm chí một website độc hại mà người dùng mở trên trình duyệt cùng mạng Wi-Fi thực hiện Cross-Site Request) đều có thể gửi file binary tùy ý để ghi đè firmware của hộp, chiếm toàn quyền kiểm soát thiết bị (Remote Code Execution).
* **Khuyến nghị khắc phục:**
  1. Thêm một khóa bí mật OTA (OTA Token/Password) lưu trong NVS, yêu cầu header `X-OTA-Key` hoặc `Authorization` trong request `POST /api/ota/begin`.
  2. Áp dụng kiểm tra chữ ký số (Cryptographic Signature Verification) của bản build trước khi chuyển đổi phân vùng boot OTA.

---

### 🔴 VULN-03: Xung đột chân phần cứng: Nháy đèn LED trên chân NAND CS (`PIN_NAND_CS = 8`)
* **Vị trí file:** `src/main.cpp` (Dòng 290–299)
* **Mô tả kỹ thuật:**
  Trong vòng lặp thức dậy sau Timer Sleep, code thực hiện đoạn nháy đèn LED chớp xanh dương:
  ```cpp
  // Nháy đèn xanh dương (GPIO 8 - Bản SuperMini, trùng chân NAND CS) để báo hiệu wakeup ngầm.
  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);  // Đèn sáng (Active LOW) / NAND CS ghim xuống
  delay(30);
  digitalWrite(8, HIGH); // Đèn tắt / NAND CS nhả ra
  delay(70);
  digitalWrite(8, LOW);
  delay(30);
  digitalWrite(8, HIGH);
  ```
  Tuy nhiên, theo sơ đồ chân tại `include/config.h:21`, `PIN_NAND_CS = 8`.
* **Nguy cơ & Tác động:**
  Chân Chip Select (CS) của chip nhớ SPI Flash W25Q128 có cơ chế Active LOW. Khi chân này bị kéo xuống `LOW`, chip Flash W25Q128 bắt đầu lắng nghe tín hiệu trên bus SPI. Nếu trong thời điểm thức dậy có xung điện hoặc nhiễu clock trên đường SCK/MOSI, chip Flash có thể hiểu nhầm thành các op-code SPI ngẫu nhiên. Trong tình huống xấu (nếu lệnh Write Enable 0x06 vô tình được nhận), dữ liệu Flash có thể bị ghi đè hoặc hỏng bảng slot. Ngoài ra, việc dùng chung chân Chip Select để làm đèn debug tạo ra sự phụ thuộc chéo nguy hiểm giữa phần cứng lưu trữ và hiển thị.
* **Khuyến nghị khắc phục:**
  Loại bỏ hoàn toàn đoạn code nháy GPIO 8 này. Nếu cần đèn báo hiệu, sử dụng chân LED chuyên dụng (ví dụ GPIO 20) hoặc nháy đèn nền LCD thông qua `DisplayDriver::wakeupFlash()`.

---

### 🔴 VULN-04: Lệnh xóa 5.5MB Flash đồng bộ gây nghẽn CPU và nguy cơ kích hoạt Task Watchdog (TWDT)
* **Vị trí file:** `lib/Storage/NandStorageProvider.cpp` (Dòng 125) và `lib/NandStorage/NandStorage.cpp` (Dòng 245–304)
* **Mô tả kỹ thuật:**
  Khi mở một slot để chuẩn bị ghi dữ liệu mới (`openForWrite`), code gọi:
  ```cpp
  _nand.eraseRange(NAND_SLOT_ADDRS[_activeSlot], _slotCapacity);
  ```
  Trong đó `_slotCapacity` là toàn bộ dung lượng của 1 slot = `0x560000 - 0x010000` = **5,570,560 bytes (~5.5MB)**.
  Hàm `eraseRange()` chia nhỏ 5.5MB thành các Block Erase 64KB (tổng cộng 87 block). Mỗi lần xóa 64KB, chip W25Q128 mất trung bình từ 150ms đến 2000ms.
  Tổng thời gian xóa 87 block mất từ **13 giây đến hơn 40 giây**. Trong suốt thời gian này, hàm `waitBusyInternal()` liên tục thăm dò trạng thái qua vòng lặp bận `delayMicroseconds(100)` mà không nhường CPU cho FreeRTOS Scheduler.
* **Nguy cơ & Tác động:**
  1. Task IDLE (Priority 0) bị bỏ đói liên tục trên 5 giây. Nếu Task Watchdog Timer (TWDT) của ESP-IDF được kích hoạt, chip sẽ bị panic và reboot đột ngột.
  2. Kết nối socket HTTP tải file từ Firebase Storage sẽ bị quá hạn (Read/Connect Timeout) vì ESP32 không thể đọc socket trong suốt 13-40 giây xóa Flash, dẫn đến việc tải file luôn thất bại.
* **Khuyến nghị khắc phục:**
  Chuyển sang cơ chế **Erase-as-you-write (Cuốn chiếu)**: Khi mở slot chỉ cần xóa Sector đầu tiên (4KB); trong quá trình `writeChunk()`, con trỏ ghi sắp chạm tới ranh giới sector/block tiếp theo thì mới phát lệnh xóa sector/block đó kèm `vTaskDelay(1)`.

---

### 🔴 VULN-05: Truyền Database Secret trực tiếp qua URL Query Parameter
* **Vị trí file:** `lib/NetworkManager/NetworkManager.cpp` (Dòng 525, 551, 588, 608, 672)
* **Mô tả kỹ thuật:**
  Tất cả các REST API gọi lên Firebase Realtime Database đều sử dụng cấu trúc:
  ```cpp
  snprintf(url, sizeof(url), "https://%s/...json?auth=%s", FIREBASE_HOST, ..., FIREBASE_AUTH_SECRET);
  ```
* **Nguy cơ & Tác động:**
  Chuỗi `FIREBASE_AUTH_SECRET` (Database Secret) cấp quyền quản trị cao nhất (toàn quyền đọc/ghi toàn bộ cơ sở dữ liệu Firebase). Việc truyền qua tham số URL (`?auth=...`) khiến secret này bị ghi lại trong nhật ký proxy mạng, bộ nhớ cache của router, log máy chủ và tiêu đề HTTP Referer. Nếu kết hợp với lỗ hổng `client.setInsecure()`, secret này sẽ bị lộ hoàn toàn.
* **Khuyến nghị khắc phục:**
  Chuyển đổi từ Firebase Database Secret cũ sang hệ thống xác thực Firebase Authentication (Anonymous Sign-In hoặc Service Account Token) và truyền token qua Authorization Header.

---

## 3. CHI TIẾT CÁC LỖ HỔNG TRUNG BÌNH (MEDIUM SEVERITY)

### 🟡 VULN-06: Nguy cơ mòn sớm ô nhớ tại Sector 0 do thiếu cơ chế dàn đều ghi (Flash Endurance Risk)
* **Vị trí file:** `lib/NandStorage/NandStorage.cpp` (Dòng 369–379, hàm `writeSlotTable()`)
* **Mô tả kỹ thuật:**
  Bảng quản lý Slot (`SlotTable`) được đặt cố định tại Sector 0 (`0x000000`). Bất kỳ khi nào thiết bị:
  - Tải xong video/ảnh mới (`closeWrite`)
  - Ghi thêm audio nối tiếp (`closeAppend`)
  - Đọc và đánh dấu tin nhắn đã xem (`markAsRead`)
  - Hủy bỏ lượt tải bị lỗi (`discardWrite`)
  - Cập nhật text caption (`setItemText`)
  Hàm `writeSlotTable()` đều thực hiện:
  ```cpp
  eraseRange(0x000000, 4096);
  writeRaw(0x000000, header, sizeof(header));
  ```
* **Nguy cơ & Tác động:**
  Chu kỳ sống (endurance) của chip SPI Flash thông thường là khoảng 100,000 lần xóa/ghi cho mỗi sector. Vì Sector 0 bị xóa và ghi đè liên tục trong mọi thao tác người dùng, sector này sẽ bị thoái hóa và xuất hiện bit hỏng sớm hơn rất nhiều so với toàn bộ dung lượng 16MB của chip. Khi Sector 0 hỏng, toàn bộ cấu trúc slot bị mất, thiết bị sẽ kẹt ở màn hình `Storage Err!`.
* **Khuyến nghị khắc phục:**
  Chuyển các cờ trạng thái thay đổi thường xuyên (như `unreadBitmask`, con trỏ đọc) sang lưu trữ trong ESP32 NVS Flash (vốn đã tích hợp sẵn thuật toán Wear Leveling của ESP-IDF). Chỉ ghi lại Slot Table trên NAND khi cấu trúc file vật lý thực sự thay đổi.

---

### 🟡 VULN-07: Bất đồng bộ trạng thái giữa các Task FreeRTOS (Race Condition)
* **Vị trí file:** `src/main.cpp` (Dòng 52, 95, 262) và `lib/NetworkManager/NetworkManager.h` (Dòng 93)
* **Mô tả kỹ thuật:**
  1. Biến `currentAppState` (kiểu enum `AppState`) là một biến toàn cục thông thường, không được khai báo `std::atomic` hay bảo vệ bởi Mutex. Biến này được ghi bởi `Task_UIController` (Priority 5) và được đọc/ghi đồng thời bởi `Task_MediaPlayer` (Priority 3).
  2. Hàm `NetworkManager::isSyncing()` kiểm tra:
     ```cpp
     return _isSyncing || _isFirebaseSyncing || _isNtpSyncing || _isDownloadingMedia;
     ```
     Dù các biến có từ khóa `volatile`, việc đọc 4 biến riêng biệt không có atomic lock có thể dẫn đến việc đọc trạng thái chắp vá giữa các chu kỳ context switch của hệ điều hành.
* **Khuyến nghị khắc phục:**
  Khai báo `std::atomic<AppState> currentAppState` hoặc sử dụng FreeRTOS Event Groups (`xEventGroupSetBits` / `xEventGroupWaitBits`) để quản lý cờ trạng thái hệ thống.

---

### 🟡 VULN-08: Captive Portal thiếu lọc mã độc (XSS / HTML Injection)
* **Vị trí file:** `lib/NetworkManager/NetworkManager.cpp` (Dòng 327)
* **Mô tả kỹ thuật:**
  Trong hàm quét Wi-Fi `handleCaptiveScan()`, trường SSID được escape ký tự nháy cho JSON. Tuy nhiên, khi đưa vào trang HTML của Captive Portal (`captive_portal_html.h`), nếu một Access Point lạ phát sóng SSID chứa mã HTML (ví dụ: `Sendlove"><script>alert(1)</script>`), mã này có thể được trình duyệt của người dùng thực thi khi đang kết nối vào Wi-Fi cấu hình `SendloveBox-Setup`.
* **Khuyến nghị khắc phục:**
  Thực hiện sanitize/escape các ký tự HTML (`<`, `>`, `&`, `"`) trước khi render ra giao diện web.

---

### 🟡 VULN-09: Tải dữ liệu dạng Chunked không giới hạn kích thước trần
* **Vị trí file:** `lib/NetworkManager/NetworkManager.cpp` (Dòng 1025)
* **Mô tả kỹ thuật:**
  Khi server phản hồi với tiêu đề `Transfer-Encoding: chunked`, `http.getSize()` trả về `-1`. Vòng lặp tải dữ liệu:
  ```cpp
  while (http.connected() && (len > 0 || len == -1)) { ... }
  ```
  Sẽ liên tục đọc và ghi vào flash cho đến khi server đóng kết nối. Dù `NandStorageProvider::writeChunk()` có chặn khi vượt quá `_slotCapacity`, thiết bị vẫn có thể bị kẹt trong trạng thái tải dữ liệu rác trong nhiều phút.
* **Khuyến nghị khắc phục:**
  Đặt trần kích thước tối đa cho file media: nếu `totalRead > MAX_MEDIA_BYTES` (ví dụ 5MB) thì chủ động ngắt kết nối và báo lỗi.

---

## 4. CÁC VẤN ĐỀ VỀ TÀI NGUYÊN & CODE QUALITY (LOW SEVERITY)

### 🟢 VULN-10: Cấp phát Stack frame cục bộ quá lớn trong hàm `showWrappedText`
* **Vị trí file:** `lib/DisplayDriver/DisplayDriver.cpp` (Dòng 97–103)
* **Mô tả kỹ thuật:**
  Hàm `showWrappedText()` khai báo các biến cục bộ trực tiếp trên stack:
  ```cpp
  char buf[300];
  char lines[16][48]; // 768 bytes
  char currentLine[64];
  char trial[64];
  ```
  Tổng dung lượng stack tiêu thụ cho riêng các biến này là **gần 1.2 KB**. Nếu hàm này được gọi từ một task có stack nhỏ (ví dụ 2KB-4KB), nó sẽ gây ra nguy cơ tràn stack (Stack Overflow) âm thầm.
* **Khuyến nghị:** Chuyển mảng `lines` sang dạng con trỏ tĩnh hoặc cấp phát động, hoặc truyền buffer vào từ caller.

### 🟢 VULN-11: Mức pin đang bị gán cứng cố định (Hardcoded Battery Mock)
* **Vị trí file:** `lib/PowerManager/PowerManager.cpp` (Dòng 38)
* **Mô tả kỹ thuật:**
  Hàm `getBatteryPercentage()` luôn trả về giá trị cứng:
  ```cpp
  return 60; // Giả lập mức pin 60%
  ```
  Hệ thống quản lý nguồn và tự động ngủ sâu (`getRecommendedSleepMode()`) bị vô hiệu hóa hoàn toàn. Nếu pin LiPo cạn, thiết bị sẽ bị sụt áp đột ngột (Brownout) thay vì chuyển sang Deep Sleep an toàn.

### 🟢 VULN-12: Tham số kiểm tra mã băm MD5 là tùy chọn trong OTA
* **Vị trí file:** `lib/OtaHandler/OtaHandler.cpp` (Dòng 32)
* **Mô tả kỹ thuật:**
  ```cpp
  if (md5.length() == 32) Update.setMD5(md5.c_str());
  ```
  Nếu client không truyền tham số `md5`, quá trình nạp OTA vẫn diễn ra mà không có bước kiểm tra tính toàn vẹn của file binary. Khi truyền qua sóng Wi-Fi bị chập chờn, dữ liệu hỏng có thể làm phân vùng firmware bị lỗi (brick).

---

## 5. LỘ TRÌNH KHẮC PHỤC ĐỀ XUẤT (ACTIONABLE REMEDIATION ROADMAP)

### Giai đoạn 1: Khắc phục Khẩn cấp (Hardware & Flash Stability)
1. **Gỡ bỏ nháy GPIO 8**: Xóa đoạn `pinMode(8, OUTPUT)` trong `main.cpp:290-299` để loại bỏ nguy cơ gửi xung rác vào chip Flash NAND W25Q128.
2. **Cơ chế Erase cuốn chiếu**: Thay đổi lệnh `eraseRange` 5.5MB trong `NandStorageProvider::openForWrite()` thành xóa từng Block 64KB theo nhu cầu thực tế khi tải dữ liệu, triệt tiêu nguy cơ kích hoạt Task Watchdog Timer và timeout mạng.
3. **Giảm stack frame `showWrappedText`**: Thu nhỏ hoặc tái sử dụng buffer để tiết kiệm 1KB stack cho Display Task.

### Giai đoạn 2: Củng cố Bảo mật Mạng & OTA
1. **Thêm Khóa Token cho OTA**: Thêm kiểm tra `X-OTA-Token` trong `OtaHandler::handleBegin()`.
2. **Thêm trần dung lượng tải**: Đặt giới hạn 5MB cho các luồng HTTP chunked stream trong `NetworkManager`.
3. **Tích hợp Chứng chỉ GTS Root CA**: Cài đặt Root Certificate cho `WiFiClientSecure` thay vì gọi `setInsecure()`.

---
*Báo cáo được lưu trữ chính thức tại `code_review_2_9_gemini_38.md` để phục vụ các chu kỳ bảo trì và nâng cấp firmware tiếp theo.*
