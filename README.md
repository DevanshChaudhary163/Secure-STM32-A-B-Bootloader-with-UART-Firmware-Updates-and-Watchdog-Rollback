# Secure STM32 A/B Bootloader with UART Firmware Updates

This project implements a custom A/B bootloader for the STM32 NUCLEO-F401RE. The bootloader supports UART-based firmware updates, dual application slots, metadata-based boot selection, CRC32 firmware validation, application self-confirmation, and watchdog-based rollback.

The goal of this project is to demonstrate a reliable embedded firmware update system similar to what is used in real-world IoT and industrial devices.

---

## 1. Project Overview

The bootloader runs first after reset and decides which application slot should boot.

The firmware is divided into two application slots:

* **Slot A**
* **Slot B**

Only one slot is active at a time. When a new firmware is uploaded, it is written to the inactive slot. The new firmware is first marked as `PENDING_TEST`. If the application boots successfully, it confirms itself as `VALID`. If it fails to confirm, the bootloader rolls back to the previous valid firmware.

---

## 2. Hardware Used

* Board: **NUCLEO-F401RE**
* MCU: **STM32F401RE**
* Flash size: **512 KB**
* RAM size: **96 KB**
* UART: **USART2 over ST-LINK Virtual COM Port**
* Baud rate: **115200**
* User button: **B1 / PC13**
* User LED: **LD2 / PA5**

---

## 3. Flash Memory Layout

```text
0x08000000 - 0x0800FFFF   Bootloader   64 KB
0x08010000 - 0x0801FFFF   Metadata     64 KB
0x08020000 - 0x0803FFFF   Slot A       128 KB
0x08040000 - 0x0805FFFF   Slot B       128 KB
0x08060000 - 0x0807FFFF   Test/Logs    128 KB
```

### Slot Addresses

```c
#define SLOT_A_ADDRESS 0x08020000U
#define SLOT_B_ADDRESS 0x08040000U
```

---

## 4. Project Structure

```text
stm32-project/
│
├── bootloader/
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── boot_jump.h
│   │   │   ├── flash_if.h
│   │   │   ├── metadata_if.h
│   │   │   └── crc_if.h
│   │   └── Src/
│   │       ├── main.c
│   │       ├── boot_jump.c
│   │       ├── flash_if.c
│   │       ├── metadata_if.c
│   │       └── crc_if.c
│
├── app_1/
│   └── Slot A application
│
├── app_2/
│   └── Slot B application
│
└── tools/
    ├── uart_update.py
    └── gui_uploader.py
```

---

## 5. Linker Configuration

Each firmware project has its own flash origin.

### Bootloader

```ld
FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
```

### Slot A Application

```ld
FLASH (rx) : ORIGIN = 0x08020000, LENGTH = 128K
```

### Slot B Application

```ld
FLASH (rx) : ORIGIN = 0x08040000, LENGTH = 128K
```

---

## 6. Vector Table Offset

Each application must use the correct vector table offset.

### Slot A

```c
#define USER_VECT_TAB_ADDRESS
#define VECT_TAB_OFFSET  0x00020000U
```

### Slot B

```c
#define USER_VECT_TAB_ADDRESS
#define VECT_TAB_OFFSET  0x00040000U
```

This ensures interrupts and reset vectors work correctly after the bootloader jumps to the application.

---

## 7. Metadata System

The bootloader stores firmware state in a dedicated metadata sector.

The metadata tracks:

```c
typedef struct
{
    uint32_t magic;
    uint32_t version;

    uint32_t active_slot;

    uint32_t slot_a_state;
    uint32_t slot_b_state;

    uint32_t slot_a_size;
    uint32_t slot_b_size;

    uint32_t slot_a_crc;
    uint32_t slot_b_crc;

    uint32_t boot_attempts;

    uint32_t reserved[5];

} boot_metadata_t;
```

### Slot States

```text
EMPTY         No valid firmware in slot
VALID         Firmware is trusted and bootable
PENDING_TEST  Firmware has been uploaded but not yet confirmed
BAD           Firmware failed boot test
```

---

## 8. Boot Flow

On reset, the bootloader performs the following steps:

```text
1. Bootloader starts.
2. Checks if USER button is pressed.
3. If button is pressed, enter UART bootloader mode.
4. If button is not pressed, read metadata.
5. Select active slot.
6. If active slot is VALID, boot it.
7. If active slot is PENDING_TEST:
   - First attempt: boot it and set boot_attempts = 1.
   - Second attempt: rollback to previous VALID slot.
8. Jump to selected application.
```

---

## 9. Entering Bootloader Mode

To enter UART bootloader mode:

```text
1. Hold USER button.
2. Press and release RESET.
3. Wait around 1 second.
4. Release USER button.
```

The bootloader prints:

```text
BOOTLOADER MODE ACTIVE
```

---

## 10. Bootloader Commands

Final recommended commands:

```text
info
metainfo
update
jump
```

### `info`

Prints bootloader and memory layout information.

### `metainfo`

Prints current metadata:

```text
Metadata: VALID
Active slot: SLOT_A
Slot A state: VALID
Slot B state: VALID
Slot A size: ...
Slot B size: ...
Boot attempts: 0
```

### `update`

Receives a firmware binary over UART and writes it to the inactive slot.

The bootloader automatically chooses the inactive slot:

```text
If active slot is SLOT_A → update target is SLOT_B
If active slot is SLOT_B → update target is SLOT_A
```

### `jump`

Manually jumps to Slot A.

---

## 11. Firmware Update Flow

The firmware update is performed over UART using a Python script or GUI uploader.

The protocol is:

```text
1. Python sends command: update
2. Bootloader asks for file size.
3. Python sends file size as 4 little-endian bytes.
4. Bootloader asks for CRC32.
5. Python sends expected CRC32 as 4 little-endian bytes.
6. Bootloader receives firmware bytes.
7. Bootloader writes firmware to inactive slot.
8. Bootloader calculates CRC32 from flash.
9. If CRC matches, firmware is accepted.
10. New slot is marked PENDING_TEST.
```

---

## 12. CRC32 Firmware Validation

Before the new firmware is accepted, the bootloader verifies its CRC32.

Python calculates:

```python
expected_crc = binascii.crc32(firmware) & 0xFFFFFFFF
```

The bootloader calculates CRC32 directly from flash:

```c
actual_crc = CRC32_Calculate_Flash(target_address, file_size);
```

If both values match:

```text
CRC OK
Metadata update OK
UPDATE PASSED
```

If they do not match:

```text
CRC MISMATCH
Update rejected
```

The metadata is not changed if the CRC fails. This prevents corrupted firmware from being booted.

---

## 13. Firmware Validation Before Boot

The bootloader also validates the uploaded application's vector table.

It checks:

```text
1. First word must be a valid SRAM stack pointer.
2. Second word must be a reset vector inside the target slot.
```

For Slot A:

```text
Reset vector must be 0x08020xxx
```

For Slot B:

```text
Reset vector must be 0x08040xxx
```

This prevents accidentally uploading Slot A firmware into Slot B or Slot B firmware into Slot A.

---

## 14. Application Self-Confirmation

A newly uploaded firmware is not trusted immediately.

After a successful update, the new slot is marked:

```text
PENDING_TEST
```

The application must confirm itself after boot.

Example for Slot B:

```c
if (metadata.active_slot == BOOT_SLOT_B &&
    metadata.slot_b_state == SLOT_STATE_PENDING_TEST)
{
    Metadata_MarkSlotBValid();
}
```

If confirmation succeeds:

```text
Slot B marked VALID
```

The firmware is now trusted.

---

## 15. Watchdog Rollback

The project includes watchdog rollback testing.

A bad firmware was created that:

```text
1. Boots.
2. Does not confirm itself.
3. Starts the watchdog.
4. Hangs forever.
```

The watchdog resets the board. On the next boot, the bootloader sees:

```text
Slot B state: PENDING_TEST
Boot attempts: 1
```

Then it rolls back:

```text
Slot B pending test failed. Rolling back...
Active slot: SLOT_A
Slot B state: BAD
Booting rollback Slot A
```

This protects the device from broken firmware.

---

## 16. Python Terminal Uploader

The terminal uploader is located in:

```text
tools/uart_update.py
```

It performs:

```text
1. Opens COM port.
2. Sends update command.
3. Sends firmware size.
4. Sends CRC32.
5. Sends firmware bytes.
6. Waits for bootloader response.
```

Run it using:

```bash
python tools/uart_update.py
```

---

## 17. Python GUI Uploader

The GUI uploader is located in:

```text
tools/gui_uploader.py
```

Run it using:

```bash
python tools/gui_uploader.py
```

The GUI supports:

```text
Select COM port
Select firmware .bin
Calculate CRC32
Read metadata
Show active slot
Show target slot
Recommend app_1.bin or app_2.bin
Upload firmware
Display bootloader logs
Show success/failure messages
```

The GUI uses the same UART update protocol as the terminal script.

---

## 18. Choosing the Correct Firmware

Because each application is linked to a different flash address, the correct binary must be uploaded to the correct slot.

```text
Target SLOT_A → upload app_1.bin
Target SLOT_B → upload app_2.bin
```

The GUI reads metadata and recommends the correct firmware.

---

## 19. Demo Test Cases

### Test 1: Normal Slot A to Slot B Update

```text
1. Active slot is SLOT_A.
2. Upload app_2.bin.
3. Bootloader writes to Slot B.
4. CRC check passes.
5. Slot B becomes PENDING_TEST.
6. Reset board.
7. Slot B boots.
8. App2 confirms itself VALID.
```

Expected final metadata:

```text
Active slot: SLOT_B
Slot A state: VALID
Slot B state: VALID
Boot attempts: 0
```

---

### Test 2: Normal Slot B to Slot A Update

```text
1. Active slot is SLOT_B.
2. Upload app_1.bin.
3. Bootloader writes to Slot A.
4. CRC check passes.
5. Slot A becomes PENDING_TEST.
6. Reset board.
7. Slot A boots.
8. App1 confirms itself VALID.
```

Expected final metadata:

```text
Active slot: SLOT_A
Slot A state: VALID
Slot B state: VALID
Boot attempts: 0
```

---

### Test 3: Bad Firmware Rollback

```text
1. Upload bad firmware to inactive slot.
2. CRC check passes.
3. New slot becomes PENDING_TEST.
4. Reset board.
5. Bad app boots but does not confirm.
6. Watchdog resets MCU.
7. Bootloader rolls back to previous VALID slot.
```

Expected bootloader log:

```text
pending test failed. Rolling back...
```

---

### Test 4: Corrupted Firmware Rejection

```text
1. Corrupt one byte after CRC is calculated.
2. Upload firmware.
3. Bootloader writes firmware to flash.
4. Bootloader calculates CRC from flash.
5. CRC mismatch is detected.
6. Metadata is not changed.
7. Corrupted firmware is not booted.
```

Expected bootloader log:

```text
CRC MISMATCH
Update rejected
UPDATE FAILED
```

---

## 20. Current Features Completed

```text
Bootloader at 0x08000000
Slot A at 0x08020000
Slot B at 0x08040000
Metadata sector at 0x08010000
UART command mode
Python firmware uploader
Python GUI firmware uploader
Automatic inactive slot update
Flash erase/write
CRC32 validation
Vector table validation
Stack pointer validation
PENDING_TEST state
Application self-confirmation
Watchdog rollback
Metadata inspection
```

---

## 21. Future Improvements

Possible future improvements:

```text
Firmware version numbers
Anti-rollback protection
Firmware header
Cryptographic signature verification
Encrypted firmware transfer
CAN / USB / BLE update transport
Progress bar using bootloader acknowledgements
Production-safe metadata redundancy
```

---

## 22. Summary

This project demonstrates a robust embedded firmware update mechanism on STM32.

The bootloader can safely receive firmware over UART, validate it using CRC32, write it to an inactive slot, test it using a pending-confirmation mechanism, and roll back automatically if the new firmware fails.

This is a practical foundation for secure OTA-style update systems in embedded and IoT devices.
