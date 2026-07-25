#!/usr/bin/env python3
"""
OTA Push Upload Script cho Sendlove Box (ESP32-C3).

Cách dùng:
  python ota_upload.py                         # Dùng mDNS mặc định
  python ota_upload.py --host 192.168.1.100    # Chỉ định IP
  python ota_upload.py --bin path/to/firmware.bin  # Chỉ định file firmware

Luồng:
  1. Đọc firmware .bin, tính MD5
  2. POST /api/ota/begin  → ESP32 chuẩn bị flash
  3. POST /api/ota/upload → Gửi firmware binary
  4. ESP32 tự restart với firmware mới

Fork từ phong_ir/ota_upload.py, điều chỉnh cho ESP32-C3.
"""

import argparse
import hashlib
import os
import sys
import time

try:
    import requests
except ImportError:
    print("ERROR: Cần cài thư viện 'requests'.")
    print("       pip install requests")
    sys.exit(1)

# ---------- defaults ----------
DEFAULT_HOST = "sendlovebox"         # Sử dụng mDNS
DEFAULT_PORT = 80
DEFAULT_BIN  = ".pio/build/esp32-c3-devkitm-1/firmware.bin"
TIMEOUT      = 30  # seconds


def md5_of_file(path: str) -> str:
    h = hashlib.md5()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()


def ota_upload(host: str, port: int, bin_path: str):
    base_url = f"http://{host}:{port}"

    # --- Kiểm tra file ---
    if not os.path.isfile(bin_path):
        print(f"[ERROR] Không tìm thấy firmware: {bin_path}")
        sys.exit(1)

    fw_size = os.path.getsize(bin_path)
    fw_md5  = md5_of_file(bin_path)

    print(f"============================================")
    print(f"       Sendlove Box OTA Push Upload         ")
    print(f"============================================")
    print(f"  Host : {host}:{port}")
    print(f"  File : {bin_path}")
    print(f"  Size : {fw_size:,} bytes")
    print(f"  MD5  : {fw_md5}")
    print(f"============================================")

    # --- Bước 1: POST /api/ota/begin (Tự động retry chờ ESP32 thức dậy từ Sleep) ---
    print("\n[1/2] Đang chờ và kết nối tới ESP32 (/api/ota/begin)...")
    print("      (Nếu ESP32 đang ngủ, script sẽ tự động thử lại trong 130s cho đến khi chip tỉnh)...")

    max_wait_time = 130  # Chờ tối đa 130 giây (> 2 phút)
    start_time = time.time()
    retry_interval = 3   # Thử lại mỗi 3 giây
    connected = False
    r = None

    while time.time() - start_time < max_wait_time:
        try:
            r = requests.post(
                f"{base_url}/api/ota/begin",
                data={"size": str(fw_size), "md5": fw_md5},
                timeout=4,
            )
            if r.status_code == 200:
                connected = True
                break
        except (requests.ConnectionError, requests.Timeout):
            pass

        elapsed = int(time.time() - start_time)
        print(f"      [Chờ ESP32 tỉnh...] Đang thử lại ({elapsed}s / {max_wait_time}s)...", end="\r", flush=True)
        time.sleep(retry_interval)

    print() # Xuống dòng sau tiến trình polling

    if not connected or r is None:
        print(f"[ERROR] Không kết nối được tới {base_url} sau {max_wait_time} giây.")
        print(f"        Kiểm tra ESP32 đã kết nối WiFi và IP đúng chưa.")
        sys.exit(1)

    if r.status_code != 200:
        print(f"[ERROR] ESP32 trả lời: {r.status_code} — {r.text}")
        sys.exit(1)

    resp = r.json()
    if not resp.get("ready"):
        print(f"[ERROR] ESP32 chưa sẵn sàng: {resp}")
        sys.exit(1)

    print("       -> ESP32 đã tỉnh và sẵn sàng nhận firmware (OK)")

    # --- Bước 2: POST /api/ota/upload ---
    print("[2/2] Dang truyen firmware ...")
    start = time.time()

    with open(bin_path, "rb") as f:
        r = requests.post(
            f"{base_url}/api/ota/upload",
            files={"firmware": ("firmware.bin", f, "application/octet-stream")},
            timeout=120,
        )

    elapsed = time.time() - start

    if r.status_code == 200:
        resp = r.json()
        print(f"\n============================================")
        print(f"  [OK] OTA THANH CONG! ({elapsed:.1f}s)")
        print(f"  -> {resp.get('msg', '')}")
        print(f"  Sendlove Box dang restart...")
        print(f"============================================")
    else:
        print(f"\n[ERROR] Upload that bai: {r.status_code} — {r.text}")
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Sendlove Box OTA Push Upload")
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"Hostname/IP của ESP32 (mặc định: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"Port (mặc định: {DEFAULT_PORT})")
    parser.add_argument("--bin", default=DEFAULT_BIN,
                        help=f"Đường dẫn firmware .bin (mặc định: {DEFAULT_BIN})")
    args = parser.parse_args()

    ota_upload(args.host, args.port, args.bin)
