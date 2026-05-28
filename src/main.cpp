#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "../front/magicalmond_ogyg820pt7b.h"

static constexpr int TFT_SCK = 12;
static constexpr int TFT_MOSI = 11;
static constexpr int TFT_CS = 10;
static constexpr int TFT_DC = 9;
static constexpr int TFT_RST = 13;

static constexpr int FSR_AO_PIN = 1;
static constexpr int IDLE_THRESHOLD = 3980;
static constexpr int PRESS_THRESHOLD_ONE = 3300;
static constexpr uint32_t SLEEP_TIMEOUT_MS = 120000;
static constexpr uint32_t SAMPLE_INTERVAL_MS = 50;
static constexpr uint32_t LONG_PRESS_MS = 2000;
const char *LONG_PRESS_TEXT = "good touch!";

const char *DISPLAY_TEXTS[] = {"zzz...", 
    "boring",
    "pet me",
    "hmm...",
    "oops",
    "hehe",
    "lonely",
    "sleepy",
    "love",
    "again!",
    "one more",
    "miss u",
    "play?",
    "ok",
    "yay",
    "fun",
    "pet me",
    "wow"};
static constexpr size_t DISPLAY_TEXT_COUNT =
    sizeof(DISPLAY_TEXTS) / sizeof(DISPLAY_TEXTS[0]);
static constexpr size_t FIRST_ACTIVE_TEXT_INDEX = 1;

bool pressInProgress = false;
bool longPressTriggered = false;
bool sleeping = true;
size_t currentTextIndex = 0;
uint32_t lastTouchMs = 0;
uint32_t lastSampleMs = 0;
uint32_t pressStartMs = 0;

Arduino_DataBus *bus = new Arduino_ESP32SPI(
    TFT_DC,
    TFT_CS,
    TFT_SCK,
    TFT_MOSI,
    GFX_NOT_DEFINED);

Arduino_GFX *gfx = new Arduino_GC9A01(
    bus,
    TFT_RST,
    0,
    true,
    240,
    240);

void drawTextCentered(const char *text) {
  gfx->fillScreen(BLACK);
  gfx->setFont(&magicalmond_ogyg820pt7b);
  gfx->setTextColor(WHITE);

  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t x = (240 - static_cast<int16_t>(w)) / 2 - x1;
  int16_t y = (240 - static_cast<int16_t>(h)) / 2 - y1;
  gfx->setCursor(x, y);
  gfx->println(text);
}

void showText(size_t index) {
  currentTextIndex = index % DISPLAY_TEXT_COUNT;
  sleeping = (currentTextIndex == 0);
  drawTextCentered(DISPLAY_TEXTS[currentTextIndex]);
}

void handleCompletedClick() {
  size_t nextIndex = currentTextIndex + 1;
  if (nextIndex >= DISPLAY_TEXT_COUNT) {
    nextIndex = FIRST_ACTIVE_TEXT_INDEX;
  }
  showText(nextIndex);

  Serial.print("Click -> ");
  Serial.println(DISPLAY_TEXTS[currentTextIndex]);
}

void handleLongPress() {
  longPressTriggered = true;
  sleeping = false;
  drawTextCentered(LONG_PRESS_TEXT);

  Serial.print("Long press -> ");
  Serial.println(LONG_PRESS_TEXT);
}

void setup() {
  Serial.begin(115200);

  if (!gfx->begin()) {
    Serial.println("GC9A01 init failed");
    while (true) {
      delay(1000);
    }
  }

  pinMode(FSR_AO_PIN, INPUT);
  analogReadResolution(12);

  Serial.println("FSR402 test start");
  showText(0);
  lastTouchMs = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs = now;

  int aoValue = analogRead(FSR_AO_PIN);
  Serial.print("AO = ");
  Serial.println(aoValue);

  if (aoValue <= PRESS_THRESHOLD_ONE) {
    if (!pressInProgress) {
      pressInProgress = true;
      longPressTriggered = false;
      pressStartMs = now;
    }
    lastTouchMs = now;

    if (!longPressTriggered && (now - pressStartMs >= LONG_PRESS_MS)) {
      handleLongPress();
    }
  } else if (pressInProgress && aoValue >= IDLE_THRESHOLD) {
    pressInProgress = false;
    bool wasLongPress = longPressTriggered;
    longPressTriggered = false;
    lastTouchMs = now;
    if (!wasLongPress) {
      handleCompletedClick();
    }
  }

  if (!sleeping && !pressInProgress && (now - lastTouchMs >= SLEEP_TIMEOUT_MS)) {
    showText(0);
    Serial.println("Sleep timeout -> zzz...");
  }
}
