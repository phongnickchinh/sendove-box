# FE Structure Design — SendLove Box (Web)

> File này ghi lại các quyết định **cấu trúc FE** (screen inventory, routing, luồng chuyển màn) đã chốt trong phiên làm việc thiết kế FE. Mục đích: để BE agent đọc và điều chỉnh backend (schema, API) cho khớp với những gì FE sẽ cần.
>
> **Phạm vi:** Đây KHÔNG phải là mockup/thiết kế thị giác. Đây là tài liệu cấu trúc — màn hình nào tồn tại, route nào, cần data gì. Chi tiết style/token thị giác xem `sendlove_web/sendlove-box-style-guide.md` (nguồn thiết kế mới, chính thức). File `sendlove_web/design-system-rules.md` hiện là **tài liệu cũ** (bóc tách ngược từ FE tạm thời), sẽ được viết lại toàn bộ sau — không dùng làm tham chiếu cho công việc mới.

---

## 1. Bối cảnh hệ thống (nhắc lại, không đổi)

- Model đồng bộ: Box **không** nhận tin nhắn ngay lập tức. Box ngủ, thức dậy mỗi 5 phút (timer) hoặc khi có chạm (GPIO interrupt) để đồng bộ với Firebase. → Bất kỳ copy nào trên FE nói về việc tin đã "đến tay người nhận" đều phải phản ánh đúng độ trễ này, không được khẳng định real-time.
- Kiến trúc "Thin Client": encode media diễn ra ở browser, box chỉ tải & phát.
- `BoxConfig` hiện tại (backend, `sendlove_backend/src/types/box.types.ts:21-29`):
  ```ts
  export interface BoxConfig {
    alarm_list: Record<string, Alarm>;
    wifi_config?: { ssid: string; pwd: string };
  }
  ```
  Không có field nào cho LED / độ sáng / âm lượng thông báo.
- Firmware: `LEDState` enum đã có (OFF/BREATHING/SOLID/BLINK_FAST) nhưng hiệu ứng breathing được comment rõ là "Phase 2 (chưa triển khai)". Độ sáng màn hình (backlight) hiện hardcode theo giờ trong ngày, chưa cấu hình được từ user. Box không có buzzer riêng — chỉ có loa MAX98357A, nên "âm thanh thông báo" = âm lượng phát, không phải chime riêng.

---

## 2. Route table hiện tại (đối chiếu code, `sendlove_web/src/App.jsx`)

```
/                              Login (public)
/dashboard                     Dashboard          [AuthRoute]
/pair                          PairBox            [AuthRoute]
/box/:boxId/sender             SenderUI           [AuthRoute]
/box/:boxId/receiver           ReceiverUI         [AuthRoute]
*                               → redirect "/"
```

### Vấn đề đã phát hiện

1. **Route đứt gãy**: `SenderUI.jsx` (dòng 122 và 158) điều hướng tới `/box/:boxId/sender/dashboard` nhưng route này **không tồn tại** trong `App.jsx`. Component đích (`SenderDashboard.jsx` — màn "Lịch sử tin nhắn" của sender) đã viết xong, đủ chức năng, chỉ thiếu khai báo route.
2. **2 trang mồ côi**:
   - `src/pages/Home.jsx` — không được import/route ở bất kỳ đâu trong `src`. → **Xoá**.
   - `src/pages/ReceiverDashboard.jsx` — không được route, chứa 1 form báo thức (`<input type="time" defaultValue="07:30" />`) trộn lẫn với status card trùng lặp với `ReceiverUI.jsx`. → **Tách** thành 2 màn riêng (mục 3.9, 3.10 bên dưới), sau đó xoá file gốc.

---

## 3. Screen inventory đầy đủ (sau khi áp dụng các quyết định)

Ký hiệu: 🟢 đã có sẵn & đang route đúng · 🟡 đã có code, cần nối lại route · 🔴 chưa tồn tại, cần tạo mới · ⚙️ cần thay đổi backend

| # | Màn hình | Route | Trạng thái | Data cần từ BE |
|---|---|---|---|---|
| 3.1 | **Login** (landing, "emotional sandwich" mở đầu) | `/` | 🟢 | Auth hiện có |
| 3.2 | **Dashboard** (hub sau đăng nhập) | `/dashboard` | 🟢 | Danh sách box đã pair |
| 3.3 | **Pair Box** | `/pair` | 🟢 | API pairing hiện có |
| 3.4 | **Sender — Chọn loại nội dung** (step 1 của SenderUI) | `/box/:boxId/sender` | 🟢 | — |
| 3.5 | **Sender — Nhập nội dung** (step 2, cùng route, khác state) | `/box/:boxId/sender` | 🟢 | — |
| 3.6 | **Sender — Encode & Upload progress** (step 3) | `/box/:boxId/sender` | 🟢 | `uploadMessage` API hiện có |
| 3.7 | **Sender — Xác nhận đã gửi** (mới, tách khỏi step 3) | `/box/:boxId/sender/confirm` | 🔴 mới | Không cần API mới — dùng lại kết quả upload. Copy phải nói "đã lưu / đang chờ đồng bộ tới Box", KHÔNG nói "đã đến tay người nhận" (do model sync 5 phút) |
| 3.8 | **Sender — Lịch sử tin nhắn** (`SenderDashboard.jsx`) | `/box/:boxId/sender/dashboard` | 🟡 nối route | `getMessages` API đã có, không đổi |
| 3.9 | **Receiver — Trạng thái Box + tin nhận được** (`ReceiverUI.jsx`) | `/box/:boxId/receiver` | 🟢 | `getMessages`, box status — đã có |
| 3.10 | **Receiver — Cài đặt báo thức** (tách từ `ReceiverDashboard.jsx`) | `/box/:boxId/receiver/alarm` | 🔴 mới (refactor từ code cũ) | `alarm_list` — **đã có sẵn** trong `BoxConfig`, không cần đổi schema |
| 3.11 | **Receiver — Cấu hình Box** (ý tưởng mới, chưa có trong dự án trước đây) | `/box/:boxId/receiver/config` | 🔴 mới | ⚙️ **Cần field mới trong `BoxConfig`** — xem mục 4 |

**Đã xác nhận giữ nguyên, không đổi:** Send History (3.8) — giữ.

---

## 4. Việc cần làm ở Backend (⚙️ action items cho BE agent)

### 4.1 Bắt buộc để hỗ trợ màn "Cấu hình Box" (3.11)

Đây là **ý tưởng mới**, chưa từng được đặc tả rõ trong dự án trước đây. FE sẽ cần các trường cấu hình sau — đề xuất mở rộng `BoxConfig`:

```ts
export interface BoxConfig {
  alarm_list: Record<string, Alarm>;
  wifi_config?: { ssid: string; pwd: string };

  // Đề xuất mới cho màn "Cấu hình Box":
  led_state?: 'OFF' | 'BREATHING' | 'SOLID' | 'BLINK_FAST'; // map theo LEDState enum hiện có ở firmware
  display_brightness?: number;   // 0-100, hiện đang hardcode theo giờ trong firmware — cần chuyển sang user-configurable
  playback_volume?: number;      // 0-100 — thay cho "âm thanh thông báo" (box không có buzzer riêng, chỉ có loa MAX98357A)
}
```

**Ràng buộc cần BE + Firmware team biết trước khi FE build màn này:**
- `led_state: BREATHING` hiện **chưa hoạt động** ở firmware (comment "Phase 2 — chưa triển khai"). Nếu chọn expose option này trên FE, cần rõ ràng là "sắp có" hoặc ẩn option cho tới khi firmware xong.
- `display_brightness` hiện hardcode theo thời gian trong ngày ở `UIController` — cần firmware đọc field mới này từ `BoxConfig` thay vì tự tính theo giờ, hoặc merge logic (ví dụ: user set override, còn lại vẫn theo giờ).
- `playback_volume` cần map sang volume control thực tế của MAX98357A (I2S) — xác nhận với firmware team về range/scale phù hợp (0-100 tuyến tính hay theo dB).
- Cần thêm flag tương tự `a_flag` (đã có cho alarm) để báo cho ESP32 biết config vừa đổi cần đọc lại — ví dụ `config_flag` hoặc gộp vào flag hiện có, tuỳ BE quyết định.

### 4.2 Không cần đổi backend

- Route reconnect (3.8), xoá `Home.jsx`, tách `ReceiverDashboard.jsx` thành 3.10 — đều là refactor thuần FE, không động tới API/schema.
- Màn "Xác nhận đã gửi" (3.7) — tái dùng response của `uploadMessage`, không cần endpoint mới.
- Cài đặt báo thức (3.10) — dùng đúng `alarm_list` đã có, chỉ là tách UI, không đổi data shape.

---

## 5. Luồng chuyển màn (đề xuất)

```
Login (3.1)
  └─→ Dashboard (3.2)
        ├─→ Pair Box (3.3) ──→ Dashboard
        ├─→ [role: Sender] Sender flow
        │     3.4 Chọn loại nội dung
        │       └─→ 3.5 Nhập nội dung
        │             └─→ 3.6 Encode & Upload (progress)
        │                   └─→ 3.7 Xác nhận đã gửi (mới)
        │                         ├─→ 3.4 (gửi tiếp)
        │                         └─→ 3.8 Lịch sử tin nhắn
        │     3.4 ──(nút "Lịch sử tin nhắn")──→ 3.8
        │           3.8 ──(nút "+ Gửi tin mới")──→ 3.4
        │
        └─→ [role: Receiver] Receiver flow
              3.9 Trạng thái Box + tin nhận được
                ├─→ 3.10 Cài đặt báo thức (mới, tách từ ReceiverDashboard)
                └─→ 3.11 Cấu hình Box (mới)
```

---

## 6. Checklist thực thi FE (chưa làm, chờ xác nhận trước khi code)

- [ ] Thêm route `/box/:boxId/sender/dashboard` → `SenderDashboard` trong `App.jsx`
- [ ] Xoá `src/pages/Home.jsx`
- [ ] Tách `src/pages/ReceiverDashboard.jsx` → tạo `AlarmSettings` (3.10) + `BoxConfig` (3.11) screens, route riêng, xoá file gốc
- [ ] Tạo màn "Xác nhận đã gửi" (3.7) như 1 step/route riêng thay vì gộp vào `EncodingProgress`'s `done` phase
- [ ] Cập nhật style token toàn bộ theo `sendlove-box-style-guide.md` (việc riêng, không phụ thuộc các mục trên)

---

*Cập nhật lần cuối: phiên thiết kế cấu trúc FE — SendLove Box.*

---

## 7. Cập nhật sau UI phase (2026-09-03, nhánh `fe-apply-design`)

Tài liệu ở trên viết trước khi đối chiếu lại code. Ba mục đã lạc hậu:

### 7.1 Đính chính

1. **Mục 4.1 sai.** `BoxConfig` **đã có sẵn** `led_state`, `display_brightness`,
   `playback_volume`, và `BoxFlags` đã có `config_flag`
   (`sendlove_backend/src/types/box.types.ts`). Không cần đổi schema.
   Route `PUT /boxes/:boxId/config` cũng đã có, validator ở
   `validation.middleware.ts:121-125`.
2. **Alarm CRUD đã đủ**: `POST/GET/PATCH/DELETE /boxes/:boxId/alarms`
   (`routes/alarm.routes.ts`), chỉ role `receiver` gọi được.
   `DELETE /boxes/:boxId/unpair` cũng đã có.
3. **Captive portal đã có `/scan`** trả `{status, nets:[{ssid,rssi,lock}]}`
   (`NetworkManager.cpp:274-276`, `handleCaptiveScan()`), kèm client poll +
   dedup sẵn trong `captive_portal_html.h`. Ghi chép cũ nói chưa có là sai.
4. **Màn hình hộp là 240 × 240** (`config.h:17-18`), không phải 128×160.

### 7.2 Route table sau khi sửa

```
/                                     Login (public)
/dashboard                            Dashboard          [AuthRoute]
/pair                                 PairBox            [AuthRoute]
/box/:boxId/sender                    SenderUI
/box/:boxId/sender/dashboard          SenderDashboard    (route đứt gãy — đã nối)
/box/:boxId/receiver                  ReceiverUI
/box/:boxId/receiver/alarm            ReceiverAlarms     (mới)
/box/:boxId/receiver/config           ReceiverConfig     (mới, gồm huỷ ghép đôi)
/box/:boxId/receiver/theme            ThemePicker        (mới, chưa có backend)
/box/:boxId/receiver/theme/edit       ThemeEditor        (mới, chưa có backend)
/box/:boxId/receiver/theme/send       ThemeSend          (mới, chưa có backend)
*                                      → redirect "/"
```

Đã xoá: `Home.jsx`, `ReceiverDashboard.jsx`, `Navbar.jsx`, `App.css`.
`AuthRoute` không còn dựng khung chung — mọi trang tự dựng `<Screen>`, nên tham
số `bare` đã bỏ.

### 7.3 Còn nợ

- **Màn 3.7 "Xác nhận đã gửi"** (`/box/:boxId/sender/confirm`) chưa tách khỏi
  bước 3 của `SenderUI`.
- **Theme**: không route, không API ghi file trên `IStorageProvider`, không khoá
  NVS. UI đã dựng, chạy trên `src/theme/layout.js`.
- **Portal màn 17/18/22**: thiếu route đặt giờ (`settimeofday`), sửa báo thức từ
  portal, quay về Wi-Fi cũ (`loadBackupWiFi` có sẵn nhưng chưa ai gọi), factory
  reset (`clearAll()` có sẵn), chọn theme trên thẻ.
- **Portal chưa nạp lên hộp thật.** `pio run` SUCCESS, flash 74.0%, khối
  `<script>` giữ nguyên từng ký tự — nhưng luồng quét → chọn → lưu → lên mạng
  phải thử trên thiết bị trước khi merge.
