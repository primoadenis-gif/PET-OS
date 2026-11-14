# PET OS v2.6 — Pet Companion

ESP32-2432S024R (2.4-inch CYD, ILI9341 + XPT2046), portrait 240x320.

- TOOLS and Focus Timer removed; replaced with PET.
- XP and companion levels removed.
- PET status: hunger, happiness, energy, health (0–100%).
- Actions: feed, pet, play, sleep.
- Pet needs decrease every five minutes; health is affected by prolonged low hunger or happiness.
- Weather fixed to Taganrog, Rostov region, Russia (47.2362, 38.8969), via Open-Meteo.
- Russian weather conditions; tap top-right weather icon for a Russian weather message.
- Wi-Fi, NTP, tasks, notes, RGB LED, timezone and touch calibration retained.

Libraries: TFT_eSPI, XPT2046_Touchscreen, ArduinoJson, ESP32 Arduino core.
Configure Wi-Fi via Serial Monitor: `wifi <SSID>|<PASSWORD>`.
Weather needs Wi-Fi; last successful values are cached. HTTPS currently uses setInsecure(), so the server certificate is not validated.
