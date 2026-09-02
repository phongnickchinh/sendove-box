# TÀI LIỆU NÊN ĐỌC (SHOULD READ) — AUDIO DSP & EMBEDDED AUDIO

Tài liệu này tổng hợp toàn bộ các nguồn kiến thức, sách kinh điển, bài viết chuyên sâu và kênh học tập chuẩn xác nhất về **Xử lý tín hiệu âm thanh số (Audio Digital Signal Processing)** và **Kỹ thuật âm thanh nhúng (Embedded Audio / I2S / Amplifiers)**.

---

## 1. Sách Kinh Điển Nhập Môn (Miễn Phí & Trực Quan Nhất)

### 📘 [The Scientist and Engineer's Guide to Digital Signal Processing](https://www.dspguide.com/)
* **Tác giả**: Steven W. Smith
* **Đặc điểm**: Được mệnh danh là cuốn "kinh thánh" DSP nhập môn hay nhất thế giới. Sách giải thích bằng trực giác, đồ thị và hình ảnh minh họa, không dùng toán vi tích phân trừu tượng.
* **Các chương cần đọc ngay**:
  * **Chapter 3: ADC and DAC** — Nguyên lý lấy mẫu, lượng tử hóa và cách tín hiệu số được chuyển đổi sang tương tự.
  * **Chapter 10: Fast Fourier Transform (FFT)** — Cách âm thanh được phân rã thành các tần số khác nhau.
  * **Chapter 13: Continuous Signal Reconstruction** — Giải thích hiện tượng bậc thang **Zero-Order Hold (ZOH)** và tại sao cần bộ lọc nội suy làm mịn (**Sinc / Linear Interpolation**) để xóa bỏ sóng hài răng cưa (Aliasing).
  * **Chapter 14 – 16: Digital Filters (FIR & IIR)** — Cách viết các bộ lọc thông thấp (Low-Pass Filter) để làm dịu tiếng chói và làm ấm âm thanh.
* **Truy cập**: Đọc online hoặc tải PDF miễn phí 100% tại: [dspguide.com](https://www.dspguide.com/)

---

## 2. Kênh YouTube & Video Đồ Họa Xuất Sắc

### 📺 [Phil's Lab](https://www.youtube.com/@PhilsLab)
* **Chủ đề**: Kỹ thuật phần cứng âm thanh nhúng (Embedded Audio Engineering).
* **Nội dung nổi bật**:
  * Hướng dẫn thiết kế mạch phần cứng âm thanh số: Chuẩn I2S, chip giải mã DAC (PCM5102, I2S DAC), ampli Class-D (MAX98357A, TPA3116).
  * Lập trình bộ lọc số (FIR / IIR / Biquad filters) trên vi điều khiển STM32 / ESP32.
  * Kỹ thuật thiết kế PCB 4 lớp chống nhiễu cho tín hiệu âm thanh Analog và Digital.

### 📺 [3Blue1Brown - But what is the Fourier Transform? A visual introduction](https://www.youtube.com/watch?v=spUNpyF58BY)
* **Chủ đề**: Biến đổi Fourier và bản chất của sóng âm thanh.
* **Nội dung nổi bật**: Video giải thích bằng đồ họa chuyển động trực quan số 1 thế giới về việc sóng âm thanh phức tạp được cấu thành từ các tần số hình sin như thế nào.

---

## 3. Blog Kỹ Thuật & Kho Thuật Toán DSP Mã Nguồn Mở

### 🌐 [EarLevel Engineering](https://www.earlevel.com/)
* **Tác giả**: Nigel Redmon
* **Nội dung**: Blog chuyên sâu của kỹ sư âm thanh về các thuật toán DSP thực hành:
  * **Linear Interpolation & Fractional Delay**: Cách nội suy mẫu âm thanh mượt mà giữa các chu kỳ lấy mẫu.
  * **Biquad Filters**: Công thức tính toán bộ lọc Equalizer (Bass Boost, Treble Cut, Low-pass, High-pass) dễ dàng nhúng vào C/C++.

### 🌐 [MusicDSP.org](https://www.musicdsp.org/)
* **Nội dung**: Kho thuật toán mã nguồn mở (C/C++) chứa hàng trăm hàm xử lý âm thanh thực chiến:
  * Bộ lọc âm sắc (Low-pass, High-pass, Band-pass, Parametric EQ).
  * Thuật toán biến đổi tần số lấy mẫu (**Sample Rate Conversion - Resampling**).
  * Hiệu ứng âm thanh (Compressor, Limiter, Reverb, Overdrive).

---

## 4. Tài Liệu Kỹ Thuật Phần Cứng (Application Notes)

* **Maxim Integrated / Analog Devices:**
  * *MAX98357A/MAX98357B Datasheet & Application Notes*: Giải thích chi tiết kiến trúc xung nhịp I2S (BCLK, LRCLK), cơ chế khóa pha PLL nội và lý do BCLK cần tần số > 1MHz để chống trượt pha gây rè tiếng.
* **Texas Instruments (TI) - Precision Labs Audio:**
  * *Understanding I2S Bus Protocol & Clocking Requirements*: Phân tích chuẩn xung nhịp âm thanh I2S.
  * *Loudspeaker Enclosure Physics*: Hiện tượng đoản mạch âm thanh (**Acoustic Short Circuit / Baffle Phase Cancellation**) khi củ loa không có thùng/buồng âm kín.

---

## 5. Bảng Từ Khóa Cốt Lõi (Keywords Tra Cứu Nhanh)

| Lĩnh vực | Từ khóa tiếng Anh tra cứu | Ý nghĩa cốt lõi |
|---|---|---|
| **Nội suy & Nâng tần số** | `Zero-Order Hold vs Linear Interpolation Audio` | Sự khác biệt giữa lặp mẫu thô (bậc thang) và nội suy dốc làm mịn sóng âm |
| **Chuyển đổi tần số mẫu** | `Sample Rate Conversion (SRC) algorithms` | Cách tăng giảm tần số lấy mẫu (vd: 8kHz -> 32kHz, 44.1kHz -> 48kHz) |
| **Nhiễu răng cưa tần số** | `Aliasing in Digital Audio & Reconstruction Filter` | Sóng hài bậc cao xuất hiện khi chuyển đổi DAC và cách lọc bỏ |
| **Xung nhịp I2S** | `I2S BCLK to LRCLK ratio & Clock Jitter` | Tỷ lệ xung nhịp bit clock so với word select trong giao thức I2S |
| **Ampli Class-D** | `Filterless Class-D amplifier modulation` | Cách ampli MAX98357A điều biến xung PWM trực tiếp ra loa |
| **Vật lý buồng âm** | `Micro speaker acoustic enclosure & Baffle design` | Cách làm kín hộp loa để chống triệt tiêu dải trầm (Bass Cancellation) |
