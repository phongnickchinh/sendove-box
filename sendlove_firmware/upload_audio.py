import serial
import time
import sys
import os

if len(sys.argv) < 3:
    print("Usage: python upload_audio.py <COM_PORT> <FILE>")
    print("Example: python upload_audio.py COM3 test.raw")
    sys.exit(1)

port = sys.argv[1]
filename = sys.argv[2]
size = os.path.getsize(filename)

try:
    ser = serial.Serial(port, 115200, timeout=10)
    ser.dtr = False
    ser.rts = False
    time.sleep(2)
    ser.reset_input_buffer()
except Exception as e:
    print(f"Failed to open port {port}: {e}")
    sys.exit(1)

print(f"Uploading {size} bytes to ESP32 on {port}...")
cmd = f"UPLOAD {size}\n"
ser.write(cmd.encode())

print("Waiting for ESP32 to erase NAND Flash (may take a few seconds)...")
ready = False
for _ in range(60):
    line = ser.readline().decode(errors='replace').strip()
    if line:
        print(f"ESP: {line}")
    if line == "READY":
        ready = True
        break

if not ready:
    print("Error: ESP32 did not respond with READY")
    ser.close()
    sys.exit(1)

chunk_size = 256
t_start = time.time()
with open(filename, 'rb') as f:
    for i in range(0, size, chunk_size):
        chunk = f.read(chunk_size)
        ser.write(chunk)
        ser.flush()
        
        while True:
            line = ser.readline().decode(errors='replace').strip()
            if line.startswith("OK"):
                received = int(line.split()[1])
                if received % 65536 == 0 or received == size:
                    elapsed = time.time() - t_start
                    speed = received / elapsed / 1024 if elapsed > 0 else 0
                    pct = received / size * 100
                    print(f"  [{pct:5.1f}%] {received}/{size} bytes ({speed:.1f} KB/s)")
                break
            elif line:
                print(f"ESP: {line}")

print(f"Upload complete in {time.time() - t_start:.1f} seconds!")
ser.close()
