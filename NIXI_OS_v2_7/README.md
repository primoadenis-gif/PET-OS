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


## Nixi sprite integration

The v2.7 sketch uses the Nixi sprite renderer without a room background:
- `NixiSprites.h` is required beside `NIXI_OS_v2_7.ino`.
- Replace the legacy `PetSprites.h` dependency; it is no longer included by the sketch.
- HOME renders only the 64x80 Nixi character on a solid dark background; the room artwork and room animation have been removed.
- Mood values map to the 8 expression sprites. Feed/pet/play/rest actions select the corresponding action animation.
- The character area is cleared before each transparent sprite frame to prevent animation trails. No room/background sprite is loaded or drawn.

Copy `NixiSprites.h` from the NIXI Sprite Pack v1.0 into this sketch folder before compiling. The header is a generated RGB565/PROGMEM asset file; the source pack also contains PNG sprite sheets and room previews.
