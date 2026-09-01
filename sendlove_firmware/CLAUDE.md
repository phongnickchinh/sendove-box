# Where design decisions live

Lý do thiết kế của dự án này được lưu ngoài repo, truy cập qua `om` MCP server (vault tại
`P:\my-vault`).

- **`search`** tra cứu quyết định đã ghi: vì sao chọn cách này, cái gì đã bị từ chối, ràng buộc
  nào đặt ra giới hạn. **Bắt đầu từ đây.**
- **`expand`** xem link/backlink của 1 note đã biết — rẻ hơn tìm lại từ đầu.
- **`recall`** trả về bài học ngắn, đã được xác thực, gắn với project này. Rỗng cho đến khi có
  phiên nào ghi vào — rỗng lúc đầu **không** có nghĩa là chưa từng ghi nhận.
- **`health`** khi thứ lẽ ra phải có mà không tìm thấy.

Tra cứu trước khi đổi:
- **Định dạng lưu trữ NAND**: magic header SLBX/VJPG/VIMG, layout offset frame, cách phân biệt
  JPEG payload vs Raw RGB565 (xem `lib/NandStorage/`, `lib/Storage/NandStorageProvider.cpp`).
- **Cơ chế chia sẻ SPI bus**: `spiMutex` dùng chung giữa TFT (CS-less) và NAND flash, ai đang
  giữ mutex kiểu gì (recursive), vì sao (xem `DisplayDriver.cpp`, `NandStorage.cpp`).
- **Schema JSON tin nhắn Firebase**: các biến thể tên key (`bin_url`/`binUrl`/...), cách xử lý
  `timestamp`, giới hạn kích thước payload (xem `lib/NetworkManager/NetworkManager.cpp`).

Nếu record và code hiện tại mâu thuẫn, record giữ phần *lý do* — đối chiếu trước khi đổi hành vi.

**Ghi lại kết quả tra cứu** vào bất cứ thứ gì viết ra trước khi code: plan, note thiết kế, hoặc
tóm tắt trả lời người dùng. Nêu quyết định đã tra được, cái gì phản đối hướng đang định làm, và
nói rõ "không có gì được ghi nhận" khi record trống — đó là một phát hiện, không phải chỗ bỏ qua.

## Recording what you learn

Hai công cụ, dễ chọn nhầm. Phép thử: điều này có giúp ích cho một project **khác** không?

- **`remember`** lưu bài học bền vững: một ràng buộc vừa phát hiện, một gotcha tốn thời gian, một
  quy tắc tổng quát hóa được. Đặt `confidence` (`verified`/`inferred`/`unverified`) trung thực.
  Việc riêng của firmware này (ví dụ: layout offset SLBX) dùng `scope: "project"`,
  `projects: ["sendlove_firmware"]`. Ưu tiên `scope: "platform"` trước `"general"` — quirk của
  ESP32-C3/FreeRTOS/LovyanGFX là platform-level dù tốn công phát hiện thế nào.
- **`record_work`** ghi lại việc vừa xảy ra ở đây: thay đổi, quyết định và phương án đã bị loại,
  cái gì đã học, cái gì còn mở, đã xác minh bằng cách nào.

Một hạn chế của dependency (ví dụ: quirk của `WiFiClientSecure` khi tái dùng object trên ESP32)
là `remember`. "Đã sửa 7 mục review firmware, đây là cái giá phải trả" là `record_work`. Làm cả
hai khi cả hai đều đúng.

> Không dẫn đầu bằng `recall` — nó trả về *ký ức*, và kho này rỗng ở phiên đầu tiên của project
> **theo thiết kế**.
