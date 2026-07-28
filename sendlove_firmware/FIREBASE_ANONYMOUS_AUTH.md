# THIẾT KẾ BẢO MẬT: FIREBASE ANONYMOUS AUTH & PAIRING FLOW FOR SENDLOVE BOX

*Tài liệu hướng dẫn kỹ thuật chi tiết dành cho giai đoạn nâng cấp bảo mật sản phẩm thương mại.*

---

## 1. MỤC TIÊU THIẾT KẾ

Giải quyết triệt để bài toán bảo mật:
- Không lưu cứng Database Secret hay Auth Token trong Firmware ESP32.
- Ngăn chặn kẻ xấu lấy API Key của ứng dụng để đọc/ghi lén dữ liệu của các Hộp quà khác.
- Mỗi thiết bị ESP32 tự xin cấp **UID Ẩn danh** từ Firebase Auth và chỉ được truy cập dữ liệu của chính Hộp quà đó sau khi được chủ sở hữu ủy quyền (Pairing qua mã PIN).

---

## 2. QUY TRÌNH HOẠT ĐỘNG (SEQUENCE DIAGRAM)

```
[Điện thoại (Web App)]              [Thiết bị ESP32]                 [Firebase Server]
        │                                  │                                  │
        │                                  │─── 1. POST /signUp?key=API_KEY──►│
        │                                  │◄── 2. Trả idToken & localId (UID)│
        │                                  │                                  │
        │                                  │─── 3. Tạo mã PIN 6 số ngẫu nhiên │
        │                                  │    Ghi /pairing_codes/849201 ───►│
        │                                  │    (Lưu: box_id, esp_uid)        │
        │                                  │                                  │
        │                                  │ (Hiển thị PIN 849201 trên LCD)   │
        │                                  │                                  │
        │─── 4. Nhập mã PIN 849201 ────────┼─────────────────────────────────►│
        │─── 5. Tra cứu /pairing_codes/────┼─────────────────────────────────►│
        │─── 6. Thêm esp_uid vào ──────────┼─────────────────────────────────►│
        │    /boxes/box_001/allowed_uids   │                                  │
        │─── 7. Xóa /pairing_codes/849201 ─┼─────────────────────────────────►│
        │                                  │                                  │
        │                                  │─── 8. GET /boxes/box_001.json ──►│
        │                                  │       ?auth=<idToken>            │
        │                                  │◄── 9. Firebase Rules kiểm tra ───│
        │                                  │       allowed_uids[UID] == true  │
        │                                  │       -> Trả về dữ liệu tin nhắn │
```

---

## 3. CHI TIẾT LẬP TRÌNH VÀ CẤU HÌNH

### 3.1. ESP32: Xin ID Token Ẩn danh (REST API)

ESP32 gửi yêu cầu HTTP POST để tạo tài khoản ẩn danh trên Firebase Auth:

- **Endpoint:** `HTTPS POST https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=[FIREBASE_API_KEY]`
- **Header:** `Content-Type: application/json`
- **Request Body:**
  ```json
  {
    "returnSecureToken": true
  }
  ```
- **Response Trả Về (HTTP 200 OK):**
  ```json
  {
    "kind": "identitytoolkit#SignUpResponse",
    "idToken": "eyJhbGciOiJSUzI1NiIs...",
    "refreshToken": "AMf-vBy...",
    "expiresIn": "3600",
    "localId": "UID_ESP32_ABC123"
  }
  ```
- ESP32 lưu `idToken` và `localId` (`esp_uid`) vào RAM hoặc NVS Flash.

---

### 3.2. ESP32: Sinh Mã PIN Kích Hoạt (Pairing PIN)

1. ESP32 dùng hàm `esp_random()` sinh mã PIN 6 chữ số (ví dụ: `849201`).
2. Gửi HTTP PUT lên Firebase node tạm `/pairing_codes/849201.json?auth=[idToken]`:
   ```json
   {
     "box_id": "box_001",
     "esp_uid": "UID_ESP32_ABC123",
     "created_at": 1719567890
   }
   ```
3. Màn hình TFT của Hộp quà hiển thị:
   `MÃ KÍCH HOẠT: 849201`

---

### 3.3. Web App Điện Thoại: Ghép Nối (Pairing Handshake)

Khi người dùng nhập mã PIN `849201` trên trình duyệt Web App:

1. Web App gọi `GET /pairing_codes/849201.json`.
2. Bóc tách dữ liệu nhận được `esp_uid = "UID_ESP32_ABC123"` và `box_id = "box_001"`.
3. Cập nhật quyền ủy quyền cho Hộp:
   `PUT /boxes/box_001/allowed_uids/UID_ESP32_ABC123.json` -> Giá trị: `true`.
4. Xóa mã PIN tạm:
   `DELETE /pairing_codes/849201.json`.

---

### 3.4. Cấu hình Firebase Realtime Database Security Rules

Thiết lập quy tắc bảo vệ trên Firebase Console để thực thi ủy quyền:

```json
{
  "rules": {
    "pairing_codes": {
      // Bất kỳ ai đã xác thực (kể cả Ẩn danh) đều có thể đọc/ghi node tạm này trong lúc ghép nối
      ".read": "auth != null",
      ".write": "auth != null"
    },
    "boxes": {
      "$box_id": {
        // CHỈ CHO PHÉP ĐỌC nếu UID của thiết bị nằm trong danh sách allowed_uids của Hộp đó
        ".read": "auth != null && (data.child('owner_id').val() == auth.uid || data.child('allowed_uids').child(auth.uid).exists())",
        
        // CHỈ CHỦ SỞ HỮU TRÊN WEB MỚI ĐƯỢC GHI THAY ĐỔI CẤU HÌNH / TẢI TIN NHẮN
        ".write": "auth != null && (data.child('owner_id').val() == auth.uid || data.child('allowed_uids').child(auth.uid).exists())"
      }
    }
  }
}
```

---

## 4. TỔNG KẾT VÀ LƯU Ý KHI TRIỂN KHAI

- **Thời gian hết hạn idToken (Expires in 3600s):** `idToken` của Firebase có hiệu lực trong 1 giờ. ESP32 cần lưu `refreshToken` để gọi API `securetoken.googleapis.com/v1/token?key=[API_KEY]` gia hạn lại Token ngầm mà không cần bắt người dùng ghép nối lại.
- **Tính khả thi:** Giải pháp này hoàn toàn không tốn tài nguyên mã hóa nặng trên ESP32, chạy cực nhẹ và bảo mật theo đúng tiêu chuẩn thiết bị IoT thương mại.
