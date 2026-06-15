import serial
import time
import struct
from pathlib import Path

PORT = "COM8"
BAUD = 115200

APP_BIN_PATH = "app_2/app_2/Debug/app_2.bin"

def read_until_text(ser, expected_text, timeout=20):
    end_time = time.time() + timeout
    received_text = ""

    while time.time() < end_time:
        data = ser.read(ser.in_waiting or 1)

        if data:
            text = data.decode(errors="ignore")
            print(text, end="")
            received_text += text

            if expected_text in received_text:
                return True

        time.sleep(0.02)

    return False

def read_for_a_bit(ser, seconds=20):
    end_time = time.time() + seconds

    while time.time() < end_time:
        data = ser.read(ser.in_waiting or 1)

        if data:
            print(data.decode(errors="ignore"), end="")

        time.sleep(0.05)

def main():
    app_path = Path(APP_BIN_PATH)

    if not app_path.exists():
        print(f"ERROR: Firmware not found: {app_path}")
        return

    firmware = app_path.read_bytes()
    file_size = len(firmware)

    print(f"Opening {PORT} at {BAUD} baud...")
    print(f"Firmware: {app_path}")
    print(f"File size: {file_size} bytes")

    with serial.Serial(PORT, BAUD, timeout=1) as ser:
        time.sleep(2)

        print("\nReading bootloader output...")
        read_for_a_bit(ser, 1)

        print("\nSending command: updateb")
        ser.write(b"updateb\r")
        ser.flush()

        if not read_until_text(ser, "Send file size as 4 bytes", timeout=5):
            print("\nERROR: Bootloader did not ask for file size")
            return

        print("\nSending file size...")
        ser.write(struct.pack("<I", file_size))
        ser.flush()

        if not read_until_text(ser, "Receiving and writing firmware to Slot B", timeout=30):
            print("\nERROR: Bootloader did not become ready to receive firmware")
            return

        print("\nSending firmware bytes...")

        chunk_size = 128

        for i in range(0, file_size, chunk_size):
            chunk = firmware[i:i + chunk_size]
            ser.write(chunk)
            ser.flush()
            time.sleep(0.005)

        print("\nWaiting for bootloader response...\n")
        print("\nWaiting for bootloader response...\n")

        if read_until_text(ser, "Metadata update OK", timeout=30):
            print("\nFirmware upload and metadata update completed.")
        else:
            print("\nERROR: Metadata update was not confirmed.")

        print("\nDone.")

if __name__ == "__main__":
    main()