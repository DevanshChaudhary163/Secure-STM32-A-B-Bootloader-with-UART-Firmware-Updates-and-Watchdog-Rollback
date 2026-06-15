import tkinter as tk
from tkinter import filedialog, messagebox
from tkinter.scrolledtext import ScrolledText
import serial
import serial.tools.list_ports
import threading
import time
import struct
import binascii
from pathlib import Path


BAUD = 115200
CHUNK_SIZE = 128


class BootloaderUploaderGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("STM32 A/B Bootloader Firmware Uploader")
        self.root.geometry("900x650")

        self.firmware_path = None
        self.firmware_data = None
        self.firmware_crc = None

        self.active_slot = None
        self.target_slot = None

        self.create_widgets()
        self.refresh_ports()

    def create_widgets(self):
        top_frame = tk.Frame(self.root)
        top_frame.pack(fill="x", padx=10, pady=10)

        tk.Label(top_frame, text="COM Port:").grid(row=0, column=0, sticky="w")

        self.port_var = tk.StringVar()
        self.port_menu = tk.OptionMenu(top_frame, self.port_var, "")
        self.port_menu.grid(row=0, column=1, sticky="ew", padx=5)

        tk.Button(top_frame, text="Refresh Ports", command=self.refresh_ports).grid(row=0, column=2, padx=5)

        tk.Label(top_frame, text="Baud:").grid(row=0, column=3, sticky="w", padx=(20, 0))
        self.baud_var = tk.StringVar(value=str(BAUD))
        tk.Entry(top_frame, textvariable=self.baud_var, width=10).grid(row=0, column=4, padx=5)

        top_frame.columnconfigure(1, weight=1)

        metadata_frame = tk.Frame(self.root)
        metadata_frame.pack(fill="x", padx=10, pady=5)

        tk.Button(
            metadata_frame,
            text="Read Metadata",
            command=self.start_read_metadata_thread,
            height=2,
            bg="#cfe2f3"
        ).pack(side="left")

        self.active_slot_label = tk.Label(metadata_frame, text="Active slot: -", anchor="w")
        self.active_slot_label.pack(side="left", padx=15)

        self.target_slot_label = tk.Label(metadata_frame, text="Target slot: -", anchor="w")
        self.target_slot_label.pack(side="left", padx=15)

        self.recommend_label = tk.Label(
            metadata_frame,
            text="Recommended firmware: -",
            anchor="w",
            fg="blue"
        )
        self.recommend_label.pack(side="left", padx=15)

        file_frame = tk.Frame(self.root)
        file_frame.pack(fill="x", padx=10, pady=5)

        tk.Button(file_frame, text="Select Firmware .bin", command=self.select_firmware).pack(side="left")

        self.file_label = tk.Label(file_frame, text="No firmware selected", anchor="w")
        self.file_label.pack(side="left", padx=10, fill="x", expand=True)

        info_frame = tk.Frame(self.root)
        info_frame.pack(fill="x", padx=10, pady=5)

        self.size_label = tk.Label(info_frame, text="Size: -")
        self.size_label.pack(side="left", padx=5)

        self.crc_label = tk.Label(info_frame, text="CRC32: -")
        self.crc_label.pack(side="left", padx=20)

        button_frame = tk.Frame(self.root)
        button_frame.pack(fill="x", padx=10, pady=10)

        self.upload_button = tk.Button(
            button_frame,
            text="Upload Firmware",
            command=self.start_upload_thread,
            height=2,
            bg="#d9ead3"
        )
        self.upload_button.pack(side="left", fill="x", expand=True)

        tk.Button(
            button_frame,
            text="Clear Log",
            command=self.clear_log,
            height=2
        ).pack(side="left", padx=10)

        self.log = ScrolledText(self.root, height=26)
        self.log.pack(fill="both", expand=True, padx=10, pady=10)

    def log_write(self, text):
        self.log.insert(tk.END, text)
        self.log.see(tk.END)
        self.root.update_idletasks()

    def clear_log(self):
        self.log.delete("1.0", tk.END)

    def refresh_ports(self):
        ports = [port.device for port in serial.tools.list_ports.comports()]

        menu = self.port_menu["menu"]
        menu.delete(0, "end")

        if not ports:
            ports = [""]

        for port in ports:
            menu.add_command(label=port, command=lambda value=port: self.port_var.set(value))

        self.port_var.set(ports[0])

    def select_firmware(self):
        path = filedialog.askopenfilename(
            title="Select firmware binary",
            filetypes=[("Binary files", "*.bin"), ("All files", "*.*")]
        )

        if not path:
            return

        self.firmware_path = Path(path)
        self.firmware_data = self.firmware_path.read_bytes()
        self.firmware_crc = binascii.crc32(self.firmware_data) & 0xFFFFFFFF

        self.file_label.config(text=str(self.firmware_path))
        self.size_label.config(text=f"Size: {len(self.firmware_data)} bytes")
        self.crc_label.config(text=f"CRC32: 0x{self.firmware_crc:08X}")

        self.log_write(f"Selected firmware: {self.firmware_path}\n")
        self.log_write(f"Size: {len(self.firmware_data)} bytes\n")
        self.log_write(f"CRC32: 0x{self.firmware_crc:08X}\n\n")

    def read_serial_for_seconds(self, ser, seconds=1):
        end_time = time.time() + seconds
        received_text = ""

        while time.time() < end_time:
            data = ser.read(ser.in_waiting or 1)

            if data:
                text = data.decode(errors="ignore")
                self.log_write(text)
                received_text += text

            time.sleep(0.05)

        return received_text

    def read_until_text(self, ser, expected_text, timeout=20):
        end_time = time.time() + timeout
        received_text = ""

        while time.time() < end_time:
            data = ser.read(ser.in_waiting or 1)

            if data:
                text = data.decode(errors="ignore")
                self.log_write(text)
                received_text += text

                if expected_text in received_text:
                    return True

            time.sleep(0.02)

        return False

    def read_until_any_text(self, ser, expected_texts, timeout=60):
        end_time = time.time() + timeout
        received_text = ""

        while time.time() < end_time:
            data = ser.read(ser.in_waiting or 1)

            if data:
                text = data.decode(errors="ignore")
                self.log_write(text)
                received_text += text

                for expected in expected_texts:
                    if expected in received_text:
                        return expected

            time.sleep(0.02)

        return None

    def start_read_metadata_thread(self):
        thread = threading.Thread(target=self.read_metadata, daemon=True)
        thread.start()

    def read_metadata(self):
        port = self.port_var.get().strip()

        if not port:
            messagebox.showerror("Error", "Please select a COM port.")
            return

        try:
            baud = int(self.baud_var.get())
        except ValueError:
            messagebox.showerror("Error", "Invalid baud rate.")
            return

        try:
            self.log_write("\n========================================\n")
            self.log_write("Reading bootloader metadata\n")
            self.log_write("Make sure board is already in bootloader mode.\n")
            self.log_write("========================================\n\n")

            with serial.Serial(port, baud, timeout=1) as ser:
                time.sleep(2)

                self.read_serial_for_seconds(ser, 1)

                self.log_write("\nSending command: metainfo\n")
                ser.write(b"metainfo\r")
                ser.flush()

                response = self.read_serial_for_seconds(ser, 3)

            if "Active slot: SLOT_A" in response:
                self.active_slot = "SLOT_A"
                self.target_slot = "SLOT_B"

                self.active_slot_label.config(text="Active slot: SLOT_A")
                self.target_slot_label.config(text="Target slot: SLOT_B")
                self.recommend_label.config(text="Recommended firmware: app_2.bin")

                self.log_write("\nDetected active slot: SLOT_A\n")
                self.log_write("Next update target: SLOT_B\n")
                self.log_write("Choose firmware: app_2.bin\n\n")

            elif "Active slot: SLOT_B" in response:
                self.active_slot = "SLOT_B"
                self.target_slot = "SLOT_A"

                self.active_slot_label.config(text="Active slot: SLOT_B")
                self.target_slot_label.config(text="Target slot: SLOT_A")
                self.recommend_label.config(text="Recommended firmware: app_1.bin")

                self.log_write("\nDetected active slot: SLOT_B\n")
                self.log_write("Next update target: SLOT_A\n")
                self.log_write("Choose firmware: app_1.bin\n\n")

            elif "Metadata: INVALID" in response:
                self.active_slot = None
                self.target_slot = None

                self.active_slot_label.config(text="Active slot: INVALID")
                self.target_slot_label.config(text="Target slot: -")
                self.recommend_label.config(text="Recommended firmware: run metainit first")

                self.log_write("\nMetadata is invalid. Run metainit first from PuTTY.\n\n")

            else:
                self.log_write("\nCould not detect active slot from metadata response.\n")
                messagebox.showwarning(
                    "Metadata",
                    "Could not detect active slot. Check bootloader log."
                )

        except serial.SerialException as e:
            self.log_write(f"\nSerial error: {e}\n")
            messagebox.showerror("Serial Error", str(e))
        except Exception as e:
            self.log_write(f"\nError: {e}\n")
            messagebox.showerror("Error", str(e))

    def start_upload_thread(self):
        thread = threading.Thread(target=self.upload_firmware, daemon=True)
        thread.start()

    def check_selected_firmware_matches_target(self):
        if self.target_slot is None or self.firmware_path is None:
            return True

        filename = self.firmware_path.name.lower()

        if self.target_slot == "SLOT_A" and "app_1" not in filename and "app1" not in filename:
            return messagebox.askyesno(
                "Firmware warning",
                "Target slot is SLOT_A, but selected file does not look like app_1.bin.\n\nContinue anyway?"
            )

        if self.target_slot == "SLOT_B" and "app_2" not in filename and "app2" not in filename:
            return messagebox.askyesno(
                "Firmware warning",
                "Target slot is SLOT_B, but selected file does not look like app_2.bin.\n\nContinue anyway?"
            )

        return True

    def upload_firmware(self):
        if not self.firmware_data:
            messagebox.showerror("Error", "Please select a firmware .bin file first.")
            return

        if not self.check_selected_firmware_matches_target():
            return

        port = self.port_var.get().strip()

        if not port:
            messagebox.showerror("Error", "Please select a COM port.")
            return

        try:
            baud = int(self.baud_var.get())
        except ValueError:
            messagebox.showerror("Error", "Invalid baud rate.")
            return

        self.upload_button.config(state="disabled")

        try:
            self.log_write("========================================\n")
            self.log_write("Starting firmware upload\n")
            self.log_write(f"Port: {port}\n")
            self.log_write(f"Baud: {baud}\n")
            self.log_write(f"Firmware: {self.firmware_path}\n")
            self.log_write(f"Size: {len(self.firmware_data)} bytes\n")
            self.log_write(f"CRC32: 0x{self.firmware_crc:08X}\n")

            if self.active_slot:
                self.log_write(f"Known active slot: {self.active_slot}\n")
                self.log_write(f"Expected target slot: {self.target_slot}\n")

            self.log_write("========================================\n\n")

            with serial.Serial(port, baud, timeout=1) as ser:
                time.sleep(2)

                self.log_write("Reading bootloader output...\n")
                self.read_serial_for_seconds(ser, 1)

                self.log_write("\nSending command: update\n")
                ser.write(b"update\r")
                ser.flush()

                if not self.read_until_text(ser, "Send file size as 4 bytes", timeout=5):
                    self.log_write("\nERROR: Bootloader did not ask for file size.\n")
                    return

                self.log_write("\nSending file size...\n")
                ser.write(struct.pack("<I", len(self.firmware_data)))
                ser.flush()

                if not self.read_until_text(ser, "Send expected CRC32 as 4 bytes", timeout=5):
                    self.log_write("\nERROR: Bootloader did not ask for CRC32.\n")
                    return

                self.log_write("\nSending expected CRC32...\n")
                ser.write(struct.pack("<I", self.firmware_crc))
                ser.flush()

                if not self.read_until_text(ser, "Receiving and writing firmware", timeout=30):
                    self.log_write("\nERROR: Bootloader did not become ready to receive firmware.\n")
                    return

                self.log_write("\nSending firmware bytes...\n")

                total = len(self.firmware_data)
                last_percent_printed = -1

                for i in range(0, total, CHUNK_SIZE):
                    chunk = self.firmware_data[i:i + CHUNK_SIZE]
                    ser.write(chunk)
                    ser.flush()
                    time.sleep(0.005)

                    percent = int(((i + len(chunk)) * 100) / total)

                    if percent >= last_percent_printed + 10:
                        last_percent_printed = percent
                        self.log_write(f"GUI progress: {percent}%\n")

                self.log_write("\nWaiting for final bootloader response...\n\n")

                result = self.read_until_any_text(
                    ser,
                    [
                        "UPDATE PASSED",
                        "CRC MISMATCH",
                        "Update rejected",
                        "Invalid reset vector",
                        "Invalid stack pointer",
                        "UPDATE FAILED",
                        "Metadata update FAILED"
                    ],
                    timeout=60
                )

                if result == "UPDATE PASSED":
                    self.log_write("\nSUCCESS: Firmware upload completed.\n")
                    messagebox.showinfo("Success", "Firmware upload completed successfully.")
                elif result == "CRC MISMATCH":
                    self.log_write("\nERROR: CRC mismatch. Firmware rejected.\n")
                    messagebox.showerror("CRC Error", "CRC mismatch. Firmware was rejected.")
                elif result == "Update rejected":
                    self.log_write("\nERROR: Update rejected by bootloader.\n")
                    messagebox.showerror("Rejected", "Update rejected by bootloader.")
                elif result == "Invalid reset vector":
                    self.log_write("\nERROR: Invalid reset vector for selected slot.\n")
                    messagebox.showerror("Invalid Firmware", "Invalid reset vector for selected slot.")
                elif result == "Invalid stack pointer":
                    self.log_write("\nERROR: Invalid stack pointer.\n")
                    messagebox.showerror("Invalid Firmware", "Invalid stack pointer.")
                elif result == "UPDATE FAILED":
                    self.log_write("\nERROR: Update failed.\n")
                    messagebox.showerror("Failed", "Update failed.")
                elif result == "Metadata update FAILED":
                    self.log_write("\nERROR: Metadata update failed.\n")
                    messagebox.showerror("Metadata Error", "Metadata update failed.")
                else:
                    self.log_write("\nERROR: No final response from bootloader.\n")
                    messagebox.showerror("Timeout", "No final response from bootloader.")

        except serial.SerialException as e:
            self.log_write(f"\nSerial error: {e}\n")
            messagebox.showerror("Serial Error", str(e))
        except Exception as e:
            self.log_write(f"\nError: {e}\n")
            messagebox.showerror("Error", str(e))
        finally:
            self.upload_button.config(state="normal")


if __name__ == "__main__":
    root = tk.Tk()
    app = BootloaderUploaderGUI(root)
    root.mainloop()