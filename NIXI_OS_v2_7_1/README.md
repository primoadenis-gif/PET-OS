# NIXI OS v2.7.1 — sprite integration

Target: ESP32-2432S024R (CYD, ILI9341 + XPT2046), portrait 240x320.

## Base
This branch is based on `feature/petos-v2.7-nixi`. It preserves the v2.7 application logic and the sprite-only HOME screen (solid dark background; no room artwork).

## Sprite source pack
The source artwork was separated into a standalone pack from the user-provided atlas:
- full-body Nixi
- portrait expressions
- action poses
- 8 walking frames
- 2 running frames
- UI effects atlas

The room, furniture and interior are excluded. Source images are PNG RGBA with transparency. The pack contains 36 character sprites plus the UI effects atlas, a preview and a manifest.

## Integration status
- [x] Create an isolated v2.7.1 development branch.
- [x] Copy the v2.7 application sketch into `NIXI_OS_v2_7_1/`.
- [x] Keep the v2.7 branch untouched.
- [ ] Convert selected PNG assets into a compact ESP32-friendly RGB565 representation.
- [ ] Add the generated `NixiSprites.h` beside the sketch and verify all enum/function interfaces.
- [ ] Compile with the user's Arduino IDE board/partition settings (the user previously selected the 3 MB app / 1 MB SPIFFS layout).
- [ ] Hardware test on ESP32-2432S024R.

## Important
This is an integration work branch, not a verified firmware release yet. The copied sketch currently includes `NixiSprites.h`, but that generated header is not yet committed in this branch. Do not flash this branch until the header is added and the project compiles. No claim of compile or hardware validation is made.

## Libraries
- TFT_eSPI
- XPT2046_Touchscreen
- ArduinoJson
- ESP32 Arduino core
