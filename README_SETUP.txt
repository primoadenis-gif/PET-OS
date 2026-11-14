PET OS v2.3 — HYBRID COMPANION FOUNDATION
ESP32-2432S024R (CYD 2.4", ILI9341 + XPT2046), portrait 240x320

FILES
- PET_OS_v2_3.ino
- PetSprites.h

INSTALL
1. Create a folder PET_OS_v2_3.
2. Put both files in that folder.
3. Open PET_OS_v2_3.ino in Arduino IDE.
4. Keep the currently working TFT_eSPI User_Setup for your board.
5. Install TFT_eSPI and XPT2046_Touchscreen; select your ESP32 board and COM port.
6. Compile and upload.

WHAT CHANGED FROM v2.2
- Reframed the device as an assistant, not a care-dependent pet. Hunger/energy/health decay is no longer run.
- HOME now shows the companion, date, status, and the next local task.
- Navigation: TASKS, TOOLS, MEMORY, SYSTEM.
- TASKS: three local checklist items, saved in ESP32 Preferences.
- TOOLS: 1-minute and 5-minute focus timers, with non-blocking millis() countdown.
- MEMORY: four short local note slots stored in Preferences.
- SYSTEM: connection status, local storage status, Wi-Fi connect action, manual save.
- Real Wi-Fi can be enabled by entering credentials in WIFI_SSID and WIFI_PASSWORD near the top of the sketch. Credentials are intentionally blank by default.
- After Wi-Fi connects, device requests NTP time and displays clock/date. This build uses UTC; timezone configuration can be added for the deployment region.
- Serial Monitor (115200 baud):
    note <text>  - save a note (max 70 chars)
    notes        - list saved notes
    del <n>      - delete note number n
    c            - RGB565 diagnostic palette
    h            - return home from color test

IMPORTANT LIMITATIONS
- This is a foundation release, not yet a complete AI assistant. No AI endpoint, cloud sync, weather API, speech input/output, or phone companion is implemented in this version.
- The touchscreen cannot enter arbitrary text yet; notes are entered through Serial Monitor. On-device keyboard is a planned follow-up.
- The timer is a simple focus countdown, not a full Pomodoro cycle or hardware alarm. It is visible while the Tools screen is open; no buzzer is assumed.
- Tasks are three starter checklist entries, not yet an editable task database or calendar.
- Wi-Fi credentials are stored in source constants in this prototype. Do not publish a sketch containing private credentials.
- Preferences namespace remains "petos" so existing device data can still be read; old pet fields are retained in the stored structure for compatibility, but no longer decay or drive care gameplay.

DISPLAY / TOUCH
- Keeps TFT_eSPI, ILI9341 rotation 0, invertDisplay(true), existing pin configuration, touch calibration, sprite sheet and 8 moods.
- Does not change TFT_eSPI RGB/BGR configuration.

TEST STATUS
Prepared from the supplied working v2.2 source. Arduino compilation and physical hardware testing have not been performed in this environment. Please retain your v2.2 ZIP as a rollback copy.
