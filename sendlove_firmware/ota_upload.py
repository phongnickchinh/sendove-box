#!/usr/bin/env python3
"""
OTA Push Upload Script for Sendlove Box (ESP32-C3).

Usage:
  python ota_upload.py --host 192.168.1.100    # IP shown on the OTA mode screen (RECOMMENDED)
  python ota_upload.py                         # mDNS sendlovebox.local (often fails to resolve on Windows)
  python ota_upload.py --bin path/to/firmware.bin  # Specific firmware binary file

Before running: on the box HOLD 3s, release, HOLD 3s, release, then HOLD 6s to enter OTA mode.
The web server only exists in that mode (it is not enabled by a cloud flag).

Workflow:
  1. Read firmware binary, compute MD5
  2. POST /api/ota/begin  -> ESP32 prepares flash partition
  3. POST /api/ota/upload -> Send firmware binary
  4. ESP32 automatically restarts with new firmware
  5. The new build must run 60s before it is confirmed; a reset before that rolls the box back
"""

import argparse
import hashlib
import os
import sys
import time

try:
    import requests
except ImportError:
    print("ERROR: Missing 'requests' library.")
    print("       pip install requests")
    sys.exit(1)

# Defaults
# ".local" is required to go through mDNS. The bare name "sendlovebox" does not resolve.
DEFAULT_HOST = "sendlovebox.local"
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

    if not os.path.isfile(bin_path):
        print(f"[ERROR] Firmware binary file not found: {bin_path}")
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

    # Step 1: POST /api/ota/begin. Retry until the user puts the box into OTA mode.
    max_wait_time = 310  # Wait up to 310 seconds (> 5 minutes)
    start_time = time.time()
    retry_interval = 3   # Retry every 3 seconds
    connected = False
    r = None

    print("\n[1/2] Connecting to ESP32 (/api/ota/begin)...")
    print(f"      (Chua vao che do OTA? Tren hop: giu 3s, giu 3s, roi giu 6s. Script thu lai trong {max_wait_time}s)...")

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
        print(f"      [Waiting for ESP32...] Retrying ({elapsed}s / {max_wait_time}s)...", end="\r", flush=True)
        time.sleep(retry_interval)

    print()  # New line after polling progress

    if not connected or r is None:
        print(f"[ERROR] Could not connect to {base_url} after {max_wait_time} seconds.")
        print(f"        Verify ESP32 is connected to Wi-Fi and IP/hostname is correct.")
        sys.exit(1)

    if r.status_code != 200:
        print(f"[ERROR] ESP32 returned status {r.status_code}: {r.text}")
        sys.exit(1)

    resp = r.json()
    if not resp.get("ready"):
        print(f"[ERROR] ESP32 not ready: {resp}")
        sys.exit(1)

    print("       -> ESP32 awake and ready for firmware update (OK)")

    # Step 2: POST /api/ota/upload
    print("[2/2] Uploading firmware binary...")
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
        print(f"  [OK] OTA UPLOAD SUCCESSFUL! ({elapsed:.1f}s)")
        print(f"  -> {resp.get('msg', '')}")
        print(f"  Sendlove Box is restarting...")
        print(f"")
        print(f"  CHO 60 GIAY roi moi rut dien: ban moi dang thu thach,")
        print(f"  reset truoc moc do thi hop tu quay ve ban cu.")
        print(f"============================================")
    else:
        print(f"\n[ERROR] Upload failed status {r.status_code}: {r.text}")
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Sendlove Box OTA Push Upload Script")
    parser.add_argument("--host", default=DEFAULT_HOST,
                        help=f"ESP32 Hostname/IP address (default: {DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT,
                        help=f"Port number (default: {DEFAULT_PORT})")
    parser.add_argument("--bin", default=DEFAULT_BIN,
                        help=f"Path to firmware .bin file (default: {DEFAULT_BIN})")
    args = parser.parse_args()

    ota_upload(args.host, args.port, args.bin)
