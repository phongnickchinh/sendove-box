# Đánh giá Codebase & Các rủi ro tiềm ẩn (Sendlove Box Firmware)

Dựa trên cấu trúc hoạt động của ESP32 với FreeRTOS hiện tại, tôi đã rà soát toàn bộ các thành phần chính (`main`, `NetworkManager`, `MediaPlayer`, `NandStorage`) và tổng hợp lại các rủi ro, điểm cần cải thiện, cũng như các đoạn code đang bị "over-engineering" hoặc vi phạm nguyên tắc KISS (Keep It Simple, Stupid).

---

## 1. Rủi ro Quản lý Bộ nhớ (Memory Management Risks)

> [!WARNING]
> **Rủi ro Crash/Tràn RAM do `http.getString()` trong `NetworkManager.cpp`**
> - **Hiện trạng:** Khi đồng bộ tin nhắn từ Firebase, code đang dùng `String payload = http.getString();` để tải toàn bộ chuỗi JSON phản hồi vào RAM, sau đó mới truyền vào `deserializeJson(doc, payload)`.
> - **Nguy cơ:** ESP32 có dung lượng RAM (Heap) liên tục (contiguous) rất hạn chế. Nếu danh sách tin nhắn trên Firebase dài ra (hoặc có chứa các URL rất dài), chuỗi JSON này có thể phình to lên vài chục KB. Việc cấp phát chuỗi này sẽ gây phân mảnh RAM nặng nề, hoặc thất bại (Out of Memory) dẫn đến Crash chip tương tự lỗi vừa rồi.
> - **Giải pháp KISS:** ArduinoJson hỗ trợ parse trực tiếp từ Stream mà không cần lưu chuỗi trung gian. Cần sửa thành: `deserializeJson(doc, http.getStream());`.

> [!TIP]
> **Bộ nhớ đệm 48KB của MediaPlayer (`_jpegBuffer`)**
> Cấp phát động 48KB bằng `malloc()` là rất lớn. Rất may là bạn đã cấp phát một lần và giữ nó suốt quá trình (chỉ `free()` khi `stop()`). Việc thêm Mutex mà chúng ta vừa làm đã cứu hệ thống khỏi lỗi Use-After-Free tại đây.

---

## 2. Rủi ro Quản lý Task & Xung đột (Concurrency & Task Risks)

> [!CAUTION]
> **Xung đột giữa OTA Update và Firebase Sync ngầm**
> - **Hiện trạng:** Trong `Task_MediaPlayer`, bạn đã có cơ chế ngưng vẽ màn hình khi OTA đang chạy (`appCtx.otaHandler.isUpdating()`). Rất tốt.
> - **Nguy cơ:** Trong khi đó, `NetworkManager::triggerFirebaseSync` lại **không** kiểm tra cờ OTA. Nếu quá trình đồng bộ nền (tải file .bin 40KB) diễn ra CÙNG LÚC với quá trình người dùng đẩy firmware OTA, hai tiến trình này sẽ tranh giành băng thông Wi-Fi, tranh giành CPU và có thể gây sụt áp hoặc Crash quá trình OTA.
> - **Giải pháp:** Cần thêm điều kiện `if (appCtx.otaHandler.isUpdating()) return;` vào đầu hàm `syncFirebaseWakeup`.

> [!WARNING]
> **Latent Bug: Wi-Fi Panic khi Light Sleep**
> - Mặc dù nguyên nhân gây Crash khi ngủ vừa qua là do SPI/RAM Race Condition (đã sửa), nhưng việc ép ESP32 vào `Light Sleep` bằng lệnh `esp_light_sleep_start()` mà **không tắt Wi-Fi** (`esp_wifi_stop()`) là một quả bom nổ chậm theo tài liệu của Espressif. Tùy thuộc vào Router Wi-Fi và tín hiệu Beacon, phần cứng Baseband có thể bị panic ngẫu nhiên khi thức dậy.
> - **Khuyến nghị:** Vẫn nên ngắt Wi-Fi (`WiFi.mode(WIFI_OFF)`) trước khi vào Sleep và `WiFi.mode(WIFI_STA)` khi thức dậy.

---

## 3. Các vị trí Over-Engineering & Vi phạm KISS

> [!NOTE]
> **1. Parse Slot Header thừa thãi**
> - Trong `NandStorageProvider::closeWrite()`, bạn đã phải parse 16 byte header để xác định định dạng (`SLBX`, `VJPG`, v.v.) và ghi vào bảng `NSLT` ở Sector 0.
> - Trong `MediaPlayer::playItem()`, thay vì tận dụng luôn `info.type` và các thông số `fps`, `frames` đã lưu ở Sector 0, bạn lại bắt SPI đi đọc (seek) lại 20 byte header một lần nữa để lấy biến `_isSlbxRgb565` và kích thước width/height.
> - **Góc nhìn KISS:** Thông tin `SLBX` hoàn toàn có thể được nạp vào struct `StorageItemInfo` (ví dụ thêm cờ `bool isRawRgb`) từ bảng Sector 0. Việc này giúp `MediaPlayer` không cần chạm vào offset 20 tự động mà chỉ dựa vào `StorageItemInfo`.

> [!NOTE]
> **2. Thuật toán quét Key Media URL quá dài dòng**
> - Trong `NetworkManager.cpp`, để tìm URL, code duyệt qua một danh sách: `"bin_url", "binUrl", "video_url", "videoUrl", "image_url", "imageUrl", "media_url", "mediaUrl", "url"`.
> - Việc lập trình "phòng thủ" (Defensive Programming) này là tốt nếu API Backend thay đổi liên tục, nhưng trong hệ thống nhúng, nó làm code phình to, tốn chu kỳ CPU và vi phạm KISS. 
> - **Góc nhìn KISS:** Hãy chốt ĐÚNG 1 hoặc 2 key duy nhất với Backend (ví dụ `media_url` hoặc `url`), và ném lỗi nếu Backend trả sai. Firmware không nên đi "dọn rác" cho Backend.

> [!NOTE]
> **3. Vòng lặp Modulo Ring Buffer (Rất thông minh nhưng hơi low-level)**
> - Logic dịch bit `_unreadBitmask |= (1 << slot)` và vòng lặp `(cur_point + i) % 5` để tìm tin nhắn cũ nhất rất nhanh và tối ưu EEPROM (chỉ tốn 1 byte để lưu mask). Tuy nhiên, về mặt đọc hiểu (Readability) thì nó hơi khó maintain cho người mới. Dù vậy, với một kĩ sư nhúng thì đây là **Good Engineering**, không hẳn là Over-engineering.

---

## User Review Required
Bạn có muốn tôi tự động tiến hành sửa các lỗi rủi ro cao sau không?
1. Đổi `http.getString()` thành Stream Parsing JSON để chống tràn RAM.
2. Thêm cờ chặn Firebase Sync khi đang nạp OTA.
3. Bổ sung tắt Wi-Fi trước khi Light Sleep để đảm bảo an toàn tuyệt đối 100%.

Hãy bấm **Proceed** hoặc báo cho tôi để tôi triển khai nhé.
