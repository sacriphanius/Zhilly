# Zhilly Firmware Detailed User Manual

This manual provides cybersecurity researchers and technology enthusiasts with a detailed breakdown of Zhilly firmware's advanced features and code-based operating principles.

---

## 1. Supported Models & Status

While the firmware utilizes a generalized architecture, it includes specific optimizations for certain hardware:

-   **LilyGO T-Embed CC1101 (Primary):** Full support for all RF, IR, BadUSB, and AI features.
-   **LilyGO T-Watch S3:** Currently supports **AI Conversation** only. Other Pentesting tools are under development.
-   **Other Models:** Over 90 board definitions in the `main/boards` folder have the potential to support basic AI functions, but pentest modules may be limited due to missing hardware (CC1101, IR LED, etc.).

---

## 2. AI Voice Command System (MCP)

Zhilly is powered by hardware control tools exposed via the **Model Context Protocol (MCP)**.

-   **Wake Word:** "Nihao Miaoban"
-   **Language Support:** Fluently speaks and understands both Turkish and English.
-   **Emote Display:** The ST7789 display provides visual feedback for listening, thinking, and speaking states.

---

## 3. RF & Sub-GHz Tools (CC1101)

Managed via `cc1101_service.cc`, this module operates in the 300-928 MHz frequency range.

### Usage & Commands:
-   **RF Jammer:** "Start RF jammer" (Infinite by default; for a limit, use "Run jammer for 10 seconds").
-   **RF Replay:** "Replay 'garage.sub' from the SD card." (Searches automatically in `/sdcard/sub` or root).
-   **Tesla Port Opener:** "Open Tesla charging port." (Transmits a specialized signal fixed at 433.92 MHz, AM650 protocol).
-   **RAW Capture (Record):** To save the last captured signal in listening mode: "Save last capture as 'test.sub'".

### SD Card Structure:
-   `/sdcard/sub/`: RF signal files (.sub). Compatible with Flipper Zero format.
-   `/sdcard/cc1101_presets.json`: Custom frequency and modulation settings.

---

## 4. Infrared (IR) Tools

Controlled via `ir_service.cc`, the IR module handles TV and AC remote operations.

-   **TV-B-Gone:** "Turn off nearby TVs." (Sequentially transmits 'eu' or 'us' codes based on region).
-   **IR Jammer:** "Jam IR signals." (Supports sweep, basic, and random modes).
-   **IR Replay:** Sends commands from `.ir` files on the SD card: "Send volume up command for Samsung TV."

---

## 5. BadUSB & Combat Mode

The **Combat Mode**, implemented in `bad_usb_service.cc`, prioritizes all device resources for keyboard emulation.

### What is Combat Mode?
When a BadUSB command is issued, the device automatically enters "Combat Mode." In this mode:
-   Audio services are stopped.
-   Active RF and IR operations are terminated.
-   The device enumerates as an HID Keyboard (TinyUSB) to the host computer.
-   Wi-Fi and Microphone remain active so the assistant can continue receiving commands.

### Keyboard Layouts:
Uses `en_US` by default. To change:
*"Set keyboard layout to Turkish"* -> `tr_TR`

---

## 6. LED Ring (WS2812)

Visual status indicators via 8 addressable RGB LEDs:
-   "Make the LEDs red."
-   "Set brightness to level 5."
-   "Apply a blue scroll effect."

---

## 7. Installation & Flashing

Binaries in the `flash_binaries/` folder can be flashed using `esptool.py`:

```bash
esptool.py -p [PORT] -b 460800 write_flash 0x0 xiaozhi.bin 0x0800000 expression_assets.bin
```

> **Note:** On first boot, you can use a serial terminal (115200 baud) to enter Wi-Fi credentials or follow the PIN code displayed on the screen.

---

## 8. Safety & Ethical Use

This firmware is developed strictly for educational and cybersecurity testing purposes. Unauthorized use against systems is illegal and the user's sole responsibility.
