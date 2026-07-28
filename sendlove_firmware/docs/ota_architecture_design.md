# Kiến Trúc Quản Lý OTA Đa Tầng (Dual-Flag & Multi-Trigger OTA Architecture)

Tài liệu này lưu trữ thiết kế kiến trúc nâng cấp Firmware từ xa (OTA) nâng cao cho **Sendlove Box**, được thống nhất qua thảo luận thiết kế hệ thống. Kiến trúc này cân bằng giữa **tính an toàn năng lượng, sự chủ động của nhà phát triển và trải nghiệm người dùng không phụ thuộc App**.

---

## 1. Phân Tách Cờ Báo Cloud (Dual-Flag Cloud Control)

Hệ thống Firebase Realtime Database quản lý 2 cờ nâng cấp riêng biệt tại node `boxes/{box_id}/flags`:

```json
{
  "flags": {
    "ota_flag": false,
    "emergency_ota": false
  }
}
```

### 1.1. Cờ `ota_flag` (Standard Feature Update - Cập nhật tính năng)
- **Mục đích**: Dùng cho các bản cập nhật giao diện, thêm tính năng mới, tối ưu hóa hiệu năng.
- **Yêu cầu**: **BẮT BUỘC có xác nhận** từ một trong các kênh người dùng trước khi tải và nạp.
- **Không tự động nạp ngầm** nếu chưa có sự đồng ý của người dùng.

### 1.2. Cờ `emergency_ota` (Critical Patch - Vá lỗi hệ thống khẩn cấp)
- **Mục đích**: Dùng cho các bản vá lỗi bảo mật nghiêm trọng, vá lỗi tràn bộ nhớ (Memory Leak), sửa lỗi đứt kết nối Cloud API.
- **Yêu cầu**: **TỰ ĐỘNG nạp ngầm** ngay khi ESP32 thức dậy phát hiện cờ, bỏ qua bước xác nhận người dùng.
- **Điều kiện duy nhất**: Phải thỏa mãn lớp bảo vệ nguồn điện (Battery Guard).

---

## 2. Các Kênh Kích Hoạt Nâng Cấp (Multi-Trigger Channels)

Khi `ota_flag == true`, người dùng có thể kích hoạt tiến trình nạp bằng 1 trong 3 kênh linh hoạt:

```
                      ┌──────────────────────────────────────────┐
                      │  Cloud Database (ota_flag == true)       │
                      └────────────────────┬─────────────────────┘
                                           │
         ┌─────────────────────────────────┼─────────────────────────────────┐
         ▼                                 ▼                                 ▼
┌──────────────────┐             ┌──────────────────┐             ┌────────────────────┐
│ Kênh 1: App      │             │ Kênh 2: App      │             │ Kênh 3: Standby UI │
│ Mobile Sender    │             │ Mobile Receiver  │             │ & Touch TTP223     │
└────────┬─────────┘             └────────┬─────────┘             └─────────┬──────────┘
         │                                │                                 │
         └────────────────────────────────┼─────────────────────────────────┘
                                          │
                                          ▼
                         ┌──────────────────────────────────┐
                         │  Kích hoạt ESP32 Stream OTA      │
                         └──────────────────────────────────┘
```

1. **Kênh 1 - App Mobile Sender**: Sender xem được tình trạng Pin và Online của Box trên App $\rightarrow$ Bấm nút "Cập nhật Firmware cho Box".
2. **Kênh 2 - App Mobile Receiver**: Receiver nhận thông báo cập nhật trên App di động $\rightarrow$ Bấm "Đồng ý nâng cấp".
3. **Kênh 3 - Độc lập trên Box (Standby UI + Touch Gesture)**:
   - `LayoutEngine` hiển thị một icon nhỏ `🚀 (New FW)` ở góc màn hình Standby.
   - Receiver tương tác trực tiếp trên chiếc hộp bằng **Chuỗi cảm ứng (Multi-tap Gesture)** trên TTP223 để kích hoạt nạp mà không cần cài App.

---

## 3. Thuật Toán Cảm Ứng Đa Nhịp (Multi-Tap Touch Algorithm)

Để tránh việc lau chùi hoặc cầm nắm vô tình kích hoạt OTA ngoài ý muốn trên cảm ứng đơn điểm TTP223:

- **Quy tắc kích hoạt (Trigger Rule)**: **Chạm 3 lần liên tiếp** trong cửa sổ thời gian 1.5 giây.
- **Thông số kỹ thuật**:
  - `TAP_WINDOW_MS = 1500`: Tổng thời gian cho 3 nhịp chạm.
  - `MIN_PRESS_MS = 100`: Thời gian chạm tối thiểu cho 1 nhịp (chống nhiễu điện dung).
  - `MAX_PRESS_MS = 600`: Thời gian chạm tối đa (nếu giữ lâu hơn sẽ tính là Long Press).
- Khi nhận diện đúng chuỗi 3 nhịp chạm và `ota_flag == true` $\rightarrow$ Phát âm thanh/hiệu ứng xác nhận và khởi chạy `OtaHandler`.

---

## 4. Lớp Bảo Vệ Nguồn Điện Cứng (Hardware Battery Guard)

Cho dù kích hoạt từ bất kỳ kênh nào (`emergency_ota`, App Sender, App Receiver hay Touch Gesture), tại hàm `OtaHandler::begin()`, Firmware ESP32 **LUÔN thực thi lớp bảo vệ phần cứng**:

```cpp
bool OtaHandler::canStartOTA(uint8_t batPercent, bool isCharging) {
    // Nếu pin dưới 20% và KHÔNG cắm sạc -> Từ chối OTA để bảo vệ phần cứng
    if (batPercent < 20 && !isCharging) {
        Serial.println(F("[OtaHandler] ERROR: Battery too low for OTA (< 20%)"));
        return false;
    }
    return true;
}
```

- **Hành vi khi bị từ chối do Pin yếu**:
  - Hiển thị thông báo `Battery Low! Please Charge` trên màn hình TFT trong 3 giây.
  - Cập nhật trạng thái `ota_status = "deferred_low_battery"` lên Cloud.

---

## 5. Cấu Trúc Partition Flash & Phôi Phục Khi Lỗi (Rollback)

- **Custom Partition Table**: Phân chia Flash 4MB thành 2 partition app bằng nhau:
  - `app0` (1.75 MB): Chạy phiên bản firmware hiện tại.
  - `app1` (1.75 MB): Ghi dữ liệu stream firmware mới.
- **Tự động Rollback**: Sử dụng cơ chế `esp_ota_mark_app_valid_cancel_rollback()` của ESP-IDF. Bản firmware mới sau khi boot bắt buộc phải xác nhận kết nối Wi-Fi & Firebase thành công trong 30s đầu tiên; nếu crash/reboot lặp lại, chip tự động rollback về phiên bản firmware cũ ổn định trước đó.

---

## 6. Lộ Trình Triển Khai (Roadmap)

- [x] **Phase 1-3B (Hiện tại)**: Đã hoàn thành hạ tầng NVS, PowerManager, NetworkManager (Firebase REST API Sync) và Sleep lifecycle.
- [ ] **Phase 3C**: Triển khai `OtaHandler` với HTTPS Stream từ Firebase Storage và lớp bảo vệ `canStartOTA()`.
- [ ] **Phase 4**: Triển khai `emergency_ota` và Icon thông báo `LayoutEngine` + Thuật toán Multi-tap Gesture trên TTP223.
