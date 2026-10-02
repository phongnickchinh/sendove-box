# Chiến lược Multi-Platform: Desktop & Mobile (Android/iOS)

## Cập nhật 2026-10-02 — PWA trước, Capacitor để sau

**Quyết định:** bản điện thoại (Android lẫn iPhone) là PWA: chính trang web,
cài lên màn hình chính, mở toàn màn hình. Chưa làm APK / App Store.

**Lý do:** app chỉ cần chọn file, ghi âm, canvas, Web Audio và gọi API — trình
duyệt điện thoại có đủ. Giao diện vốn thiết kế cho điện thoại (cột 430, xem
`sendlove_web/sendlove-box-style-guide.md` §1) nên không phải vẽ lại. Hộp cài
Wi-Fi qua captive portal riêng, không cần Bluetooth hay tính năng native.
App Store tốn máy Mac + 99 USD/năm + kiểm duyệt (Apple hay từ chối app chỉ là
web bọc lại).

**Đã làm (nhánh `fe-ui-phase-2`):**

| Việc | File |
|---|---|
| Manifest + icon 192/512/maskable + apple-touch-icon | `sendlove_web/public/` |
| Thẻ meta iPhone, `viewport-fit=cover`, `lang="vi"`, `theme-color` theo giao diện sáng/tối | `index.html`, `ThemeContext.jsx` |
| Service worker: chỉ trả trang "Mất kết nối" khi mở app lúc không có mạng, không cache gì khác | `public/sw.js`, `public/offline.html` |
| Gợi ý cài trên màn Login (iPhone: chỉ nút Chia sẻ; Android: nút cài) | `components/InstallHint.jsx` |
| Cảnh báo khi mở trong trình duyệt của Zalo/Facebook (Google chặn đăng nhập ở đó) | `components/InstallHint.jsx` |
| Ô nhập 16px trên iOS để Safari không tự phóng to trang | `styles/sendlove.css` |

**Còn phải làm bằng tay:**

1. **Tên miền đăng nhập.** `VITE_FIREBASE_AUTH_DOMAIN` đang là
   `iot-app-839a2.firebaseapp.com`. Trên Safari/iPhone, khi popup bị chặn app
   rơi sang đăng nhập redirect, và redirect chỉ chạy khi auth domain trùng tên
   miền host web. Cách sửa: đổi biến này thành `iot-app-839a2.web.app`, **đồng
   thời** thêm `https://iot-app-839a2.web.app/__/auth/handler` vào "Authorized
   redirect URIs" của OAuth client (Google Cloud Console → Credentials) và vào
   cấu hình app Facebook. Đổi biến mà chưa thêm URI thì đăng nhập hỏng hẳn
   (`redirect_uri_mismatch`), nên hai việc phải làm cùng lúc.
2. **Thử trên máy thật** (không giả lập được):
   - iPhone Safari: gửi video, ảnh, tin thoại. Chỗ nghi ngờ nhất là
     `encodeVideoToBin` (`utils/mediaEncoder.js`): nó chờ sự kiện `loadeddata`
     của một thẻ `<video>` không gắn vào trang, mà iOS thường không tải dữ liệu
     video cho tới khi phát → có thể treo ở 0%. Nếu treo: thử thêm
     `video.preload = 'auto'; video.load();` hoặc chuyển sang `loadedmetadata`.
   - iPhone: Thêm vào Màn hình chính → mở từ icon → đăng nhập → tắt mở lại vẫn
     còn đăng nhập.
   - Android Chrome: nút "Cài lên màn hình chính" hiện ở màn Login, cài xong mở
     toàn màn hình; nút Back đóng được trang.
3. **Deploy** lên Firebase Hosting (PWA cần HTTPS — `localhost` chỉ để thử).

**Đính chính bản chiến lược bên dưới** (viết trước khi đối chiếu code):

- "Không phụ thuộc CORS ở phía native app" — sai với Capacitor. App Capacitor
  vẫn là WebView, origin `https://localhost`. Hiện chạy được vì backend để
  `cors({ origin: true })` và bucket để `*`; khi siết CORS theo TODO ở
  `sendlove_backend/src/index.ts` phải thêm origin này.
- `@capacitor/google-auth` không phải package chính thức. Nếu làm Capacitor,
  dùng `@capacitor-firebase/authentication` (kiểm lại trên npm trước khi cài).
- "Trỏ `VITE_API_URL` sang production" — đã xong, `.env.production` lo việc này.
- Web **không** dùng Firebase SDK cho Realtime Database/Storage nữa; mọi thứ đi
  qua backend, media dùng signed URL (`src/config/firebase.js`).

**Khi nào quay lại Capacitor / App Store:** khi cần có mặt trên store để người
mua hộp tìm thấy, hoặc cần tính năng native (ví dụ cài hộp qua Bluetooth). Các
bước khi đó: cài Android Studio → `cap add android` trong `sendlove_web` → đăng
nhập Google native + `signInWithCredential` (popup/redirect đều bị chặn trong
WebView) → quyền `RECORD_AUDIO`, nút Back, icon/splash → keystore và SHA-1 bản
release trong Firebase Console.

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
