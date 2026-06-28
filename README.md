# Sendlove Box

Dự án Hộp quà thông minh kết nối Wi-Fi, hiển thị video và phát âm thanh từ người gửi thông qua Firebase.

## Tổng quan phần cứng
- Vi điều khiển: ESP32-C3 (Kiến trúc RISC-V, tối ưu năng lượng)
- Màn hình: TFT LCD 1.77 inch (Giao tiếp SPI)
- Âm thanh: Module amply MAX98357A (Giao tiếp I2S) + Loa Mini
- Lưu trữ: Thẻ nhớ MicroSD + Module MicroSD (Giao tiếp SPI)
- Cảm biến chạm: Module điện dung TTP223
- Nguồn: Pin Lipo 3.7V + Mạch sạc TP4056 Type-C

## Kiến trúc phần mềm
- **Firmware**: C++ trên nền tảng PlatformIO. Sử dụng hệ điều hành thời gian thực FreeRTOS (Task-based architecture).
- **Backend**: Firebase Realtime Database (lưu cờ trạng thái) & Cloud Storage (lưu trữ phương tiện).
- **Frontend**: Web App (HTML/CSS/JS, Web Audio API, Canvas API) nhằm offload tác vụ xử lý tệp tin từ ESP32-C3 lên trình duyệt.

Chi tiết về đặc tả use case, luồng hoạt động và phương pháp thiết kế xem tại [ACTION_PLAN.md](ACTION_PLAN.md).
