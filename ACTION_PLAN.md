# TÀI LIỆU ĐẶC TẢ KIẾN TRÚC VÀ KẾ HOẠCH DỰ ÁN: SENDLOVE BOX
*Phiên bản: 1.1 | Trạng thái: Sẵn sàng phát triển*

Tài liệu này đóng vai trò là "Bản thiết kế thi công" (Blueprint) cho toàn bộ dự án Sendlove Box, bao phủ từ phần cứng, firmware, backend cho đến luồng tương tác người dùng.

---

## 1. TỔNG QUAN CÔNG NGHỆ & PHƯƠNG PHÁP THIẾT KẾ

### 1.1. Danh sách công nghệ (Tech Stack)
* **Phần cứng (Microcontroller):** ESP32-C3 (Kiến trúc RISC-V, tối ưu năng lượng).
* **Firmware Framework:** C++ trên nền tảng PlatformIO. Sử dụng FreeRTOS API tích hợp sẵn trong ESP-IDF/Arduino Core để quản lý Task.
* **Cloud Backend:** Firebase Blaze Plan (Pay-as-you-go). Bao gồm các dịch vụ: Authentication (Xác thực), Realtime Database (Lưu cờ trạng thái), Cloud Storage (Lưu file nhị phân) và Hosting (Lưu mã nguồn Web).
* **Frontend Web App:** HTML/CSS/JS thuần hoặc React/Vue. Tích hợp Web Audio API (Thu âm WAV) và Canvas API (Encode Video sang RAW RGB565).

### 1.2. Các phương pháp thiết kế cốt lõi
* **Kiến trúc "Thin Client" (Trạm cuối siêu nhẹ):** Mọi tác vụ nặng (chuyển đổi video, âm thanh) bị ép lên trình duyệt web của người gửi. Hộp quà ESP32 chỉ làm nhiệm vụ "Tải về và Phát mù" để tiết kiệm RAM và CPU.
* **Quản lý năng lượng "Event-Driven Light-Sleep":** ESP32 duy trì trạng thái ngủ nông (Light-sleep) tắt Wi-Fi. Đánh thức bằng đa sự kiện (Multi-wakeup): Hết thời gian Timer 5 phút hoặc Tín hiệu ngắt (Interrupt) từ cảm biến chạm.
* **Smartwatch UI Style:** Màn hình TFT mặc định tắt đèn nền (thông qua băm xung PWM chân BLK). Chỉ sáng lên khi có tương tác vật lý (chạm) hoặc khi tới giờ báo thức.
* **Hệ điều hành thời gian thực (RTOS):** Chuyển từ kiến trúc vòng lặp nguyên thủy sang kiến trúc Task-based (FreeRTOS) để quản lý đa tiến trình song song.

---

## 2. DANH SÁCH LINH KIỆN PHẦN CỨNG (BOM)

| STT | Tên Linh kiện | Chức năng & Thông số kỹ thuật |
| :--- | :--- | :--- |
| 1 | **ESP32-C3 Super Mini** | Vi điều khiển chính, quản lý Wi-Fi, SPI, I2S. |
| 2 | **Màn hình TFT LCD 1.77 inch** | Giao tiếp SPI. Nối chân BLK (Backlight) vào 1 chân GPIO để điều tốc PWM. |
| 3 | **Module MicroSD Card** | Giao tiếp SPI. Đọc/ghi dữ liệu phương tiện. |
| 4 | **Thẻ nhớ MicroSD** | Dung lượng 2GB - 16GB, định dạng FAT32. |
| 5 | **Module MAX98357A** | Amply Class D, giao tiếp I2S (giải mã tín hiệu số ra analog). |
| 6 | **Loa Mini** | Trở kháng 8 Ohm, Công suất 1W - 2W. |
| 7 | **Module TTP223** | Cảm biến điện dung siêu nhạy (Touch Sensor). |
| 8 | **Đèn LED + Trở 330Ω** | Thông báo trạng thái (Hiệu ứng Breathing). |
| 9 | **Pin Lipo 3.7V** | Dung lượng 2000mAh (Kích thước phù hợp lòng hộp). |
| 10 | **Module TP4056 Type-C** | Mạch sạc và bảo vệ pin Lipo. |
| 11 | **Dây điện AWG 30 & AWG 24** | Loại lõi đặc (Solid core) để uốn góc 90 độ, đi dây trực tiếp. |
| 12 | **Vỏ hộp in 3D** | Nhựa PETG hoặc PLA+ (Màu Matte/nhám). |

---

## 3. THIẾT KẾ USE CASE & TÁC NHÂN (ACTORS)

**Tác nhân 1: Người gửi (Sender)**
1. Đăng nhập / Xác thực (Login).
2. Tạo thông điệp Video/GIF (Create Visual Message).
3. Tạo thông điệp Âm thanh (Create Voice Message).
4. Xem trước thông điệp (Preview).
5. Gửi thông điệp (Send Message).
6. Kiểm tra trạng thái Hộp quà (Check Box Status).

**Tác nhân 2: Người nhận (Receiver)**
1. Đăng nhập không gian quản lý (Login).
2. Cài đặt báo thức / Nhắc nhở (Set Alarm).
3. Xem lịch sử thông điệp (View History).
4. Xem dung lượng pin Hộp quà (Check Battery).
5. Chạm để mở quà / Xem giờ (Touch to Wake).

**Tác nhân 3: Hộp quà (Smart Box - Sbox)**
1. Đồng bộ thời gian thực (NTP Sync).
2. Kiểm tra và Tải dữ liệu định kỳ (Fetch Media).
3. Hiển thị Đa phương tiện (Play Media).
4. Quản lý trạng thái Ngủ (Sleep Management).
5. Báo cáo phần trăm pin (Report Battery).

---

## 4. ĐẶC TẢ CHI TIẾT USE CASE

**UC_S05: Gửi thông điệp (Tác nhân: Sender)**
* **Điều kiện tiên quyết:** Sender đã đăng nhập, đã encode xong file `.bin` và `.wav` trên trình duyệt. Sbox đã được kích hoạt.
* **Luồng cơ bản:** Trình duyệt đẩy file `.bin` và `.wav` lên Firebase Cloud Storage. Storage trả về 2 link tải. Trình duyệt cập nhật vào Realtime DB biến `has_new_msg = true` kèm theo 2 link trên. Giao diện báo gửi thành công.
* **Hậu điều kiện:** Máy chủ lưu trữ file sẵn sàng chờ Sbox tải.

**UC_R02: Cài đặt báo thức (Tác nhân: Receiver)**
* **Điều kiện tiên quyết:** Receiver truy cập Web App quản lý.
* **Luồng cơ bản:** Receiver nhập mốc thời gian báo thức (VD: 07:30). Web App đẩy chuỗi thời gian này lên node `config/alarm_time` trên Firebase Realtime DB.
* **Hậu điều kiện:** Sbox sẽ đọc và lưu cấu hình này vào chu kỳ thức dậy tiếp theo.

**UC_R05: Chạm để mở quà hoặc xem giờ (Tác nhân: Receiver & Sbox)**
* **Điều kiện tiên quyết:** Sbox đang trong trạng thái Light-sleep.
* **Luồng cơ bản:** Receiver chạm tay vào vỏ hộp. Cảm biến TTP223 kích hoạt ngắt phần cứng. Sbox thức dậy tức thời, kiểm tra cờ `co_tin_nhan` trên RAM.
* **Nhánh A (co_tin_nhan = true):** Bật đèn nền TFT, xuất đồng thời file từ thẻ SD ra SPI (hình ảnh) và I2S (âm thanh). Chạy xong, chuyển cờ về `false`, tắt màn hình.
* **Nhánh B (co_tin_nhan = false):** Tạm bật Wi-Fi kiểm tra Firebase ("Pull-to-refresh"). Nếu phát hiện có tin nhắn, tải về và chuyển sang Nhánh A. Nếu không có, hiển thị mặt Đồng hồ trong 5 giây, sau đó tắt màn hình.
* **Hậu điều kiện:** Sbox ra lệnh tắt Wi-Fi và gọi hàm vào lại Light-sleep.

**UC_Sbox01: Đồng bộ dữ liệu định kỳ (Tác nhân: Sbox)**
* **Điều kiện tiên quyết:** Sbox vừa thức dậy do hết 5 phút của Timer.
* **Luồng cơ bản:** Sbox bật Wi-Fi, kết nối mạng. Gọi HTTP GET lấy cấu hình báo thức mới và kiểm tra cờ `has_new_msg` trên Firebase. Nếu cờ là `true`, tải stream file `.bin` và `.wav` ghi thẳng vào thẻ SD. Đổi cờ nội bộ `co_tin_nhan = true`. Gọi HTTP PUT lên Firebase chuyển cờ trên cloud thành `false`.
* **Hậu điều kiện:** Dữ liệu nằm an toàn trong thẻ SD, Sbox ngắt mạng và đi ngủ chờ chạm.

---

## 5. MÔ TẢ API VÀ CẤU TRÚC DATABASE (FIREBASE)

**Firebase Realtime Database (Cấu trúc JSON)**
```json
{
  "boxes": {
    "box_id_001": {
      "status": {
        "battery_percent": 85,
        "is_online": false,
        "last_sync_timestamp": 1719567890
      },
      "messaging": {
        "has_new_msg": true,
        "video_url": "[https://firebasestorage.googleapis.com/.../video_001.bin](https://firebasestorage.googleapis.com/.../video_001.bin)",
        "voice_url": "[https://firebasestorage.googleapis.com/.../voice_001.wav](https://firebasestorage.googleapis.com/.../voice_001.wav)"
      },
      "config": {
        "alarm_time": "07:30",
        "is_alarm_active": true,
        "timezone": 7
      }
    }
  }
}
Firebase Cloud Storage Structure

/media/box_id_001/video_latest.bin (Ghi đè mỗi khi có tin nhắn mới).

/media/box_id_001/voice_latest.wav

6. KIẾN TRÚC FIRMWARE: PHƯƠNG PHÁP TASK (FREERTOS)
Task 1: Task_PowerManager (Độ ưu tiên: Cao nhất)
Nhiệm vụ quản lý sự sống còn của hộp. Lắng nghe cờ báo công việc từ các Task khác. Ra lệnh ngắt Wi-Fi, cài đặt Timer 5 phút, cài ngắt GPIO cảm biến chạm và gọi lệnh Light-sleep. Khi thức dậy, xác định nguyên nhân và phân luồng đánh thức các Task tương ứng.

Task 2: Task_NetworkHandler (Độ ưu tiên: Trung bình)
Chuyên trách giao tiếp mạng (Chỉ chạy khi PowerManager gọi). Bật Wi-Fi, cập nhật giờ NTP. Gọi HTTP GET/PUT tương tác với Firebase. Quản lý việc stream dữ liệu từ URL thẳng vào File object của thẻ MicroSD (Sử dụng buffer để chống tràn RAM).

Task 3: Task_MediaPlayer (Độ ưu tiên: Cao)
Phát hình ảnh và âm thanh mượt mà (Chỉ chạy khi có lệnh chạm). Sử dụng DMA (Direct Memory Access) để đọc block dữ liệu từ SD Card. Bơm song song buffer ảnh ra SPI và buffer âm thanh ra I2S. Đảm bảo đồng bộ hình tiếng.

Task 4: Task_UI_Controller (Độ ưu tiên: Thấp)
Quản lý hiệu ứng hình ảnh phụ. Điều khiển băm xung PWM cho LED nhịp thở. Điều tốc PWM chân BLK của màn hình (Sáng 100% ban ngày, 20% ban đêm). Xử lý chống nhiễu (Debounce) cho tín hiệu chạm.

7. YÊU CẦU PHI CHỨC NĂNG (NON-FUNCTIONAL REQUIREMENTS)
Hiệu năng (Performance): Tốc độ phản hồi khi chạm để bật màn hình phải nhỏ hơn 50ms. Tốc độ khung hình Video (Framerate) duy trì 15 - 20 FPS.

Năng lượng (Power): Dòng điện khi ngủ (Light-sleep) phải dưới 2mA. Thời lượng pin duy trì từ 15 đến 20 ngày với viên pin 2000mAh (Chu kỳ check mạng 5 phút/lần).

Lưu trữ (Storage): File .bin và .wav không vượt quá 15MB/tin nhắn để đảm bảo tính ổn định khi tải xuống bằng vi điều khiển.

Bảo mật (Security): Mọi kết nối API đi qua HTTPS. Firebase Rules chỉ cho phép user xác thực đúng box_id được quyền ghi/đọc.

Khả năng bảo trì (Maintainability): Không hàn chết linh kiện thành một khối. Sử dụng ngàm in 3D hoặc băng keo xốp 3M để cố định module.

8. CÁC LUỒNG HOẠT ĐỘNG CHÍNH (MAIN WORKFLOWS)
Luồng 1: Định kỳ nhận tin nhắn ngầm (Background Sync)
Sbox ngủ 5 phút -> Thức dậy bởi Timer -> Task_Network bật mạng, check Firebase -> Tải file vào thẻ SD -> Đổi cờ co_tin_nhan = true -> Task_UI bật LED nhịp thở -> Task_PowerManager ngắt mạng, tiếp tục ngủ.

Luồng 2: Tương tác mở quà tiêu chuẩn (Standard Playback)
Sbox đang ngủ (LED nháy) -> Người nhận chạm tay -> Thức dậy bởi GPIO Interrupt -> Task_PowerManager kiểm tra thấy có tin nhắn -> Task_MediaPlayer đẩy dữ liệu từ SD ra màn hình và loa -> Phát xong, tắt LED, tắt màn hình -> Task_Network báo Firebase đã đọc -> Vào giấc ngủ.

Luồng 3: Tương tác "Kéo để làm mới" hoặc xem giờ
Sbox đang ngủ (Không LED) -> Người nhận chạm tay -> Thức dậy -> Task_Network bật mạng kiểm tra Firebase.
Nếu có quà ẩn: Tải file và gọi Task_MediaPlayer phát ngay lập tức.
Nếu không có quà: Task_UI hiển thị mặt đồng hồ trong 5 giây, sau đó tắt màn hình và tiếp tục ngủ.