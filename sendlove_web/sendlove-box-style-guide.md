# SendLove Box — Style Guide

Khung phong cách thiết kế cho web app (và app native sau này). Đây là nguồn sự thật duy nhất. Mọi giá trị hardcode nằm ngoài file này đều là bug.

Phạm vi: **chỉ giao diện phần mềm**. Màn hình TFT 1.77" trên thiết bị là hệ riêng, không áp dụng file này.

---

## 0. Nguyên tắc dẫn dắt

**Web app là bao bì, không phải món quà.** Vai trò của nó là làm quá trình chuẩn bị quà đáng nhớ, rồi lùi ra nhường chỗ cho chiếc hộp.

Ba hệ quả:

1. **Trau chuốt nằm ở chuyển động và chi tiết nhỏ, không ở độ trang trí.** Transition mượt tạo cảm giác chăm chút mạnh hơn gradient hay hoa văn.
2. **Ít màn hình nhưng mỗi màn hình hoàn thiện.** App chỉ có 4–6 màn hình — đủ ngân sách để polish sâu từng cái.
3. **Mô hình sandwich cảm xúc.** Cảm xúc ở đầu (landing) và cuối (xác nhận đã gửi); ở giữa (luồng upload) là công cụ thuần tuý, tối giản triệt để.

**Phong cách chốt: Warm Minimalism + Motion-led delight.**

---

## 1. Layout

| Thuộc tính | Giá trị | Lý do |
|---|---|---|
| Cấu trúc | Một cột, căn giữa | Port sang native app không phải thiết kế lại |
| Chiều rộng nội dung tối đa | 480px | Giữ tỷ lệ mobile-first ngay cả trên desktop |
| Padding ngang (mobile) | 20px | |
| Padding ngang (desktop) | 24px | |
| Hành động chính mỗi màn hình | Tối đa 1 | Chống rối, dẫn hướng rõ |

Không dùng layout 2–3 cột rộng. Không dùng sidebar.

---

## 2. Typography

### Fonts

| Font | Vai trò | Weight |
|---|---|---|
| Dancing Script | Display duy nhất — landing, màn hình xác nhận gửi thành công | 700 |
| Quicksand | Toàn bộ phần còn lại: heading, body, label, số liệu | 400, 500, 600, 700 |

**Quy tắc khoá cứng:** Dancing Script CHỈ tồn tại ở bậc `display`, và chỉ xuất hiện ở landing + màn hình xác nhận. Tuyệt đối không dùng trong luồng upload, form, nút bấm, caption.

**Số liệu:** Quicksand + `font-variant-numeric: tabular-nums`. Không dùng monospace.

Import (bắt buộc có subset vietnamese):

```
https://fonts.googleapis.com/css2?family=Dancing+Script:wght@700&family=Quicksand:wght@400;500;600;700&display=swap&subset=vietnamese
```

### Type scale

| Token | Vai trò | Mobile | Desktop | Weight | Line-height |
|---|---|---|---|---|---|
| `display` | Landing, xác nhận gửi (Dancing Script) | 40 | 56 | 700 | 1.15 |
| `numeric-hero` | % upload, số liệu điểm nhấn | 30 | 40 | 700 | 1.2 |
| `title` | Tiêu đề màn hình | 22 | 28 | 700 | 1.2 |
| `heading` | Tiêu đề mục / bước | 17 | 20 | 600 | 1.3 |
| `body` | Nội dung chính, mô tả | 15 | 16 | 400 | 1.6 |
| `label` | Nút bấm, label form, tên file | 14 | 15 | 500–600 | 1.4 |
| `caption` | Dung lượng, thời lượng, hint, lỗi | 12 | 13 | 400 | 1.4 |

### Ràng buộc

1. **Sàn 12px.** Nếu buộc nhỏ hơn, tăng weight lên 500 bù độ mảnh của Quicksand.
2. **Line-height body tối thiểu 1.5**, khuyến nghị 1.6 — tiếng Việt có dấu trên và dưới ký tự.
3. **Chỉ weight 400, 500, 600, 700.** Không dùng 300.
4. **Không tạo bậc mới.** Cần size lạ thì chọn bậc gần nhất.
5. Phân cấp heading vs body dựa vào **weight**, không chỉ size.

### Kiểm tra tiếng Việt (bắt buộc trước ship)

```
Kỷ niệm — Yêu thương — Đầu tiên — Những ngày ấy
Chúng mình — Hạnh phúc — Đã gửi — Đang tải lên
ĐĂNG KÝ — Ừm — Ơn giời — Tuyệt vời
```

Dancing Script rủi ro lỗi dấu cao hơn. Nếu lỗi, thay bằng font script khác có Vietnamese subset đầy đủ.

---

## 3. Color

### Phân bổ diện tích

| Nhóm | % màn hình |
|---|---|
| Neutral + Surface | 85–90% |
| Brand (Rose + Caramel) | 5–10% |
| Semantic | 1–3% |

Màu brand vượt 20% màn hình = lạm dụng.

### Ramp: Rose (accent chính)

| Stop | Hex | Gốc |
|---|---|---|
| `rose-50` | `#FDF0EF` | |
| `rose-100` | `#FAD6D3` | Misty Rose |
| `rose-200` | `#F7BCBE` | |
| `rose-300` | `#F4A3AF` | |
| `rose-400` | `#F28AA1` | Salmon Pink |
| `rose-500` | `#E0758E` | |
| `rose-600` | `#C55A73` | |
| `rose-700` | `#9E3A52` | |
| `rose-800` | `#7A2A3D` | |
| `rose-900` | `#4F1A27` | |

### Ramp: Caramel (brand ấm)

| Stop | Hex | Gốc |
|---|---|---|
| `caramel-50` | `#FDF8F3` | |
| `caramel-100` | `#F6DFB3` | Wheat |
| `caramel-200` | `#EFCB93` | |
| `caramel-300` | `#E7AE75` | Fawn |
| `caramel-400` | `#D4915A` | |
| `caramel-500` | `#B87548` | |
| `caramel-600` | `#9E6244` | |
| `caramel-700` | `#83513E` | Bole |
| `caramel-800` | `#603A2C` | |
| `caramel-900` | `#3D2A20` | |

### Ramp: Neutral (xám ám nâu)

Bắt buộc xám ấm. Không dùng xám thuần hay xám ám xanh — sẽ lệch tông với brand chocolate.

| Stop | Hex |
|---|---|
| `neutral-0` | `#FFFFFF` |
| `neutral-25` | `#FDF8F3` |
| `neutral-50` | `#F7F1EA` |
| `neutral-100` | `#EAE0D6` |
| `neutral-200` | `#D9C9BB` |
| `neutral-300` | `#C4B5A9` |
| `neutral-400` | `#A89689` |
| `neutral-500` | `#8A7767` |
| `neutral-600` | `#7A6558` |
| `neutral-700` | `#5C4A3E` |
| `neutral-800` | `#3D2A20` |
| `neutral-900` | `#241812` |

### Semantic

Bắt buộc tách khỏi brand. Không dùng Salmon Pink cho lỗi.

| Vai trò | fill | bg | text |
|---|---|---|---|
| success | `#4A7C59` | `#E3F0E7` | `#2F5C3D` |
| error | `#C4443C` | `#FBE6E4` | `#8E2F29` |
| warning | `#D89B3C` | `#FBF0DC` | `#8A5F16` |

### Purpose tokens — LIGHT

Component chỉ được tham chiếu lớp này, không tham chiếu trực tiếp ramp stop.

**Text**

| Token | Giá trị |
|---|---|
| `--text-primary` | `#3D2A20` (caramel-900) |
| `--text-secondary` | `#7A6558` (neutral-600) |
| `--text-muted` | `#A89689` (neutral-400) |
| `--text-disabled` | `#C4B5A9` (neutral-300) |
| `--text-accent` | `#9E3A52` (rose-700) |
| `--text-brand` | `#83513E` (caramel-700) |

**Surface**

| Token | Giá trị | Dùng cho |
|---|---|---|
| `--surface-0` | `#FDF8F3` | Nền trang toàn app |
| `--surface-1` | `#FFFFFF` | Card |
| `--surface-2` | `#FFFFFF` | Panel, bottom sheet |
| `--surface-3` | `#FFFFFF` | Popover, modal |
| `--surface-warm` | `#F6DFB3` | Vùng nhấn ấm, empty state |

**Border**

| Token | Giá trị |
|---|---|
| `--border` | `#EAE0D6` (neutral-100) |
| `--border-strong` | `#D9C9BB` (neutral-200) |
| `--border-accent` | `#F4A3AF` (rose-300) |

**Fill (bề mặt điều khiển)**

| Token | Giá trị | Chữ đi kèm |
|---|---|---|
| `--fill-accent` | `#F28AA1` | `--on-accent` = `#3D2A20` |
| `--fill-accent-hover` | `#E0758E` | `#3D2A20` |
| `--fill-brand` | `#83513E` | `#FDF8F3` |
| `--fill-brand-hover` | `#603A2C` | `#FDF8F3` |
| `--fill-disabled` | `#EAE0D6` | `--text-disabled` |

**Background tint (nền thông tin)**

| Token | Giá trị | Chữ đi kèm |
|---|---|---|
| `--bg-accent` | `#FAD6D3` | `#9E3A52` |
| `--bg-brand` | `#F6DFB3` | `#83513E` |

**Phân biệt fill và bg:** `fill-*` là màu đậm cho bề mặt điều khiển (nút, checkbox, toggle) — đi với chữ tương phản mạnh. `bg-*` là tint nhạt cho nền thông tin (badge, banner, card highlight) — đi với chữ cùng họ ở stop 700.

**Lưu ý ngược trực giác:** chữ trên nút hồng `#F28AA1` phải là nâu đen `#3D2A20` (≈7:1), không phải trắng (chỉ ≈2.4:1).

### Purpose tokens — DARK

Không đảo ngược cơ học. Palette thiên sáng nên phải định nghĩa lại.

| Token | Hex |
|---|---|
| `--surface-0` | `#1A1210` |
| `--surface-1` | `#241A16` |
| `--surface-2` | `#2D211C` |
| `--surface-3` | `#362822` |
| `--surface-warm` | `#3A2A1E` |
| `--text-primary` | `#F5E9DE` |
| `--text-secondary` | `#C4B0A0` |
| `--text-muted` | `#94806F` |
| `--text-disabled` | `#6B5A4D` |
| `--text-accent` | `#F7BCBE` |
| `--text-brand` | `#EFCB93` |
| `--border` | `#3D2E27` |
| `--border-strong` | `#4F3C33` |
| `--border-accent` | `#7A2A3D` |
| `--fill-accent` | `#F28AA1` (giữ nguyên) |
| `--on-accent` | `#3D2A20` |
| `--fill-brand` | `#B87548` (sáng hơn light mode — Bole biến mất trên nền tối) |
| `--bg-accent` | `#5A2333` |
| `--bg-brand` | `#4A3524` |

Semantic dark: fill giữ nguyên; bg đổi sang `#1F3527` / `#3D1E1B` / `#3A2C14`; text đổi sang `#8FCBA3` / `#F0A09A` / `#EBC77E`.

### Ràng buộc màu

1. **Tương phản tối thiểu 4.5:1** chữ thường, 3:1 chữ ≥18px. Kiểm tra cả hover và disabled.
2. **Không ghép hai stop cùng vùng sáng.** Chữ trên tint phải dùng stop 700+ của cùng ramp.
3. **Tối đa 1 điểm nhấn màu bão hoà mỗi màn hình.**
4. **Tối đa 3 màu brand xuất hiện đồng thời.**
5. **Màu không được là kênh thông tin duy nhất** — trạng thái phải kèm icon hoặc chữ.
6. **Đặt tên token theo vai trò**, không theo màu.
7. **`caramel-300` (Fawn) chỉ trang trí** — icon phụ, divider, viền. Không đặt chữ lên nó.

### Gradient

**Được phép:** landing/onboarding, nền một card điểm nhấn duy nhất, overlay trên ảnh/video preview để chữ nổi lên.

**Không dùng cho:** nền toàn app, nút bấm, bất kỳ nền nào chứa text dài.

**Nếu dùng:** cùng họ màu, chênh lệch nhỏ. An toàn: `#FAD6D3 → #F6DFB3`. Tránh: `rose-400 → caramel-700`.

---

## 4. Spacing

Thang 4px. Chỉ dùng các bậc sau, không dùng giá trị lẻ.

| Token | px | Dùng cho |
|---|---|---|
| `space-1` | 4 | Khoảng cách icon–text |
| `space-2` | 8 | Trong component (padding nút nhỏ) |
| `space-3` | 12 | Giữa các dòng liên quan |
| `space-4` | 16 | Padding card, gap list item |
| `space-5` | 20 | Padding ngang màn hình (mobile) |
| `space-6` | 24 | Giữa các nhóm nội dung |
| `space-8` | 32 | Giữa các section |
| `space-10` | 40 | Trên/dưới hành động chính |
| `space-12` | 48 | Khoảng thở lớn (landing, xác nhận) |

**Nguyên tắc:** khoảng cách giữa các nhóm phải lớn hơn khoảng cách trong nhóm. Nếu hai phần tử cách nhau bằng nhau, mắt không phân biệt được nhóm.

---

## 5. Corner radius

| Token | px | Dùng cho |
|---|---|---|
| `radius-sm` | 8 | Nút, input, checkbox, badge |
| `radius-md` | 12 | Card, panel |
| `radius-lg` | 20 | Preview ảnh/video, modal |
| `radius-full` | 999 | Avatar, progress bar, pill tag |

Bo tròn vừa phải hợp với Quicksand. Không dùng radius lớn hơn 20px cho card — sẽ trượt sang cảm giác "trẻ con".

---

## 6. Elevation & Border

**Không dùng shadow** cho card và panel. Phân lớp bằng chênh lệch nền + hairline.

| Lớp | Cách thể hiện |
|---|---|
| Nền trang | `--surface-0` `#FDF8F3` |
| Card | `--surface-1` `#FFFFFF` + `0.5px solid var(--border)` |
| Bottom sheet / Panel | `--surface-2` + border-top |
| Popover / Modal | `--surface-3` + shadow nhẹ `0 8px 24px rgba(61,42,32,0.12)` |

Shadow **chỉ** dùng cho popover và modal. Tối đa 2 lớp nổi cùng lúc.

Viền mặc định `0.5px`, không dùng 1px trừ khi cần nhấn mạnh. Dùng viền tiết chế — nhiều viền là nguyên nhân chính gây rối mắt.

---

## 7. Motion

**Đây là nơi đầu tư chính của phong cách này.**

| Loại | Duration | Easing |
|---|---|---|
| Micro (hover, press, toggle) | 150ms | `ease-out` |
| Chuyển trạng thái component | 250ms | `cubic-bezier(0.4, 0, 0.2, 1)` |
| Chuyển màn hình | 300ms | `cubic-bezier(0.4, 0, 0.2, 1)` |
| Feedback thành công | 400ms | `ease-out` |

**Nguyên tắc:**

- Không dùng bounce/spring — dễ thành "cute quá đà", lệch với tông tối giản.
- Mọi chuyển động phải có mục đích: cho biết cái gì vừa thay đổi, hoặc cái gì đến từ đâu.
- Tôn trọng `prefers-reduced-motion: reduce` — tắt toàn bộ transition không thiết yếu.
- Progress bar không bao giờ đứng im. Nếu không biết % thật, dùng indeterminate animation.

---

## 8. Iconography

| Thuộc tính | Giá trị |
|---|---|
| Kiểu | Outline (không filled) |
| Stroke width | 1.75px |
| Đầu nét | Bo tròn (round cap, round join) |
| Kích thước | 16 / 20 / 24px — chỉ 3 bậc |
| Màu mặc định | `--text-secondary` |

Filled icon quá nặng so với palette nhạt và font bo tròn. Icon chỉ dùng filled khi biểu thị trạng thái active.

---

## 9. Media

| Thuộc tính | Giá trị |
|---|---|
| Bo góc preview | `radius-lg` 20px |
| Tỷ lệ ưu tiên | 4:5 hoặc 1:1 |
| Placeholder khi loading | Nền `--surface-warm`, không dùng skeleton xám lạnh |
| Overlay trên media | `linear-gradient(transparent, rgba(61,42,32,0.6))` |

Tránh 16:9 — chiếm ít chiều cao trên mobile, lãng phí không gian.

---

## 10. Component conventions

### Button

| Loại | Nền | Chữ | Dùng cho |
|---|---|---|---|
| Primary | `--fill-accent` | `--on-accent` | Hành động chính duy nhất mỗi màn |
| Secondary | `--surface-1` + border | `--text-primary` | Hành động phụ |
| Ghost | trong suốt | `--text-secondary` | Huỷ, quay lại |
| Destructive | `error.fill` | `#FFFFFF` | Xoá, huỷ gửi |

Chiều cao: 44px (mobile) / 40px (desktop). Radius `radius-sm`. Font `label` weight 600.

### Input

Nền `--surface-1`, viền `--border`, focus đổi viền sang `--border-accent` + ring `0 0 0 3px rgba(242,138,161,0.2)`. Chiều cao 44px. Label luôn hiện phía trên, không dùng placeholder thay label.

### States bắt buộc thiết kế

Mọi component tương tác phải có đủ: `default`, `hover`, `focus-visible`, `active`, `disabled`, `loading`, `error`.

`focus-visible` bắt buộc có ring rõ ràng — không được `outline: none` mà không thay thế.

---

## 11. Ưu tiên polish

Xếp theo mức ảnh hưởng tới cảm nhận chất lượng:

1. **Trạng thái upload/xử lý** — người dùng nhìn lâu nhất. Phải phân biệt rõ từng giai đoạn (đang nén ≠ đang tải lên), có ước lượng thời gian, không bao giờ đứng im.
2. **Màn hình xác nhận đã gửi** — điểm cảm xúc cao nhất. Dancing Script, animation, palette được phép toả sáng ở đây.
3. **Empty state và error state** — với app xử lý file, lỗi xảy ra thường xuyên (file quá lớn, sai định dạng, mất mạng). Error state tử tế nâng cảm giác chất lượng rất nhiều.
4. **Drag & drop / chọn file** — tương tác đầu tiên, tạo ấn tượng ban đầu.
5. **Preview** — cho người gửi thấy trước quà sẽ trông thế nào.

---

## 12. Triển khai

### Figma

- Text Styles theo 7 token, tách `Mobile/` và `Desktop/`.
- Color Variables 2 lớp: lớp ramp (`rose/400`) và lớp purpose (`text/primary`) alias tới ramp.
- 2 mode `Light` / `Dark` — chỉ lớp purpose có 2 mode, lớp ramp giữ nguyên.
- Variables cho spacing và radius.
- Không tạo layer nào không gắn Style/Variable.
- Bật Vietnamese subset khi load Google Fonts.

### Coding agent

- Toàn bộ token trong một file theme duy nhất. Không hardcode hex/px trong component.
- Component chỉ tham chiếu purpose token (`--text-primary`, `--fill-accent`), không tham chiếu ramp stop (`--rose-400`).
- Dark mode override purpose token dưới `[data-theme="dark"]` và `@media (prefers-color-scheme: dark)`.
- `font-variant-numeric: tabular-nums` cho mọi element hiển thị số liệu, dung lượng, thời lượng, %.
- Viền mặc định `0.5px solid var(--border)`.
- Container chính: `max-width: 480px; margin: 0 auto;`.
- Hỗ trợ `prefers-reduced-motion`.
