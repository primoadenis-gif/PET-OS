/*
  NIXI OS v2.7 — Nixi Companion
  Board: ESP32-2432S024R (2.4" CYD, ILI9341 + XPT2046)
  Portrait UI: 240 x 320

  Libraries (Arduino Library Manager):
    - TFT_eSPI
    - XPT2046_Touchscreen

  Board package:
    - esp32 by Espressif Systems

  NIXI OS v2.7 — Base system
  - Editable tasks and notes via Serial Monitor
  - UTC offset selection in System (30-minute steps)
  - RGB LED status indication
  - Russian companion messages; English navigation labels
*/

#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "PetSprites.h"

// -------------------- BOARD PINS: ESP32-2432S024R --------------------
static constexpr int PIN_TFT_SCLK = 14;
static constexpr int PIN_TFT_MISO = 12;
static constexpr int PIN_TFT_MOSI = 13;
static constexpr int PIN_TFT_CS   = 15;
static constexpr int PIN_TFT_DC   = 2;
static constexpr int PIN_TFT_RST  = -1;
static constexpr int PIN_TFT_BL   = 27;
static constexpr int PIN_TOUCH_CS = 33;
static constexpr int PIN_TOUCH_IRQ = 36;

// On-board RGB LED
#define LED_RED     4
#define LED_GREEN   16
#define LED_BLUE    17
static constexpr bool RGB_LED_COMMON_ANODE = true;

// Shared SPI bus
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(PIN_TOUCH_CS, PIN_TOUCH_IRQ);
Preferences prefs;

// -------------------- TOUCH CALIBRATION --------------------
static constexpr int TOUCH_RAW_X_MIN = 275;
static constexpr int TOUCH_RAW_X_MAX = 3623;
static constexpr int TOUCH_RAW_Y_MIN = 264;
static constexpr int TOUCH_RAW_Y_MAX = 3532;
static constexpr bool TOUCH_SWAP_XY = true;
static constexpr bool TOUCH_INVERT_X = true;
static constexpr bool TOUCH_INVERT_Y = true;

// -------------------- THEME --------------------
#define C_BG       0x080D
#define C_PANEL    0x18B3
#define C_PANEL2   0x2127
#define C_CYAN     0x07FF
#define C_PINK     0xF81F
#define C_PURPLE   0xA1DF
#define C_BLUE     0x3D7F
#define C_GREEN    0xBFE0
#define C_YELLOW   0xFFE0
#define C_WHITE    0xFFFF
#define C_MUTED    0x9D7F
#define C_DARK     0x0000
#define C_RED      0xF986

enum Screen : uint8_t { HOME, TASKS, PET, MEMORY, SYSTEM };
enum Mood : uint8_t { HAPPY, CURIOUS, SLEEPY, SAD, EXCITED, LOVE, ANGRY, CONFUSED };
enum PetAction : uint8_t { ACT_IDLE, ACT_WAVE, ACT_DANCE, ACT_SLEEP, ACT_BLINK };

struct PetState {
  uint8_t hunger;
  uint8_t happiness;
  uint8_t energy;
  uint8_t health;
  uint32_t careCount;
  uint32_t lastSaved;
  bool taskWater;
  bool taskMove;
  bool taskBreak;
};

PetState pet;
Screen currentScreen = HOME;

static const uint8_t MAX_NOTES = 4;
String notes[MAX_NOTES];
uint8_t noteCount = 0;

static const uint8_t MAX_TASKS = 8;
struct TaskItem { String title; bool done; };
TaskItem tasks[MAX_TASKS];
uint8_t taskCount = 0;

int16_t utcOffsetMinutes = 0;
bool ledEnabled = true;
uint8_t ledBrightness = 90;
bool ledError = false;
bool resetArmed = false;
uint32_t resetArmUntil = 0;
uint8_t selectedNote = 0;

bool wifiConnecting = false;
uint32_t wifiAttemptAt = 0;

String clockText = "--:--";
String dateText = "--.--";
uint32_t lastClockDraw = 0;

// Credentials are stored in Preferences. Configure: wifi <SSID>|<PASSWORD>
String wifiSsid, wifiPassword;
bool ntpSynced=false;
uint32_t lastNtpCheck=0;
float weatherLat=0, weatherLon=0, weatherTemp=0;
int weatherCode=-1;
bool weatherLocationValid=false, weatherValid=false;
String weatherCity="", weatherText="WEATHER: OFFLINE";
uint32_t lastWeatherAttempt=0;
static constexpr uint32_t WEATHER_INTERVAL=30UL*60UL*1000UL;
static constexpr float TAGANROG_LAT=47.2362f, TAGANROG_LON=38.8969f;
static constexpr uint32_t PET_STAT_INTERVAL=5UL*60UL*1000UL;
uint32_t lastPetStatUpdate=0;
int touchRawXMin=TOUCH_RAW_X_MIN, touchRawXMax=TOUCH_RAW_X_MAX;
int touchRawYMin=TOUCH_RAW_Y_MIN, touchRawYMax=TOUCH_RAW_Y_MAX;
bool calibrationActive=false;
uint8_t calibrationStep=0;
int calMinX=4095,calMaxX=0,calMinY=4095,calMaxY=0;

Mood currentMood = HAPPY;
PetAction petAction = ACT_IDLE;

uint32_t bootMillis = 0;
uint32_t lastTick = 0;
uint32_t lastDraw = 0;
uint32_t lastPetAction = 0;
uint32_t actionUntil = 0;
uint32_t lastTouch = 0;
uint32_t lastSave = 0;
uint32_t lastMoodChange = 0;

bool dirty = true;
bool screenChanged = true;
bool petFrameDirty = false;
bool wifiEnabled = false;


String statusMessage = "РАД ТЕБЯ ВИДЕТЬ!";
uint32_t statusUntil = 0;

// -------------------- GRAPHICS HELPERS --------------------

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void drawPixelBlock(int x, int y, int w, int h, uint16_t color) {
  tft.fillRect(x, y, w, h, color);
}

void drawFrame(int x, int y, int w, int h, uint16_t color) {
  tft.drawRect(x, y, w, h, color);
  tft.drawPixel(x + 1, y + 1, color);
  tft.drawPixel(x + w - 2, y + h - 2, color);
}

void printCentered(const String &s, int x, int y, int w,
                   uint16_t color, uint16_t background,
                   uint8_t size=1) {
  tft.setTextSize(size);
  tft.setTextColor(color, background);
  const int textW = tft.textWidth(s);
  const int textX = x + (w - textW) / 2;
  tft.setCursor(textX, y);
  tft.print(s);
}

void drawWeatherIcon(int x,int y);
String moodName(Mood m);
String weatherReport();
Mood choosePetMood();
bool updateWeather();

void drawTopBar() {
  tft.fillRect(0, 0, 240, 31, C_BG);
  tft.drawFastHLine(8, 30, 224, C_PANEL2);

  tft.setTextSize(1);
  tft.setTextColor(C_PINK, C_BG);
  tft.setCursor(9, 12);
  tft.print("NIXI");

  tft.setTextColor(C_CYAN, C_BG);
  tft.print(" OS");

  tft.setTextSize(1);
  tft.setTextColor(C_CYAN, C_BG);
  tft.setCursor(72, 12);
  tft.print(clockText);
  tft.print("  ");
  tft.print(dateText);
  drawWeatherIcon(211,7);
}

void drawCyrillicCentered(const String &value, int x, int y,
                          int w, uint16_t fg, uint16_t bg);

void drawNeonButton(int x, int y, int w, int h,
                    const char *label, uint16_t color,
                    bool active=false) {
  uint16_t fill = active ? C_PANEL2 : C_BG;

  tft.fillRoundRect(x, y, w, h, 5, fill);
  tft.drawRoundRect(x, y, w, h, 5, color);
  tft.drawFastHLine(x + 5, y + 2, w - 10,
                    active ? C_PINK : color);

  drawCyrillicCentered(String(label), x, y + h/2 - 3, w,
                       active ? C_WHITE : color, fill);
}

void drawStatBar(int x, int y, const char *label,
                 uint8_t value, uint16_t color) {
  tft.setTextSize(1);
  tft.setTextColor(C_MUTED, C_BG);
  tft.setCursor(x, y);
  tft.print(label);

  tft.drawRect(x + 49, y - 1, 91, 9, C_PANEL2);

  int fillW = (value * 87) / 100;
  if (fillW > 0)
    tft.fillRect(x + 51, y + 1, fillW, 5, color);
}

void drawHeart(int x, int y, uint16_t c) {
  tft.fillRect(x+2,y,4,3,c);
  tft.fillRect(x+8,y,4,3,c);
  tft.fillRect(x,y+3,16,4,c);
  tft.fillRect(x+2,y+7,12,3,c);
  tft.fillRect(x+5,y+10,6,3,c);
  tft.fillRect(x+7,y+13,2,2,c);
}

// -------------------- PET SPRITE RENDERER --------------------

static constexpr uint16_t SPRITE_TRANSPARENT = 0x0001;
static uint8_t idleFrame = 0;
static uint32_t lastIdleFrame = 0;
static uint32_t idleCycle = 0;

void drawPet(int cx, int cy, uint8_t frame, Mood mood) {
  uint8_t m = (uint8_t)mood;
  if (m > 7) m = HAPPY;

  uint8_t f = frame & 3;
  const uint16_t *sprite = &PET_SPRITES[m][f][0];

  uint16_t runBuffer[48];

  const int left = cx - 24;
  const int top = cy - 24;

  for (int sy = 0; sy < 48; sy++) {
    int sx = 0;

    while (sx < 48) {
      while (sx < 48 &&
             pgm_read_word(&sprite[sy * 48 + sx]) == SPRITE_TRANSPARENT)
        sx++;

      if (sx >= 48) break;

      int runStart = sx;
      int runLength = 0;

      while (sx < 48) {
        uint16_t pixel = pgm_read_word(&sprite[sy * 48 + sx]);

        if (pixel == SPRITE_TRANSPARENT) break;

        runBuffer[runLength++] = pixel;
        sx++;
      }

      int drawX = left + runStart;
      int drawY = top + sy;

      if (drawX >= 0 && drawY >= 0 &&
          drawX + runLength <= tft.width() &&
          drawY < tft.height()) {
        tft.pushImage(drawX, drawY, runLength, 1, runBuffer);
      }
    }
  }
}

void drawRoomBackground() {
  tft.fillScreen(C_BG);
  tft.drawFastHLine(10, 229, 220, C_PANEL2);
  tft.drawPixel(8, 40, C_CYAN);
  tft.drawPixel(231, 40, C_PINK);
}

// -------------------- CYRILLIC FONT --------------------

static const uint8_t CYR_FONT[33][7] PROGMEM = {
  {14,17,17,31,17,17,17},
  {31,16,16,30,17,17,30},
  {30,17,17,30,17,17,30},
  {31,16,16,16,16,16,16},
  {14,10,10,10,31,17,17},
  {31,16,16,30,16,16,31},
  {10,0,31,16,30,16,31},
  {21,21,14,4,14,21,21},
  {14,17,1,6,1,17,14},
  {17,19,21,25,17,17,17},
  {10,0,17,19,21,25,17},
  {17,18,20,24,20,18,17},
  {7,9,9,9,17,17,17},
  {17,27,21,21,17,17,17},
  {17,17,31,17,17,17,17},
  {14,17,17,17,17,17,14},
  {31,17,17,17,17,17,17},
  {30,17,17,30,16,16,16},
  {14,17,16,16,16,17,14},
  {31,4,4,4,4,4,4},
  {17,17,17,15,1,17,14},
  {4,14,21,21,21,14,4},
  {17,17,10,4,10,17,17},
  {17,17,17,17,17,31,1},
  {17,17,17,15,1,1,1},
  {21,21,21,21,21,21,31},
  {21,21,21,21,21,31,1},
  {24,8,8,14,9,9,14},
  {17,17,29,19,21,21,29},
  {16,16,30,17,17,17,30},
  {14,17,1,7,1,17,14},
  {17,21,21,29,21,21,17},
  {15,17,17,15,5,9,17}
};

uint32_t utf8Next(const String &s, int &i) {
  uint8_t c = (uint8_t)s[i++];

  if (c < 0x80) return c;

  if ((c & 0xE0) == 0xC0 && i < s.length())
    return ((c & 0x1F) << 6) | ((uint8_t)s[i++] & 0x3F);

  if ((c & 0xF0) == 0xE0 && i+1 < s.length())
    return ((c & 0x0F) << 12) |
           (((uint8_t)s[i++] & 0x3F) << 6) |
           ((uint8_t)s[i++] & 0x3F);

  return '?';
}

uint8_t cyrIndex(uint32_t cp) {
  if (cp == 0x401 || cp == 0x451) return 6;

  if (cp >= 0x430 && cp <= 0x44F)
    cp -= 0x20;

  if (cp >= 0x410 && cp <= 0x415)
    return (uint8_t)(cp - 0x410);

  if (cp >= 0x416 && cp <= 0x42F)
    return (uint8_t)(cp - 0x410 + 1);

  return 255;
}

uint16_t drawCyrillicText(const String &value, int x, int y,
                          uint16_t fg, uint16_t bg,
                          int maxChars=40) {
  int cursor = x;
  int count = 0;

  for (int i=0; i<value.length() && count<maxChars;) {
    uint32_t cp = utf8Next(value, i);
    uint8_t gi = cyrIndex(cp);

    if (gi != 255) {
      for (int row=0; row<7; row++) {
        uint8_t bits = pgm_read_byte(&CYR_FONT[gi][row]);

        for (int col=0; col<5; col++) {
          tft.drawPixel(cursor+col, y+row,
                        (bits & (1 << (4-col))) ? fg : bg);
        }
      }
    } else if (cp < 128) {
      tft.drawChar(cursor, y, (uint16_t)cp, fg, bg, 1);
    } else {
      tft.drawChar(cursor, y, '?', fg, bg, 1);
    }

    cursor += 6;
    count++;
  }

  return cursor-x;
}

void drawCyrillicCentered(const String &value, int x, int y,
                          int w, uint16_t fg, uint16_t bg) {
  int chars = 0;

  for (int i=0; i<value.length();) {
    utf8Next(value, i);
    chars++;
  }

  int width = chars * 6;
  int start = x + (w-width)/2;

  drawCyrillicText(value, start, y, fg, bg, chars);
}

// -------------------- HOME SCREEN --------------------

void drawHomeDynamic() {
  drawTopBar();

  tft.fillRoundRect(8, 43, 224, 35, 5, C_PANEL);
  tft.drawRoundRect(8, 43, 224, 35, 5, C_PURPLE);

  drawCyrillicCentered(statusMessage, 12, 56, 216,
                       C_WHITE, C_PANEL);
  tft.fillRect(95, 125, 48, 48, C_BG);
  drawPet(119, 149, idleFrame, currentMood);

  tft.fillRoundRect(8, 190, 224, 43, 5, C_PANEL);
  tft.drawRoundRect(8, 190, 224, 43, 5, C_GREEN);

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED, C_PANEL);
  tft.setCursor(17, 199);
  tft.print("NEXT TASK");

  tft.setTextColor(C_WHITE, C_PANEL);
  tft.setCursor(17, 214);

  String next = "ВСЁ ВЫПОЛНЕНО";

  for (uint8_t i=0; i<taskCount; ++i) {
    if (!tasks[i].done) {
      next = tasks[i].title;
      break;
    }
  }

  if (next.length() > 58)
    next = next.substring(0,58);

  drawCyrillicText(next,17,214,C_WHITE,C_PANEL,34);

  tft.fillRect(8, 237, 224, 14, C_BG);

  tft.setTextColor(C_MUTED,C_BG); tft.setCursor(10,240); tft.print("СОСТОЯНИЕ:");
  drawCyrillicText(moodName(currentMood),82,240,C_CYAN,C_BG,24);
}

void drawHome() {
  drawRoomBackground();
  drawTopBar();

  drawNeonButton(5, 260, 44, 43, "TASK", C_CYAN, true);
  drawNeonButton(52, 260, 44, 43, "NIXI", C_PINK);
  drawNeonButton(99, 260, 66, 43, "MEMORY", C_PURPLE);
  drawNeonButton(168, 260, 66, 43, "SYSTEM", C_GREEN);

  drawHomeDynamic();
}

// -------------------- TASKS SCREEN --------------------

void drawTasks() {
  tft.fillScreen(C_BG);
  drawTopBar();

  tft.setTextColor(C_PINK, C_BG);
  tft.setTextSize(2);
  tft.setCursor(12, 42);
  tft.print("TASKS");

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED, C_BG);
  tft.setCursor(12, 67);
  tft.print("TAP TO COMPLETE / SERIAL TO EDIT");

  for (uint8_t i=0; i<MAX_TASKS; ++i) {
    int y = 83+i*23;

    if (i<taskCount) {
      tft.drawRoundRect(10,y,220,20,4,
                        tasks[i].done ? C_GREEN : C_PANEL2);

      tft.fillRect(17,y+6,9,9,
                   tasks[i].done ? C_GREEN : C_BG);

      if (tasks[i].done) {
        tft.drawLine(18,y+10,21,y+13,C_BG);
        tft.drawLine(21,y+13,25,y+7,C_BG);
      }

      tft.setTextColor(tasks[i].done ? C_GREEN : C_WHITE,C_BG);

      String label = String(i+1)+". "+tasks[i].title;

      drawCyrillicText(label,33,y+7,
                       tasks[i].done ? C_GREEN : C_WHITE,
                       C_BG,32);
    } else {
      tft.drawRoundRect(10,y,220,20,4,C_PANEL2);
      tft.setTextColor(C_MUTED,C_BG);
      tft.setCursor(18,y+7);
      tft.print("EMPTY SLOT");
    }
  }

  drawNeonButton(8, 282, 65, 28, "BACK", C_PINK);

  tft.setTextColor(C_MUTED, C_BG);
  tft.setCursor(83, 291);
  tft.print("Serial: add / edit / del");
}

// -------------------- SYSTEM / SETTINGS --------------------

String formatUtcOffset() {
  int v = utcOffsetMinutes;

  char sign = (v<0) ? '-' : '+';
  int a = abs(v);

  char b[16];
  snprintf(b,sizeof(b),"UTC %c%02d:%02d",
           sign,a/60,a%60);

  return String(b);
}

void drawSystem() {
  tft.fillScreen(C_BG);
  drawTopBar();

  tft.setTextColor(C_GREEN,C_BG);
  tft.setTextSize(2);
  tft.setCursor(12,40);
  tft.print("SYSTEM");

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED,C_BG);
  tft.setCursor(14,66);
  tft.print("HYBRID CONNECTION STATUS");

  tft.drawRoundRect(10,82,220,34,5,C_PANEL2);

  tft.setTextColor(C_WHITE,C_BG);
  tft.setCursor(19,95);
  tft.print("WI-FI");

  tft.setTextColor(
    WiFi.status()==WL_CONNECTED ? C_GREEN : C_YELLOW,
    C_BG
  );

  tft.setCursor(145,95);
  tft.print(
    WiFi.status()==WL_CONNECTED ? "CONNECTED" : "OFFLINE"
  );

  tft.drawRoundRect(10,122,220,43,5,C_PANEL2);

  tft.setTextColor(C_WHITE,C_BG);
  tft.setCursor(18,139);
  tft.print("TIME ZONE");

  tft.setTextColor(C_CYAN,C_BG);
  tft.setCursor(92,139);
  tft.print(formatUtcOffset());

  drawNeonButton(174,126,24,34,"-",C_PINK);
  drawNeonButton(202,126,24,34,"+",C_CYAN);

  tft.drawRoundRect(10,171,220,32,5,C_PANEL2);

  tft.setTextColor(C_WHITE,C_BG);
  tft.setCursor(19,183);
  tft.print("RGB LED");

  tft.setTextColor(ledEnabled ? C_GREEN : C_MUTED,C_BG);
  tft.setCursor(155,183);
  tft.print(ledEnabled ? "ON" : "OFF");

  drawNeonButton(10,210,106,32,"CONNECT WIFI",C_PURPLE);
  drawNeonButton(124,210,106,32,"SAVE DATA",C_GREEN);

  drawNeonButton(10,249,106,28,
                 resetArmed ? "CONFIRM RESET" : "RESET DEVICE",C_RED);
  drawNeonButton(124,249,106,28,"TOUCH CAL",C_CYAN);
  drawNeonButton(8,282,65,28,"BACK",C_PINK);

  tft.setTextColor(C_MUTED,C_BG);
  tft.setCursor(123,291);
  tft.print("UTC OFFSET / NO DST");
  tft.setTextColor(C_CYAN,C_BG);
  tft.setCursor(19,107);
  tft.print(ntpSynced ? "NTP SYNCED" : "NTP WAITING");
}

// -------------------- PET SCREEN --------------------
void drawPetScreen() {
  tft.fillScreen(C_BG); drawTopBar();
  tft.setTextColor(C_PINK,C_BG); tft.setTextSize(2); tft.setCursor(12,40); tft.print("NIXI");
  drawStatBar(14,78,"СЫТОСТЬ",pet.hunger,C_YELLOW);
  tft.setTextColor(C_WHITE,C_BG); tft.setCursor(169,78); tft.printf("%u%%",pet.hunger);
  drawStatBar(14,112,"НАСТР.",pet.happiness,C_PINK);
  tft.setTextColor(C_WHITE,C_BG); tft.setCursor(169,112); tft.printf("%u%%",pet.happiness);
  drawStatBar(14,146,"ЭНЕРГ.",pet.energy,C_CYAN);
  tft.setTextColor(C_WHITE,C_BG); tft.setCursor(169,146); tft.printf("%u%%",pet.energy);
  drawStatBar(14,180,"ЗДОРОВ.",pet.health,C_GREEN);
  tft.setTextColor(C_WHITE,C_BG); tft.setCursor(169,180); tft.printf("%u%%",pet.health);
  drawNeonButton(10,211,105,30,"КОРМИТЬ",C_YELLOW);
  drawNeonButton(125,211,105,30,"ЛАСКА",C_PINK);
  drawNeonButton(10,247,105,30,"ИГРАТЬ",C_CYAN);
  drawNeonButton(125,247,105,30,"ОТДЫХ",C_PURPLE);
  drawNeonButton(8,282,65,28,"BACK",C_PINK);
  tft.setTextColor(C_MUTED,C_BG); tft.setCursor(82,291); tft.print("ЗАБОТА О NIXI");
}

// -------------------- MEMORY SCREEN --------------------

void drawMemory() {
  tft.fillScreen(C_BG);
  drawTopBar();

  tft.setTextColor(C_PURPLE, C_BG);
  tft.setTextSize(2);
  tft.setCursor(12, 43);
  tft.print("MEMORY");

  tft.setTextSize(1);
  tft.setTextColor(C_MUTED, C_BG);
  tft.setCursor(12, 72);
  tft.print("LOCAL NOTES / SERIAL INPUT");

  for (uint8_t i=0; i<MAX_NOTES; i++) {
    int y = 91+i*43;

    tft.drawRoundRect(10,y,220,35,5,
                      i<noteCount ? C_CYAN : C_PANEL2);

    tft.setTextColor(i<noteCount ? C_WHITE : C_MUTED,C_BG);
    tft.setCursor(19,y+13);

    if (i<noteCount) {
      tft.print(i+1);
      tft.print(". ");

      String v = notes[i];

      if (v.length()>27)
        v = v.substring(0,27);

      tft.print(v);
    } else {
      tft.print("EMPTY NOTE SLOT");
    }
  }

  drawNeonButton(8, 282, 65, 28, "BACK", C_PINK);

  tft.setTextColor(C_MUTED,C_BG);
  tft.setCursor(83,291);
  tft.print("Serial: note your text");
}

// -------------------- SCREEN ROUTER --------------------

void drawScreen() {
  switch (currentScreen) {
    case HOME: drawHome(); break;
    case TASKS: drawTasks(); break;
    case PET: drawPetScreen(); break;
    case MEMORY: drawMemory(); break;
    case SYSTEM: drawSystem(); break;
  }

  dirty = false;
}

// -------------------- MESSAGES --------------------

void setMessage(const String &msg, uint32_t duration=3500) {
  statusMessage = msg;
  statusUntil = millis() + duration;
  dirty = true;
}

// -------------------- SAVE / LOAD --------------------

void savePet() {
  prefs.begin("petos", false);

  prefs.putUChar("hunger", pet.hunger);
  prefs.putUChar("happy", pet.happiness);
  prefs.putUChar("energy", pet.energy);
  prefs.putUChar("health", pet.health);

  prefs.putUInt("care", pet.careCount);

  prefs.putBool("water", pet.taskWater);
  prefs.putBool("move", pet.taskMove);
  prefs.putBool("break", pet.taskBreak);

  prefs.putUChar("noteCount", noteCount);

  for (uint8_t i=0; i<MAX_NOTES; i++) {
    String k = "n"+String(i);
    prefs.putString(k.c_str(), notes[i]);
  }

  prefs.putUChar("taskCount", taskCount);

  for (uint8_t i=0; i<MAX_TASKS; i++) {
    String k = "t"+String(i);
    String d = "d"+String(i);

    prefs.putString(k.c_str(),tasks[i].title);
    prefs.putBool(d.c_str(),tasks[i].done);
  }

  prefs.putShort("tzMin",utcOffsetMinutes);
  prefs.putBool("ledOn",ledEnabled);
  prefs.putUChar("ledBright",ledBrightness);

  prefs.end();

  lastSave = millis();
}

void loadPet() {
  prefs.begin("petos", true);

  bool exists = prefs.isKey("hunger");

  if (exists) {
    pet.hunger = prefs.getUChar("hunger",75);
    pet.happiness = prefs.getUChar("happy",80);
    pet.energy = prefs.getUChar("energy",90);
    pet.health = prefs.getUChar("health",100);

    pet.careCount = prefs.getUInt("care",0);

    pet.taskWater = prefs.getBool("water",false);
    pet.taskMove = prefs.getBool("move",false);
    pet.taskBreak = prefs.getBool("break",false);

    noteCount = min((uint8_t)MAX_NOTES,
                    prefs.getUChar("noteCount",0));

    for (uint8_t i=0; i<MAX_NOTES; i++) {
      String k = "n"+String(i);
      notes[i] = prefs.getString(k.c_str(),"");
    }
  } else {
    pet = {75,85,90,100,0,0,false,false,false};
  }

  taskCount = min((uint8_t)MAX_TASKS,
                  prefs.getUChar("taskCount",0));

  for (uint8_t i=0; i<MAX_TASKS; i++) {
    String k = "t"+String(i);
    String d = "d"+String(i);

    tasks[i].title = prefs.getString(k.c_str(),"");
    tasks[i].done = prefs.getBool(d.c_str(),false);
  }

  utcOffsetMinutes =
    constrain((int)prefs.getShort("tzMin",0),-720,840);

  ledEnabled = prefs.getBool("ledOn",true);
  ledBrightness = prefs.getUChar("ledBright",90);

  prefs.end();

  if (taskCount==0) {
    tasks[0] = {"Выпить воды",false};
    tasks[1] = {"Немного подвигаться",false};
    tasks[2] = {"Сделать перерыв",false};

    taskCount = 3;
  }
}

void resetToDefaults() {
  prefs.begin("petos",false);
  prefs.clear();
  prefs.end();
  prefs.begin("petcal",false);prefs.clear();prefs.end();
  prefs.begin("petwx",false);prefs.clear();prefs.end();
  weatherValid=false;weatherLocationValid=false;
  weatherCode=-1;weatherText="WEATHER: OFFLINE";
  touchRawXMin=TOUCH_RAW_X_MIN;touchRawXMax=TOUCH_RAW_X_MAX;
  touchRawYMin=TOUCH_RAW_Y_MIN;touchRawYMax=TOUCH_RAW_Y_MAX;

  pet = {75,85,90,100,0,0,false,false,false};

  for (uint8_t i=0; i<MAX_NOTES; i++)
    notes[i] = "";

  noteCount = 0;

  for (uint8_t i=0; i<MAX_TASKS; i++) {
    tasks[i].title = "";
    tasks[i].done = false;
  }

  tasks[0] = {"Выпить воды",false};
  tasks[1] = {"Немного подвигаться",false};
  tasks[2] = {"Сделать перерыв",false};

  taskCount = 3;

  utcOffsetMinutes = 0;
  ledEnabled = true;
  ledBrightness = 90;

  wifiConnecting = false;
  ledError = false;

  if (WiFi.status()==WL_CONNECTED)
    WiFi.disconnect(true);

  WiFi.mode(WIFI_OFF);
  wifiEnabled = false;
  wifiSsid="";wifiPassword="";saveNetworkSettings();

  savePet();

  setMessage("НАСТРОЙКИ СБРОШЕНЫ");

  currentScreen = HOME;
  screenChanged = true;
  dirty = true;
}

// -------------------- TOUCH --------------------

bool readTouch(int &x, int &y) {
  if (!touch.touched())
    return false;

  TS_Point p = touch.getPoint();

  int rx = p.x;
  int ry = p.y;

  if (TOUCH_SWAP_XY) {
    int tmp = rx;
    rx = ry;
    ry = tmp;
  }

  int sx = map(rx, touchRawXMin, touchRawXMax, 0, 239);
  int sy = map(ry, touchRawYMin, touchRawYMax, 0, 319);

  if (TOUCH_INVERT_X) sx = 239-sx;
  if (TOUCH_INVERT_Y) sy = 319-sy;

  x = constrain(sx,0,239);
  y = constrain(sy,0,319);

  return true;
}

// -------------------- RGB LED --------------------

void writeRgb(uint8_t r,uint8_t g,uint8_t b) {
  if (!ledEnabled)
    r = g = b = 0;

  auto cv=[](uint8_t v)->uint8_t {
    return RGB_LED_COMMON_ANODE ? 255-v : v;
  };

  uint8_t rr=cv((uint16_t)r*ledBrightness/255);
  uint8_t gg=cv((uint16_t)g*ledBrightness/255);
  uint8_t bb=cv((uint16_t)b*ledBrightness/255);
  static int oldR=-1,oldG=-1,oldB=-1;
  if(rr==oldR&&gg==oldG&&bb==oldB)return;
  oldR=rr;oldG=gg;oldB=bb;
  analogWrite(LED_RED,rr);analogWrite(LED_GREEN,gg);analogWrite(LED_BLUE,bb);
}

void updateRgbLed() {
  if (!ledEnabled) {
    writeRgb(0,0,0);
    return;
  }

  if (ledError) {
    writeRgb(255,0,0);
    return;
  }

  if (wifiConnecting) {
    writeRgb(0,0,255);
    return;
  }

  if (WiFi.status()==WL_CONNECTED) {
    writeRgb(0,255,0);
    return;
  }

  bool pending = false;

  for (uint8_t i=0; i<taskCount; i++) {
    if (!tasks[i].done)
      pending = true;
  }

  if (pending)
    writeRgb(255,100,0);
  else
    writeRgb(0,180,200);
}

// -------------------- TIME ZONE --------------------

void changeUtcOffset(int delta) {
  utcOffsetMinutes =
    constrain((int)utcOffsetMinutes+delta,-720,840);

  savePet();
  dirty = true;

  setMessage("ПОЯС СОХРАНЁН");
}

// -------------------- TOUCH HANDLERS --------------------

void handleHomeTouch(int x, int y) {
  if (y>=260 && y<=306) {
    if (x<50)
      currentScreen = TASKS;
    else if (x<98)
      currentScreen = PET;
    else if (x<166)
      currentScreen = MEMORY;
    else
      currentScreen = SYSTEM;

    screenChanged = true;
    dirty = true;
    return;
  }

  if (y>105 && y<180) {
    currentMood = HAPPY;
    idleFrame = 0;

    setMessage("Я ГОТОВ ПОМОЧЬ!");

    pet.careCount++;

    dirty = true;
  }
}

void handleTouch(int x,int y) {
  if(y<=30 && x>=202){setMessage(weatherReport(),5000);if(weatherValid)currentMood=CURIOUS;else if(WiFi.status()==WL_CONNECTED)updateWeather();dirty=true;return;}
  if (currentScreen==HOME) {
    handleHomeTouch(x,y);
    return;
  }

  if (currentScreen==TASKS) {
    if (y>=282 && x<85) {
      currentScreen = HOME;
      screenChanged = true;
      dirty = true;
      return;
    }

    if (y>=83 && y<267) {
      uint8_t i = (y-83)/23;

      if (i<taskCount) {
        tasks[i].done = !tasks[i].done;

        if (tasks[i].done) {
          setMessage("ЗАДАЧА ВЫПОЛНЕНА!");
        } else {
          setMessage("ЗАДАЧА ВОЗВРАЩЕНА");
        }

        savePet();
        dirty = true;
      }
    }

    return;
  }

  if (currentScreen==PET) {
    if(y>=282 && x<85){currentScreen=HOME;screenChanged=true;dirty=true;return;}
    if(y>=211 && y<=241){
      if(x<120){pet.hunger=min(100,(int)pet.hunger+28);pet.happiness=min(100,(int)pet.happiness+4);pet.energy=max(0,(int)pet.energy-2);currentMood=LOVE;setMessage("НЯМ-НЯМ! СПАСИБО!");}
      else{pet.happiness=min(100,(int)pet.happiness+18);currentMood=LOVE;setMessage("МУРРР... КАК ПРИЯТНО!");}
      pet.careCount++;savePet();dirty=true;return;
    }
    if(y>=247 && y<=277){
      if(x<120){pet.happiness=min(100,(int)pet.happiness+20);pet.energy=max(0,(int)pet.energy-15);pet.hunger=max(0,(int)pet.hunger-5);currentMood=EXCITED;setMessage("УРА! ДАВАЙ ЕЩЁ!");}
      else{pet.energy=min(100,(int)pet.energy+30);pet.happiness=min(100,(int)pet.happiness+2);currentMood=SLEEPY;setMessage("СЛАДКИХ СНОВ...");}
      pet.careCount++;savePet();dirty=true;return;
    }
  }

  if (currentScreen==MEMORY) {
    if (y>=282 && x<85) {
      currentScreen = HOME;
      screenChanged = true;
      dirty = true;
      return;
    }

    if (y>=91 && y<263) {
      uint8_t i = (y-91)/43;

      if (i<noteCount) {
        selectedNote = i;
        setMessage("СМ. SERIAL MONITOR");
      }
    }
  }

  if (currentScreen==SYSTEM) {
    if (y>=282 && x<85) {
      currentScreen = HOME;
      screenChanged = true;
      dirty = true;
      return;
    }

    if (y>=126 && y<=160) {
      if (x>=170 && x<200)
        changeUtcOffset(-30);
      else if (x>=200)
        changeUtcOffset(30);

      return;
    }

    if (y>=171 && y<=203) {
      ledEnabled = !ledEnabled;
      savePet();
      updateRgbLed();
      dirty = true;
      return;
    }

    if (y>=210 && y<=242) {
      if (x<120) {
        beginWifiConnection();
      } else {
        savePet();
        setMessage("ДАННЫЕ СОХРАНЕНЫ");
      }

      dirty = true;
      return;
    }

    if (y>=249 && y<=278 && x>=120) {
      startTouchCalibration();
      return;
    }

    if (y>=249 && y<=278 && x<120) {
      if (!resetArmed || millis()>resetArmUntil) {
        resetArmed = true;
        resetArmUntil = millis()+5000;

        setMessage("НАЖМИТЕ ЕЩЁ РАЗ");
        dirty = true;
      } else {
        resetArmed = false;
        resetToDefaults();
      }

      return;
    }
  }
}

// -------------------- MAIN UPDATE --------------------

void updatePet() {
  uint32_t now = millis();

  if (now-lastSave>300000UL)
    savePet();

  if (wifiConnecting) {
    if (WiFi.status()==WL_CONNECTED) {
      wifiConnecting = false;
      ledError = false;

      configTime(0,0,"pool.ntp.org","time.nist.gov");
      lastNtpCheck=now;
      ntpSynced=false;
      setMessage("WI-FI ПОДКЛЮЧЁН");
      dirty = true;
    } else if (now-wifiAttemptAt>15000UL) {
      wifiConnecting = false;
      ledError = true;

      setMessage("НЕТ СВЯЗИ WI-FI");
      dirty = true;
    }
  }

  if (WiFi.status()==WL_CONNECTED && now-lastNtpCheck>5000UL) {
    lastNtpCheck=now;
    ntpSynced=time(nullptr)>1700000000;
    if (ntpSynced && lastWeatherAttempt==0) lastWeatherAttempt=now-WEATHER_INTERVAL;
  }

  if (WiFi.status()==WL_CONNECTED &&
      now-lastClockDraw>1000UL) {
    lastClockDraw = now;

    time_t utc = time(nullptr);

    if (utc>100000) {
      time_t local =
        utc+(time_t)utcOffsetMinutes*60;

      struct tm ti;

      gmtime_r(&local,&ti);

      char tb[8];
      char db[8];

      strftime(tb,sizeof(tb),"%H:%M",&ti);
      strftime(db,sizeof(db),"%d.%m",&ti);

      clockText = tb;
      dateText = db;

      if (currentScreen==HOME)
        drawTopBar();
    }
  }

  if (statusUntil && now>statusUntil) {
    statusUntil = 0;
    statusMessage = "ГОТОВ ПОМОЧЬ";
    dirty = true;
  }

  if (resetArmed && now>resetArmUntil) {
    resetArmed = false;

    if (currentScreen==SYSTEM)
      dirty = true;
  }

  if(now-lastPetStatUpdate>=PET_STAT_INTERVAL){
    lastPetStatUpdate=now; pet.hunger=pet.hunger?pet.hunger-1:0; pet.happiness=pet.happiness?pet.happiness-1:0; pet.energy=pet.energy?pet.energy-1:0;
    if(pet.hunger<20||pet.happiness<15)pet.health=pet.health?pet.health-1:0;
    else if(pet.hunger>65&&pet.happiness>60&&pet.energy>50)pet.health=min(100,(int)pet.health+1);
    currentMood=choosePetMood(); savePet(); dirty=true;
  }
  if(currentScreen==PET && now-lastIdleFrame>=1000UL){lastIdleFrame=now;dirty=true;}
  if (currentScreen==HOME &&
      now-lastIdleFrame>=260UL) {
    lastIdleFrame = now;
    idleFrame = (idleFrame+1)&3;
    petFrameDirty = true;
  }

  if (WiFi.status()==WL_CONNECTED && ntpSynced &&
      now-lastWeatherAttempt>=WEATHER_INTERVAL) {
    lastWeatherAttempt=now;
    updateWeather();
  }
  updateRgbLed();
}

// -------------------- NETWORK / WEATHER / CALIBRATION --------------------

String weatherDescription(int c){
 if(c==0)return "ЯСНО"; if(c==1)return "ПРЕИМУЩЕСТВЕННО ЯСНО"; if(c==2)return "ПЕРЕМЕННАЯ ОБЛАЧНОСТЬ"; if(c==3)return "ОБЛАЧНО";
 if(c==45||c==48)return "ТУМАН"; if(c>=51&&c<=55)return "МОРОСЬ"; if(c>=56&&c<=57)return "ЛЕДЯНАЯ МОРОСЬ";
 if(c>=61&&c<=65)return "ДОЖДЬ"; if(c>=66&&c<=67)return "ЛЕДЯНОЙ ДОЖДЬ"; if(c>=71&&c<=77)return "СНЕГ";
 if(c>=80&&c<=82)return "ЛИВЕНЬ"; if(c>=85&&c<=86)return "СНЕЖНЫЕ ЗАРЯДЫ"; if(c==95)return "ГРОЗА"; if(c>=96)return "СИЛЬНАЯ ГРОЗА"; return "НЕТ ДАННЫХ";
}
String moodName(Mood m){switch(m){case HAPPY:return "РАДОСТНЫЙ";case CURIOUS:return "ЛЮБОПЫТНЫЙ";case SLEEPY:return "СОННЫЙ";case SAD:return "ГРУСТНЫЙ";case EXCITED:return "ВОСТОРЖЕННЫЙ";case LOVE:return "ЛАСКОВЫЙ";case ANGRY:return "СЕРДИТЫЙ";default:return "СМУЩЁННЫЙ";}}
Mood choosePetMood(){if(pet.health<25||pet.hunger<15||pet.happiness<15)return SAD;if(pet.energy<20)return SLEEPY;if(pet.happiness>85)return LOVE;if(pet.hunger>70&&pet.happiness>60)return HAPPY;return CURIOUS;}
void drawWeatherIcon(int x,int y){uint16_t c=weatherValid?C_CYAN:C_MUTED;tft.drawCircle(x+6,y+5,3,weatherValid?C_YELLOW:c);tft.drawFastHLine(x+3,y+10,14,c);tft.drawFastHLine(x+2,y+11,16,c);tft.drawPixel(x+4,y+8,c);tft.drawPixel(x+5,y+7,c);tft.drawPixel(x+11,y+8,c);tft.drawPixel(x+12,y+7,c);}
String weatherReport(){if(!weatherValid)return "ПОГОДА В ТАГАНРОГЕ НЕДОСТУПНА";return "ТАГАНРОГ: "+weatherDescription(weatherCode)+" "+String((int)lroundf(weatherTemp))+"C";}

void saveNetworkSettings() {
  prefs.begin("petnet",false);
  prefs.putString("ssid",wifiSsid);
  prefs.putString("pass",wifiPassword);
  prefs.end();
}
void loadNetworkSettings() {
  prefs.begin("petnet",true);
  wifiSsid=prefs.getString("ssid","");
  wifiPassword=prefs.getString("pass","");
  prefs.end();
  prefs.begin("petcal",true);
  touchRawXMin=prefs.getInt("xmin",TOUCH_RAW_X_MIN);
  touchRawXMax=prefs.getInt("xmax",TOUCH_RAW_X_MAX);
  touchRawYMin=prefs.getInt("ymin",TOUCH_RAW_Y_MIN);
  touchRawYMax=prefs.getInt("ymax",TOUCH_RAW_Y_MAX);
  prefs.end();
}
void beginWifiConnection() {
  if(!wifiSsid.length()) {
    Serial.println("Configure Wi-Fi: wifi <SSID>|<PASSWORD>");
    setMessage("НАСТРОЙ WI-FI В SERIAL"); dirty=true; return;
  }
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
  WiFi.begin(wifiSsid.c_str(),wifiPassword.c_str());
  wifiConnecting=true; wifiAttemptAt=millis(); ledError=false;
  setMessage("ПОДКЛЮЧАЮ WI-FI..."); dirty=true;
}
bool httpsGetJson(const String &url,JsonDocument &doc,const char *stage) {
  WiFiClientSecure client;
  client.setInsecure(); // HTTPS encrypted; server certificate is not verified.
  HTTPClient http;
  http.setTimeout(12000);
  http.setConnectTimeout(12000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  Serial.printf("[WEATHER] %s: connecting...\\n",stage);
  if(!http.begin(client,url)) {
    Serial.printf("[WEATHER] %s: http.begin() failed\\n",stage);
    return false;
  }

  int code=http.GET();
  Serial.printf("[WEATHER] %s: HTTP status %d\\n",stage,code);
  if(code!=HTTP_CODE_OK) {
    if(code<0) Serial.printf("[WEATHER] %s: %s\\n",stage,http.errorToString(code).c_str());
    String body=http.getString();
    if(body.length()) Serial.println(body.substring(0,240));
    http.end();
    return false;
  }

  String body=http.getString();
  http.end();
  DeserializationError err=deserializeJson(doc,body);
  if(err) {
    Serial.printf("[WEATHER] %s: JSON parse failed: %s\\n",stage,err.c_str());
    Serial.println(body.substring(0,240));
    return false;
  }
  return true;
}

bool updateWeather() {
  if(WiFi.status()!=WL_CONNECTED) {
    Serial.printf("[WEATHER] Wi-Fi not connected, status=%d\\n",(int)WiFi.status());
    return false;
  }

  weatherLat=TAGANROG_LAT; weatherLon=TAGANROG_LON; weatherCity="Таганрог"; weatherLocationValid=true;
  Serial.printf("[WEATHER] Fixed location: Taganrog | lat=%.5f lon=%.5f\\n",weatherLat,weatherLon);
  String url="https://api.open-meteo.com/v1/forecast?latitude="+String(TAGANROG_LAT,4)+
    "&longitude="+String(TAGANROG_LON,4)+"&current=temperature_2m,weather_code&timezone=auto";
  JsonDocument wx;
  if(!httpsGetJson(url,wx,"OPEN-METEO")) {
    if(!weatherValid) weatherText="WEATHER: API REQUEST FAILED";
    dirty=true; return false;
  }

  if(wx["error"].as<bool>()) {
    const char *reason=wx["reason"]|"unknown API error";
    Serial.printf("[WEATHER] Open-Meteo rejected request: %s\\n",reason);
    if(!weatherValid) weatherText="WEATHER: API REJECTED";
    dirty=true; return false;
  }

  String wxDebug; serializeJson(wx,wxDebug);
  Serial.println("[WEATHER] API response: "+wxDebug.substring(0,500));
  Serial.printf("[WEATHER] API coordinates: lat=%.5f lon=%.5f\\n",wx["latitude"].as<float>(),wx["longitude"].as<float>());
  JsonObject cur=wx["current"];
  if(cur.isNull()) {
    Serial.println("[WEATHER] Open-Meteo response has no 'current' object.");
    if(!weatherValid) weatherText="WEATHER: NO CURRENT DATA";
    dirty=true; return false;
  }

  if(cur["temperature_2m"].isNull()) Serial.println("[WEATHER] WARNING: current.temperature_2m is missing.");
  if(cur["weather_code"].isNull()) Serial.println("[WEATHER] WARNING: current.weather_code is missing.");
  weatherTemp=cur["temperature_2m"]|NAN;
  weatherCode=cur["weather_code"]|-1;
  Serial.printf("[WEATHER] Parsed current: temp=%.2f code=%d time=%s\\n",weatherTemp,weatherCode,(const char*)(cur["time"]|"unknown"));
  weatherValid=weatherCode>=0;
  if(!weatherValid) {
    Serial.println("[WEATHER] Missing or invalid current.weather_code.");
    if(!weatherValid) weatherText="WEATHER: BAD API DATA";
    dirty=true; return false;
  }

  weatherText="ТАГАНРОГ "+String(weatherTemp,1)+" C "+weatherDescription(weatherCode);
  prefs.begin("petwx",false);
  prefs.putFloat("lat",weatherLat); prefs.putFloat("lon",weatherLon);
  prefs.putString("city",weatherCity); prefs.putFloat("temp",weatherTemp);
  prefs.putInt("code",weatherCode); prefs.end();
  dirty=true;
  Serial.println("[WEATHER] Updated: "+weatherText);
  return true;
}
void loadCachedWeather() {
  prefs.begin("petwx",true);
  weatherLat=prefs.getFloat("lat",0); weatherLon=prefs.getFloat("lon",0);
  weatherCity=prefs.getString("city",""); weatherTemp=prefs.getFloat("temp",0);
  weatherCode=prefs.getInt("code",-1); prefs.end();
  weatherLat=TAGANROG_LAT;weatherLon=TAGANROG_LON;weatherCity="Таганрог";weatherLocationValid=true;
  weatherValid=weatherCode>=0;
  if(weatherValid)weatherText="ТАГАНРОГ "+String(weatherTemp,1)+" C "+weatherDescription(weatherCode);
}
void drawCalibrationTarget() {
  tft.fillScreen(C_BG); tft.setTextSize(1); tft.setTextColor(C_WHITE,C_BG);
  tft.setCursor(12,12); tft.print("TOUCH CALIBRATION");
  tft.setTextColor(C_MUTED,C_BG); tft.setCursor(12,28); tft.print("Tap target center");
  const int tx[4]={20,220,220,20},ty[4]={35,35,295,295};
  tft.drawCircle(tx[calibrationStep],ty[calibrationStep],10,C_PINK);
  tft.drawFastHLine(tx[calibrationStep]-14,ty[calibrationStep],28,C_CYAN);
  tft.drawFastVLine(tx[calibrationStep],ty[calibrationStep]-14,28,C_CYAN);
  tft.setCursor(12,310); tft.printf("Point %u / 4",calibrationStep+1);
}
void startTouchCalibration() {
  calibrationActive=true; calibrationStep=0;
  calMinX=4095;calMaxX=0;calMinY=4095;calMaxY=0;
  drawCalibrationTarget(); Serial.println("Touch calibration started.");
}
void acceptCalibrationTouch() {
  if(!touch.touched())return;
  TS_Point p=touch.getPoint(); int rx=p.x,ry=p.y;
  if(TOUCH_SWAP_XY){int z=rx;rx=ry;ry=z;}
  calMinX=min(calMinX,rx);calMaxX=max(calMaxX,rx);
  calMinY=min(calMinY,ry);calMaxY=max(calMaxY,ry);
  while(touch.touched())delay(5);
  if(++calibrationStep<4){drawCalibrationTarget();return;}
  if(calMaxX-calMinX<500||calMaxY-calMinY<500) {
    Serial.println("Calibration failed; old values retained.");
  } else {
    touchRawXMin=calMinX;touchRawXMax=calMaxX;
    touchRawYMin=calMinY;touchRawYMax=calMaxY;
    prefs.begin("petcal",false);
    prefs.putInt("xmin",touchRawXMin);prefs.putInt("xmax",touchRawXMax);
    prefs.putInt("ymin",touchRawYMin);prefs.putInt("ymax",touchRawYMax);
    prefs.end(); setMessage("ТАЧСКРИН НАСТРОЕН");
  }
  calibrationActive=false;currentScreen=SYSTEM;screenChanged=true;dirty=true;
}

// -------------------- SETUP --------------------

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(30);

  delay(150);

  randomSeed(esp_random());

  pinMode(LED_RED,OUTPUT);
  pinMode(LED_GREEN,OUTPUT);
  pinMode(LED_BLUE,OUTPUT);

  writeRgb(0,0,0);

  pinMode(PIN_TFT_BL,OUTPUT);
  digitalWrite(PIN_TFT_BL,HIGH);

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(true);
  tft.setTextWrap(false);

  Serial.printf("TFT dimensions: %d x %d\\n",
                tft.width(),tft.height());

  if (tft.width()!=240 || tft.height()!=320)
    Serial.println("WARNING: expected portrait 240x320");

  SPI.begin(PIN_TFT_SCLK,
            PIN_TFT_MISO,
            PIN_TFT_MOSI,
            PIN_TOUCH_CS);

  touch.begin();
  touch.setRotation(0);

  loadPet();
  loadNetworkSettings();
  loadCachedWeather();

  bootMillis = millis();
  lastTick = millis();
  lastPetAction = millis();

  drawScreen();

  setMessage("ПОМОЩНИК ГОТОВ!");

  Serial.println();
  Serial.println("NIXI OS v2.7 ready.");

  Serial.println("Serial: help, wifi <SSID>|<PASSWORD>, weather, calibrate, tasks, notes, led, tz, reset yes");
  if(wifiSsid.length())beginWifiConnection();
  updateRgbLed();
}

// -------------------- SERIAL COMMANDS --------------------

int parseOffsetMinutes(String v) {
  v.trim();

  if (v.length()<3)
    return 9999;

  int sign = 1;

  if (v[0]=='-') {
    sign = -1;
    v.remove(0,1);
  } else if (v[0]=='+') {
    v.remove(0,1);
  }

  int colon = v.indexOf(':');

  if (colon<0)
    return 9999;

  int hh = v.substring(0,colon).toInt();
  int mm = v.substring(colon+1).toInt();

  if (hh>14 || mm>59 ||
      (mm!=0 && mm!=30) ||
      (hh==14 && mm!=0)) {
    return 9999;
  }

  return sign*(hh*60+mm);
}

void printHelp() {
  Serial.println("Commands:");

  Serial.println(" tasks                 - list tasks");
  Serial.println(" add <text>            - add task (max 8)");
  Serial.println(" edit <n> <text>       - edit task number n");
  Serial.println(" del <n>               - delete task number n");
  Serial.println(" done <n>              - toggle task completion");

  Serial.println(" note <text>           - add note");
  Serial.println(" notes                 - list notes");
  Serial.println(" delnote <n>           - delete note");

  Serial.println(" tz +03:00             - set UTC offset; 30-minute steps");

  Serial.println(" led on|off            - RGB LED");
  Serial.println(" brightness <0-255>    - RGB LED brightness");

  Serial.println(" reset yes             - restore defaults and erase saved data");


}

void processSerialLine(String line) {
  line.trim();

  if (line=="help") {
    printHelp();
    Serial.println("wifi <SSID>|<PASSWORD> - save Wi-Fi and connect");
    Serial.println("wifi clear | weather | calibrate");
    return;
  }
  if(line.startsWith("wifi ")) {
    String a=line.substring(5);
    if(a=="clear") {
      wifiSsid="";wifiPassword="";saveNetworkSettings();
      WiFi.disconnect(true);WiFi.mode(WIFI_OFF);wifiConnecting=false;
      Serial.println("Wi-Fi settings erased.");return;
    }
    int p=a.indexOf('|');
    if(p<=0||p>=a.length()-1){Serial.println("Usage: wifi <SSID>|<PASSWORD>");return;}
    wifiSsid=a.substring(0,p);wifiPassword=a.substring(p+1);wifiSsid.trim();
    saveNetworkSettings();Serial.println("Credentials saved.");beginWifiConnection();return;
  }
  if(line=="weather"){if(!updateWeather())Serial.println("Weather update failed.");return;}
  if(line=="calibrate"){startTouchCalibration();return;}

  if (line=="tasks") {
    for (uint8_t i=0; i<taskCount; i++) {
      Serial.printf("%u. [%c] ",
                    (unsigned)(i+1),
                    tasks[i].done ? 'x' : ' ');

      Serial.println(tasks[i].title);
    }

    return;
  }

  if (line.startsWith("add ")) {
    String v = line.substring(4);
    v.trim();

    if (!v.length()) {
      Serial.println("Empty task");
      return;
    }

    if (taskCount>=MAX_TASKS) {
      Serial.println("Task list full (8)");
      return;
    }

    if (v.length()>60)
      v = v.substring(0,60);

    tasks[taskCount++] = {v,false};

    savePet();
    dirty = true;

    Serial.println("Task added");
    return;
  }

  if (line.startsWith("edit ")) {
    int sp = line.indexOf(' ',5);

    if (sp<0) {
      Serial.println("Usage: edit <n> <text>");
      return;
    }

    int n = line.substring(5,sp).toInt();

    String v = line.substring(sp+1);
    v.trim();

    if (n<1 || n>taskCount || !v.length()) {
      Serial.println("Invalid task number or text");
      return;
    }

    if (v.length()>60)
      v = v.substring(0,60);

    tasks[n-1].title = v;

    savePet();
    dirty = true;

    Serial.println("Task updated");
    return;
  }

  if (line.startsWith("del ")) {
    int n = line.substring(4).toInt();

    if (n<1 || n>taskCount) {
      Serial.println("Invalid task number");
      return;
    }

    for (uint8_t i=n-1; i+1<taskCount; i++)
      tasks[i] = tasks[i+1];

    tasks[--taskCount] = {"",false};

    savePet();
    dirty = true;

    Serial.println("Task deleted");
    return;
  }

  if (line.startsWith("done ")) {
    int n = line.substring(5).toInt();

    if (n<1 || n>taskCount) {
      Serial.println("Invalid task number");
      return;
    }

    tasks[n-1].done = !tasks[n-1].done;


    savePet();
    dirty = true;

    Serial.println("Task status toggled");
    return;
  }

  if (line.startsWith("note ")) {
    String v = line.substring(5);
    v.trim();

    if (v.length()>70)
      v = v.substring(0,70);

    if (v.length()) {
      if (noteCount<MAX_NOTES) {
        notes[noteCount++] = v;
      } else {
        for (uint8_t i=1; i<MAX_NOTES; i++)
          notes[i-1] = notes[i];

        notes[MAX_NOTES-1] = v;
      }

      savePet();
      setMessage("ЗАМЕТКА СОХРАНЕНА");
      dirty = true;

      Serial.println("Note saved");
    }

    return;
  }

  if (line=="notes") {
    for (uint8_t i=0; i<noteCount; i++) {
      Serial.print(i+1);
      Serial.print(": ");
      Serial.println(notes[i]);
    }

    return;
  }

  if (line.startsWith("delnote ")) {
    int n = line.substring(8).toInt();

    if (n>=1 && n<=noteCount) {
      for (uint8_t i=n-1; i+1<noteCount; i++)
        notes[i] = notes[i+1];

      notes[--noteCount] = "";

      savePet();
      dirty = true;

      Serial.println("Note deleted");
    } else {
      Serial.println("Invalid note number");
    }

    return;
  }

  if (line.startsWith("tz ")) {
    int v = parseOffsetMinutes(line.substring(3));

    if (v==9999) {
      Serial.println(
        "Use tz +HH:MM or tz -HH:MM; minutes 00 or 30"
      );
      return;
    }

    utcOffsetMinutes = v;

    savePet();
    dirty = true;

    Serial.print("UTC offset set to ");
    Serial.println(formatUtcOffset());

    return;
  }

  if (line=="led on" || line=="led off") {
    ledEnabled = (line=="led on");

    savePet();
    updateRgbLed();
    dirty = true;

    Serial.println(ledEnabled ? "LED on" : "LED off");
    return;
  }

  if (line.startsWith("brightness ")) {
    int v = line.substring(11).toInt();

    if (v<0 || v>255) {
      Serial.println("Brightness must be 0..255");
      return;
    }

    ledBrightness = v;

    savePet();
    updateRgbLed();

    Serial.println("Brightness saved");
    return;
  }

  if (line=="reset yes") {
    resetToDefaults();

    Serial.println("Defaults restored; saved data erased");
    return;
  }

  if (line.length())
    Serial.println("Unknown command. Type help.");
}

// -------------------- MAIN LOOP --------------------

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');

    if(line.length()) processSerialLine(line);
  }

  if(calibrationActive){acceptCalibrationTouch();delay(12);return;}
  updatePet();
  int x, y;

  if (millis()-lastTouch>220 && readTouch(x,y)) {
    lastTouch = millis();

    Serial.printf("Touch: screen x=%d y=%d\n",x,y);

    handleTouch(x,y);
  }

  if (dirty) {
    lastDraw = millis();

    if (screenChanged) {
      drawScreen();
      screenChanged = false;
    } else if (currentScreen==HOME) {
      drawHomeDynamic();
      dirty = false;
    } else {
      drawScreen();
    }

    petFrameDirty = false;

  } else if (petFrameDirty && currentScreen==HOME) {
    tft.fillRect(95,125,48,48,C_BG);
    drawPet(119,149,idleFrame,currentMood);
    petFrameDirty = false;
  }

  delay(8);
}
