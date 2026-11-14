# NIXI OS v2.7

Target: ESP32-2432S024R (2.4-inch CYD, ILI9341 + XPT2046), portrait 240x320.

## What's new in 2.7
- Product identity renamed to **NIXI OS**.
- Companion screen and home tab renamed to **NIXI**.
- NIXI status labels and care actions localized to Russian.
- Preserves the v2.6 hybrid companion logic, Wi-Fi/NTP, Open-Meteo weather for Taganrog, tasks, notes, RGB LED, UTC offset, touch calibration and reset.

## Important sprite status
The supplied concept atlas is a visual reference, not a frame-aligned transparent sprite sheet. This build temporarily retains the legacy 48x48 PET sprite renderer and `PetSprites.h`; the NIXI character artwork has **not yet been converted into import-ready RGB565 frames**. This is intentionally called out rather than silently presenting the old cat as NIXI. Replace this header and renderer when the final 64x80 NIXI frame set is prepared.

## Libraries
- TFT_eSPI
- XPT2046_Touchscreen
- ArduinoJson
- ESP32 Arduino core

Configure Wi-Fi in Serial Monitor at 115200 baud:
`wifi <SSID>|<PASSWORD>`

Weather requires Wi-Fi. Cached values remain available offline. HTTPS uses `setInsecure()`: traffic is encrypted, but the server certificate is not validated.

## Hardware settings
Pin mapping, TFT inversion, touch transform and RGB LED settings are inherited unchanged from the tested v2.5/v2.6 project baseline. Do not change them unless the specific board requires it.

## Build status
Source prepared on branch `feature/petos-v2.7-nixi`. It has not been compiled in Arduino IDE or tested on physical hardware yet.
