# Dữ liệu thô Figma — section `sendlove-web`

**Trạng thái: ĐẦY ĐỦ.** Ghép từ 2 lượt gọi `use_figma` (read-only) khác nhau
trong cùng phiên làm việc (2026-09-03) — không tốn thêm lượt nào để hoàn
thiện, vì phần "thiếu" ở lượt gọi thứ hai thực ra đã có sẵn ở lượt gọi thứ
nhất, chỉ cần ghép lại.

## File

| File | Nội dung | Nguồn |
|---|---|---|
| `login-signup.json` | 2 biến thể màn đăng nhập, đầy đủ (effects, gradient, font thật) | lượt gọi 2 |
| `maindashboard-share-page.json` | Dashboard có box — ghép `appbar` (lượt 2) + `box_list` (lượt 1, chi tiết nhất) | lượt 1 + 2 |
| `maindashboard-share-page-empty.json` | Dashboard rỗng, đầy đủ | lượt 1 |
| `new-box.json` | Popup ghép hộp mới, đầy đủ | lượt 1 |
| `account-popover.json` | Popover tài khoản (Setting/Theme/Logout), đầy đủ | lượt 1 |
| `box-list-detailed.json` | Riêng `box-list` với chi tiết nhất (dash pattern add-box-card, cả 2 nhánh online/offline, path icon mailbox đầy đủ) — dùng file này làm nguồn chính cho box-card, KHÔNG dùng bản box-list lồng trong `new-box.json`/`account-popover.json`/`maindashboard-share-page-empty.json` (những bản đó nông hơn, thiếu dash pattern) | lượt 2 |
| `sendlove-web-section.RAW.txt` | Text thô gốc của lượt gọi 2 (6 frame, bị cắt ở ~20KB giữa chừng frame thứ 3) — giữ lại làm nguồn tham chiếu, các file `.json` ở trên đã trích xuất phần dùng được | lượt 2 |

**"Lượt 1"** = lượt gọi trước đó trong phiên, dump 5 entry (`login signup` ×2,
`maindashboard-share-page-empty`, `new-box`, `account-popover`) bằng schema
rút gọn (khoá 1 ký tự: `t,n,b,al,r,sw,sc,bg,k`), dùng để dựng
Dashboard.jsx/Login.jsx/PairBox.jsx lần đầu.

**"Lượt 2"** = lượt gọi "toàn bộ section" theo yêu cầu người dùng, dump 6
frame bằng schema đầy đủ (`type,name,x,y,w,h,layout{...},stroke{weight,color,
dash},fill,effects,characters,fontSize,fontStyle,fontFamily,lineHeight,
letterSpacing,sizing,...`) — **bị cắt ở ~20KB** vì schema đầy đủ nặng hơn
nhiều lần so với schema rút gọn, và `login signup` là frame nặng node nhất
trong section (ảnh nền + blur + inner-shadow + 2 icon instance × 2 biến thể).

## Lưu ý khi dùng dữ liệu này

- `fill:"#787878"` trên chính khung `appbar` (thấy ở cả 2 lượt gọi, cả frame
  có box lẫn rỗng) — rất có thể là màu debug/thừa còn sót lại trong file
  Figma nguồn, KHÔNG nên chép lại làm nền xám cho appbar trong code. Mọi
  appbar khác trong file (màn sender, receiver) đều trong suốt.
- Logo "SendloveBox" ở `login-signup.json` dùng font **Dancing Script**
  40/Bold, không phải Quicksand — ngoại lệ có chủ đích, chỉ riêng logo.
- `Frame 41` (hàng trạng thái trong box-card) có **2 nhánh nội dung khác
  nhau** tuỳ online/offline — xem `box-list-detailed.json` để thấy cả hai:
  online = chấm xanh + "online" + icon pin + "%"; offline = chấm xám +
  "offline" + text "Last sync Xhours ago…" (không có pin). FE hiện tại
  (commit `7bcc0c5`) chỉ dựng nhánh online, mặc định cứng — xem
  `sendlove-api-con-thieu.md`.
- Field "Box's display name" trong `new-box.json` dùng dấu nháy đơn kiểu
  Unicode `'` (U+2019), không phải `'` ASCII thường — giữ nguyên khi copy
  chuỗi, đừng tự sửa thành ASCII.

## Cách gọi lại nếu cần phần khác của file Figma (không phải section này)

`fileKey: 'NFwxXe0wzmi4wL8OPetr4w'`. Muốn tránh bị cắt 20KB: gọi riêng từng
frame một lượt, hoặc dùng schema rút gọn (khoá 1 ký tự) thay vì đầy đủ khi
chỉ cần vị trí/màu/layout cơ bản.
