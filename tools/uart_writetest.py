import serial
import time

PORT = "COM8"      # change this if your ST-LINK VCP is different
BAUD = 115200

def read_available(ser, delay=0.5):
    time.sleep(delay)
    data = ser.read(ser.in_waiting or 1)
    if data:
        print(data.decode(errors="ignore"), end="")

def main():
    print(f"Opening {PORT} at {BAUD} baud...")

    with serial.Serial(PORT, BAUD, timeout=2) as ser:
        time.sleep(2)

        print("Reading bootloader output...")
        read_available(ser, 1)

        print("\nSending command: writetest")
        ser.write(b"writetest\r\n")
        ser.flush()

        time.sleep(0.5)
        read_available(ser, 0.5)

        print("\nSending 16 bytes...")
        ser.write(b"abcdefghijklmnop")
        ser.flush()

        time.sleep(1)
        read_available(ser, 1)

        print("\nDone.")

if __name__ == "__main__":
    main()