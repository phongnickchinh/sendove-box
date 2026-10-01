# Thùng rác chờ duyệt

Các file dưới đây **không còn được tham chiếu** trong code, build, CI hay script nào (vài tài
liệu cũ có nhắc tới, đều ở dạng "file thừa, nên xoá"), nhưng chưa chắc xoá được nên được gom
về đây (dọn dẹp ngày 2026-10-02). Mỗi file giữ nguyên
đường dẫn gốc bên trong thư mục này, nên khôi phục bằng một lệnh `git mv` về chỗ cũ.

Xoá cả thư mục này khi đã xem xong. Lịch sử git vẫn giữ mọi thứ.

| Đường dẫn gốc | Vì sao nằm ở đây |
|---|---|
| `sendlove_firmware/src/audio_data.h` | Mảng âm thanh 1,1 MB, không file nào `#include`. `codebase_review.md` cũng đề nghị xoá. |
| `sendlove_firmware/upload_audio.py`, `test.raw` | Script nạp audio qua Serial bằng lệnh `UPLOAD` — firmware không còn lệnh này. `test.raw` (2,9 MB) là file mẫu của script. |
| `sendlove_firmware/wokwi.toml` | Cấu hình mô phỏng Wokwi; env `wokwi` đã bị gỡ khỏi `platformio.ini`. |
| `sendlove_firmware/ChakraPetch-*.ttf` | Font nguồn. Font biên dịch sẵn đã gỡ khỏi firmware (giờ dùng VLW tải theo theme). |
| `sendlove_firmware/image/`, `scratch/` | Ảnh nháp lúc cắt icon Wi-Fi. Icon thật nằm trong `include/Custom*Icons.h`. |
| `sendlove_firmware/implementation_plan.md` | Kế hoạch sửa lỗi reboot khi Light Sleep (08/2026), đã làm xong. |
| `sendlove_firmware/FIREBASE_ANONYMOUS_AUTH.md` | Thiết kế Anonymous Auth (07/2026). Hộp hiện đăng nhập bằng email/password riêng (MEMORY.md §11, §15). |
| `HANDOFF.md` | Bàn giao context ngày 2026-08-31, đã lỗi thời. |
| `DRAFT/login signup.png` | Ảnh nháp màn đăng nhập, không code hay tài liệu thiết kế nào dùng. |
| `sendlove_enclosure/sendlove_box*.scad`, `sendlove_cat_v4.scad` | Vỏ OpenSCAD đời v1–v4, đã thay bằng `breadcat/`. |
| `sendlove_enclosure/*.stl` (`shell*`, `lid*`, `cat_*`, `check`, `chk`) | STL xuất từ các file `.scad` ở trên. |
| `sendlove_enclosure/render*/`, `phac_thao/`, `bo_tri_A.png` | Ảnh render và phác thảo của các đời vỏ cũ. |
| `sendlove_enclosure/breadcat/ban_luu_v1_duoi_thanh/` | Bản lưu Breadcat v1 (đuôi thanh), đã thay bằng bản hiện tại. |

## Đã xoá hẳn (sinh lại được hoặc là file mẫu)

- `sendlove_firmware/*.d` (7 file phụ thuộc của trình biên dịch) — đã thêm `*.d` vào `.gitignore`.
- `build_tail.log` — log build.
- `sendlove_firmware/src/main.cpp.bak` — bản sao cũ của `main.cpp`, MEMORY.md đã ghi là file thừa.
- `sendlove_firmware/ota_architecture_design.md` — trùng từng byte với `docs/ota_architecture_design.md`.
- `sendlove_firmware/{include,lib,test}/README` — file mẫu của PlatformIO.
- `sendlove_web/README.md`, `src/assets/{react.svg,vite.svg,hero.png}`, `public/icons.svg` — file mẫu của Vite, không được import.
- `sendlove_backend/src/services/media-processing.service.ts` (và bản `lib/`) — hàm giữ chỗ, không file nào import.

## Chỉ ghi nhận, không đụng

- `sendlove_firmware/compile_commands.json` (2,2 MB, sinh bằng `pio run -t compiledb`) — clangd đang dùng.
- `.firebase/hosting.*.cache` — cache deploy đang bị track; nên `git rm --cached` rồi thêm `.firebase/` vào `.gitignore`.
- `sendlove_enclosure/breadcat/chay_lai.log`, các `.blend`/`.stl`/`.png` do `chay_lai.bat` sinh ra — đang được sửa ở nhánh `main`.
- `codebase_review.md`, `code_review_2_9_gemini_38.md`, `fix_download_timeout_plan.md`, `SHOULD_READ.md`, `ACTION_PLAN.md` — cũ nhưng còn được `MEMORY.md`, `BOM.md`, `README.md` dẫn tới.
- `sendlove_kicad/` — chỉ có file thiết kế và thư viện, không có gì để dọn.
- `sendlove_backend/package.json` còn khai báo `@ffmpeg-installer/ffmpeg`, `fluent-ffmpeg`, `sharp` mà `src/` không dùng — gỡ là đổi gói deploy nên để user quyết.
