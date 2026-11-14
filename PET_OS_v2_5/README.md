# PET OS v2.5 — Connected

Target: ESP32-2432S024R (CYD 2.4", ILI9341 + XPT2046), portrait 240×320.

## Features
- Original PET OS v2.4 companion, tasks, notes, timer, RGB LED and 8×4 pet sprite animations.
- Optional Wi-Fi configured from Serial Monitor and stored in ESP32 Preferences.
- NTP time synchronization. The manually selected UTC offset remains authoritative; IP geolocation never changes it.
- Approximate IP-based location and current weather from ipwho.is and Open-Meteo.
- Weather cache retained for offline display.
- Four-point touchscreen calibration saved in Preferences.
- Offline local functions remain available without Wi-Fi.

## Libraries
Install with Arduino Library Manager:
- TFT_eSPI
- XPT2046_Touchscreen
- ArduinoJson **7.x**

Board package: **esp32 by Espressif Systems**.

Configure TFT_eSPI for the board using the known working project setup. Pins and display inversion are retained from v2.4.

## Upload
Open `PET_OS_v2_5.ino` from this folder in Arduino IDE. Keep `PetSprites.h` in the same folder.

## First boot / Wi-Fi
Open Serial Monitor at **115200 baud**, line ending **Newline**, then enter:

```
wifi YOUR_SSID|YOUR_PASSWORD
```

Credentials are stored in NVS Preferences, not in source code. To erase them:

```
wifi clear
```

To refresh weather manually:

```
weather
```

Weather updates automatically about every 30 minutes while connected and after NTP time becomes valid. IP geolocation is approximate and depends on the ISP/VPN. Weather requests use HTTPS with certificate verification disabled in this initial version; do not treat the connection as suitable for sensitive data.

## Touch calibration
Open **SYSTEM → TOUCH CAL** and tap the center of each of the four targets. Calibration values are saved. If calibration is poor, run the wizard again. The known-good axis swap and inversion are intentionally preserved.

## Serial commands
Type `help` in Serial Monitor for the command list. Existing task, note, timezone, LED and reset commands are retained.

## Important
This branch is a development build. It has not been compiled or tested on the physical CYD hardware yet. Confirm the board's TFT_eSPI setup and touch response before relying on it. Wi-Fi credentials previously pasted into a sketch should be changed at the router and re-entered through Serial Monitor.
