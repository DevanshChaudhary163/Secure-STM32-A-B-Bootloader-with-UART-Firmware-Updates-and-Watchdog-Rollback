import serial
import time
import struct
from pathlib import Path
import binascii

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


def read_until_any_text(ser, expected_texts, timeout=30):
    end_time = time.time() + timeout
    received_text = ""

    while time.time() < end_time:
        data = ser.read(ser.in_waiting or 1)

        if data:
            text = data.decode(errors="ignore")
            print(text, end="")
            received_text += text

            for expected in expected_texts:
                if expected in received_text:
                    return expected

        time.sleep(0.02)

    return None


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
        print(f"ERROR: Firmware not found: {app_path}")
        return

    firmware = app_path.read_bytes()
    file_size = len(firmware)

    expected_crc = binascii.crc32(firmware) & 0xFFFFFFFF

    print(f"Opening {PORT} at {BAUD} baud...")
    print(f"Firmware: {app_path}")
    print(f"File size: {file_size} bytes")
    print(f"Expected CRC32: 0x{expected_crc:08X}")

    with serial.Serial(PORT, BAUD, timeout=1) as ser:
        time.sleep(2)

        print("\nReading bootloader output...")
        read_for_a_bit(ser, 1)

        print("\nSending command: update")
        ser.write(b"update\r")
        ser.flush()

        if not read_until_text(ser, "Send file size as 4 bytes", timeout=5):
            print("\nERROR: Bootloader did not ask for file size")
            return

        print("\nSending file size...")
        ser.write(struct.pack("<I", file_size))
        ser.flush()

        if not read_until_text(ser, "Send expected CRC32 as 4 bytes", timeout=5):
            print("\nERROR: Bootloader did not ask for CRC32")
            return

        print("\nSending expected CRC32...")
        ser.write(struct.pack("<I", expected_crc))
        ser.flush()

        if not read_until_text(ser, "Receiving and writing firmware", timeout=30):
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

        result = read_until_any_text(
            ser,
            [
                "UPDATE PASSED",
                "CRC MISMATCH",
                "Update rejected",
                "UPDATE FAILED",
                "Metadata update FAILED"
            ],
            timeout=60
        )

        if result == "UPDATE PASSED":
            print("\nFirmware upload completed successfully.")
        elif result == "CRC MISMATCH":
            print("\nERROR: CRC mismatch. Firmware was rejected.")
        elif result == "Update rejected":
            print("\nERROR: Update rejected by bootloader.")
        elif result == "UPDATE FAILED":
            print("\nERROR: Update failed.")
        elif result == "Metadata update FAILED":
            print("\nERROR: Firmware written, but metadata update failed.")
        else:
            print("\nERROR: No final bootloader response received.")

        print("\nDone.")


if __name__ == "__main__":
    main()