# Fix Light Sleep Reboots ("Booting..." issue)

Hiện tại, Box đang bị hiện tượng Reset (khởi động lại từ đầu và hiện chữ "Booting...") thay vì thức dậy bình thường từ Light Sleep. 

## Nguyên nhân gốc rễ (Root Cause)
Khi gọi lệnh `esp_light_sleep_start()` bằng tay (Manual Light Sleep), nếu module Wi-Fi **chưa được tắt hoàn toàn**, phần cứng MAC/Baseband của Wi-Fi trên ESP32 sẽ rơi vào trạng thái lỗi (panic) hoặc gây ra WatchDog Timeout (WDT), dẫn đến việc chip tự động khởi động lại (Reset).

## Các giải pháp đề xuất (Proposed Solutions)

### Giải pháp 1: Tắt hoàn toàn Wi-Fi trước khi ngủ (Khuyên dùng - An toàn nhất)
- **Cách hoạt động**: Trong hàm `enterLightSleep()`, chúng ta sẽ ngắt kết nối và tắt module Wi-Fi (gọi `WiFi.mode(WIFI_OFF)` hoặc `esp_wifi_stop()`). Sau khi thức dậy, chúng ta sẽ bật lại Wi-Fi và cho phép nó tự động kết nối lại ở chế độ nền (background).
- **Ưu điểm**: Đảm bảo 100% không bị crash/reboot do phần cứng Wi-Fi. Tiết kiệm pin tối đa khi ngủ.
- **Nhược điểm**: Mất khoảng 1-3 giây để Wi-Fi kết nối lại sau khi thức dậy. Tuy nhiên, do tính năng đồng bộ Firebase đang chạy ngầm (`FirebaseTask`), việc trễ 2 giây này hoàn toàn **không ảnh hưởng** đến tốc độ bật màn hình khi Touch (màn hình vẫn sáng ngay lập tức < 50ms).

### Giải pháp 2: Sử dụng Automatic Light Sleep (Modem Sleep)
- **Cách hoạt động**: Thay vì ép chip ngủ bằng lệnh `esp_light_sleep_start()`, chúng ta cấu hình chế độ Power Management của ESP-IDF để chip tự động ngủ khi rảnh rỗi (`WIFI_PS_MIN_MODEM`). Chúng ta chỉ việc tắt màn hình (`display->turnOff()`).
- **Ưu điểm**: Wi-Fi vẫn duy trì kết nối liên tục, không cần tốn thời gian reconnect.
- **Nhược điểm**: Chip sẽ thức dậy liên tục mỗi ~100ms để bắt các gói tin beacon của Router Wi-Fi, do đó tiêu thụ pin sẽ cao hơn khá nhiều so với Giải pháp 1. Dễ bị nhiễu và khó kiểm soát trạng thái ngủ sâu.

## Kế hoạch triển khai cho Giải pháp 1 (Nếu bạn chọn)

### 1. Cập nhật `PowerManager.cpp`
Trước khi gọi `esp_light_sleep_start()`, thêm lệnh tắt Wi-Fi.

```cpp
#include <WiFi.h>
#include <esp_wifi.h>

void PowerManager::enterLightSleep(uint64_t sleepDurationUs, DisplayDriver* display) {
    uint32_t waitStart = millis();
    while (digitalRead(_touchPin) == HIGH && (millis() - waitStart < 2000)) delay(10);

    if (display != nullptr) display->turnOff();

    // 1. NGẮT KẾT NỐI VÀ TẮT HẲN WI-FI ĐỂ TRÁNH CRASH MAC BASEBAND
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(50); // Chờ Wi-Fi tắt hẳn

    configureTimerWakeup(sleepDurationUs);
    configureTouchWakeup(_touchPin);

    esp_light_sleep_start();
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    delay(50);
}
```

### 2. Cập nhật `NetworkManager::ensureConnected()` trong `NetworkManager.cpp`
Hàm này được gọi ngay sau khi thức dậy trong `main.cpp`. Cần sửa để nó khởi động lại Wi-Fi đúng cách.

```cpp
void NetworkManager::ensureConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        // Bật lại Wi-Fi từ trạng thái OFF
        if (WiFi.getMode() == WIFI_OFF) {
            WiFi.mode(WIFI_STA);
        }
        WiFi.reconnect();
    }
}
```

## User Review Required
> [!IMPORTANT]
> Hãy cho tôi biết bạn muốn chọn Giải pháp 1 hay Giải pháp 2? Giải pháp 1 là lựa chọn tốt nhất để giải quyết triệt để lỗi "Booting..." mà vẫn đáp ứng được yêu cầu về Task Sync ngầm của bạn.
