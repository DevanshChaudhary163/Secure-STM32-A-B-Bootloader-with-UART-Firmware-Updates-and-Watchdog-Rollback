import serial
import time
import struct
from pathlib import Path

PORT = "COM8"          # change if needed
BAUD = 115200

APP_BIN_PATH = "app/app_1/Debug/app_1.bin"   # change this path if your app .bin has a different name

def read_available(ser, delay=0.5):
    time.sleep(delay)
    data = ser.read(ser.in_waiting or 1)
    if data:
        print(data.decode(errors="ignore"), end="")

def read_for_a_bit(ser, seconds=2):
    end_time = time.time() + seconds
    while time.time() < end_time:
        data = ser.read(ser.in_waiting or 1)
        if data:
            print(data.decode(errors="ignore"), end="")
        time.sleep(0.05)

def main():
    app_path = Path(APP_BIN_PATH)

    if not app_path.exists():
        print(f"ERROR: App binary not found: {app_path}")
        print("Fix APP_BIN_PATH in this script.")
        return

    firmware = app_path.read_bytes()
    file_size = len(firmware)

    print(f"Opening {PORT} at {BAUD} baud...")
    print(f"App binary: {app_path}")
    print(f"File size: {file_size} bytes")

    with serial.Serial(PORT, BAUD, timeout=2) as ser:
        time.sleep(2)

        print("\nReading bootloader output...")
        read_available(ser, 1)

        print("\nSending command: recvfile")
        ser.write(b"recvfile\r")
        ser.flush()

        read_available(ser, 0.5)

        print("\nSending file size...")
        ser.write(struct.pack("<I", file_size))
        ser.flush()

        read_available(ser, 0.5)

        print("\nSending file bytes...")
        ser.write(firmware)
        ser.flush()

        print("\nWaiting for bootloader response...\n")
        read_for_a_bit(ser, 8)

        print("\nDone.")

if __name__ == "__main__":
    main()