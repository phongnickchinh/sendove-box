# Chiến lược Multi-Platform: Desktop & Mobile (Android/iOS)

## Bối cảnh

Backend hiện tại (`sendlove_backend`) là REST API stateless chạy trên Firebase
Cloud Functions (Express), xác thực bằng Bearer token (Firebase ID token).
Không dùng cookie/session, không phụ thuộc CORS ở phía native app → gọi được
từ bất kỳ client nào (web, desktop, mobile) mà không cần sửa backend.

Web client (`sendlove_web`) dùng React + Vite, gọi REST API qua `axios`
(`src/api/client.js`) và dùng thẳng Firebase JS SDK cho Realtime Database +
Storage (`src/config/firebase.js`). Đăng nhập bằng Google qua
`signInWithPopup`.

**Kết luận nền tảng**: không cần sửa backend để hỗ trợ desktop/mobile. Toàn bộ
công việc nằm ở lớp client.

## Lựa chọn công nghệ

### Android + iOS: Capacitor

Bọc lại `sendlove_web` (bản build Vite) chạy trong WebView native, tái sử
dụng ~100% code React/Firebase JS SDK/API client hiện có. Cùng một codebase
phục vụ cả Android lẫn iOS (`npx cap add android`, `npx cap add ios`).

Việc cần làm thêm:
- Thay `signInWithPopup` bằng `@capacitor/google-auth` (Google chặn OAuth
  trong embedded WebView) → lấy `idToken` native rồi `signInWithCredential`
  vào Firebase.
- Trỏ `VITE_API_URL` sang endpoint production khi build app đóng gói (không
  dùng `127.0.0.1`).
- Nếu cần sau này: `@capacitor/camera`, `@capacitor/push-notifications` (yêu
  cầu cấu hình thêm APNs key trong Firebase Console cho iOS).

Yêu cầu riêng cho iOS: cần máy Mac (hoặc CI cloud-Mac: GitHub Actions macOS
runner, Codemagic, EAS Build) để build bằng Xcode, và Apple Developer account
(99 USD/năm) để chạy trên thiết bị thật / TestFlight / App Store.

Lựa chọn bị loại:
- **React Native**: tái dùng được logic (api/client.js, auth.js...) nhưng
  phải viết lại toàn bộ UI bằng RN components — tốn công nhiều hơn đáng kể so
  với lợi ích (mượt hơn, native widget) mà app này (hiển thị tin nhắn/media/
  báo thức) không thực sự cần.
- **Native Kotlin / Swift**: không tái dùng được gì, phải maintain codebase
  riêng biệt vĩnh viễn, chỉ đáng cân nhắc nếu cần hiệu năng/tích hợp hệ điều
  hành sâu (widget, NFC, background service phức tạp) — ngoài phạm vi nhu cầu
  hiện tại.

### Desktop: Electron

Đóng gói Chromium + Node.js runtime, chạy `sendlove_web` trong `BrowserWindow`.
Tái sử dụng 100% code web, kể cả `signInWithPopup` (chạy được trong
BrowserWindow thật, khác với WebView hạn chế trên Android). Dùng
`electron-builder` để đóng gói installer Windows/Mac/Linux.

Lựa chọn bị loại:
- **Tauri**: cũng tái dùng gần như 100% code frontend (chạy trong WebView hệ
  điều hành), binary nhẹ hơn nhiều (vài MB thay vì hàng trăm MB), nhưng cần
  cài thêm Rust toolchain để build, hành vi có thể khác nhau nhẹ giữa
  WebView2 (Windows) và WebKit (Mac/Linux), và gọi native API phải viết
  command bằng Rust. Đáng cân nhắc lại nếu sau này dung lượng/hiệu năng
  Electron trở thành vấn đề thực sự.

## Tổng kết chiến lược

| Nền tảng | Công nghệ | Tái sử dụng code | Việc cần làm thêm |
|---|---|---|---|
| Android | Capacitor | ~100% `sendlove_web` | Plugin Google Sign-In native, config API URL |
| iOS | Capacitor | ~100% `sendlove_web` | Giống Android + cần Mac/Xcode + Apple Developer account |
| Desktop | Electron | 100% `sendlove_web` | Đóng gói bằng electron-builder |
| Backend | Không đổi | — | Không cần sửa |

Nguyên tắc chung: một codebase web duy nhất (`sendlove_web`) là nguồn UI/logic
cho cả 4 nền tảng (web, desktop, Android, iOS). Chỉ viết thêm phần cầu nối
native (auth, push notification) riêng cho từng nền tảng khi cần.
