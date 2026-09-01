# Where design decisions live

Lý do thiết kế của dự án này nằm **trong repo**, ở `sendlove_firmware/MEMORY.md`. **Bắt đầu từ
đây** trước khi đổi hành vi code.

> MCP server `om` (vault `P:\my-vault`) đã bị gỡ ngày 2026-09-01 — trước đó nó lỗi
> `search failed: qmd launcher exited`. Mọi tham chiếu tới `search`/`expand`/`recall`/`health`
> trong tài liệu cũ không còn hiệu lực.

Tra cứu trước khi đổi:
- **Định dạng lưu trữ NAND**: magic header SLBX/VJPG/VIMG, layout offset frame, cách phân biệt
  JPEG payload vs Raw RGB565 (xem `lib/NandStorage/`, `lib/Storage/NandStorageProvider.cpp`).
- **Cơ chế chia sẻ SPI bus**: `spiMutex` dùng chung giữa TFT (CS-less) và NAND flash, ai đang
  giữ mutex kiểu gì (recursive), vì sao (xem `DisplayDriver.cpp`, `NandStorage.cpp`).
- **Schema JSON tin nhắn Firebase**: các biến thể tên key (`bin_url`/`binUrl`/...), cách xử lý
  `timestamp`, giới hạn kích thước payload (xem `lib/NetworkManager/NetworkManager.cpp`).

Nếu `MEMORY.md` và code hiện tại mâu thuẫn, `MEMORY.md` giữ phần *lý do* — đối chiếu trước khi
đổi hành vi, và sửa lại `MEMORY.md` khi phát hiện nó ghi sai (đánh dấu chỗ sai, đừng xoá lịch sử).

**Ghi lại kết quả tra cứu** vào bất cứ thứ gì viết ra trước khi code: plan, note thiết kế, hoặc
tóm tắt trả lời người dùng. Nêu quyết định đã tra được, cái gì phản đối hướng đang định làm, và
nói rõ "không có gì được ghi nhận" khi không tìm thấy — đó là một phát hiện, không phải chỗ bỏ qua.

## Recording what you learn

Ghi vào `sendlove_firmware/MEMORY.md`:

- **Thay đổi vừa làm** — sửa gì, quyết định gì, phương án nào đã bị loại và vì sao, xác minh bằng
  cách nào (build/log/thử máy thật). Mục 7 của file là nơi kiểm kê trạng thái từng phần.
- **Ràng buộc do người dùng chốt** — ví dụ: giữ 8 kHz, không giảm đèn nền, decode phía client,
  chỉ làm local (không push/PR). Ghi kèm ngày, để phiên sau không mở lại tranh luận đã khép.
- **Gotcha tốn thời gian** — quirk của ESP32-C3/FreeRTOS/LovyanGFX, hành vi thật của HTTP chunked,
  cách `writeChunk`/`closeAppend` tính offset...

Bài học không riêng của project này (thói quen làm việc, lỗi công cụ) thì ghi vào memory của
Claude Code ở `C:\Users\phamp\.claude\projects\P--coddd-sendove-box\memory\`, đừng nhét vào repo.
