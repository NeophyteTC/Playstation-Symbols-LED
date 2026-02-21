#include <FastLED.h>
#include <EEPROM.h>

#define LED_PIN 6
#define BTN_MODE 2
#define BTN_BRIGHT 3

#define NUM_LEDS 27
#define DEFAULT_BRIGHTNESS 180
#define NIGHT_BRIGHTNESS 40
#define MIN_BRIGHTNESS 40
#define MAX_MILLIAMPS 500

#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

#define TOTAL_EFFECTS 6

// -------- SEGMENTE --------
#define SQ_START 0
#define SQ_COUNT 8
#define CR_START 8
#define CR_COUNT 5
#define CI_START 13
#define CI_COUNT 7
#define TR_START 20
#define TR_COUNT 7

CRGB baseColors[4] = {
  CRGB(255,20,147),
  CRGB::Blue,
  CRGB::Red,
  CRGB::Green
};

// -------- STATE --------
uint8_t currentEffect = 0;
uint8_t currentBrightness;
uint8_t targetBrightness;
bool nightModeActive = false;

unsigned long lastFrame = 0;
const uint16_t frameInterval = 16;

// Button State
bool lastModeState = HIGH;
bool lastBrightState = HIGH;
unsigned long pressStart = 0;
bool longPressHandled = false;

// Boot State
uint8_t bootPhase = 0;
unsigned long bootTimer = 0;

// -------- SAFE EEPROM WRITE --------
void safeWriteEEPROM(int addr, uint8_t val) {
  if (EEPROM.read(addr) != val)
    EEPROM.update(addr, val);
}

// -------- SICHERE FADE KURVE --------
uint8_t safeCurve(uint8_t x) {
  float f = x / 255.0;
  f = pow(f, 2.0);
  uint8_t out = f * 255;
  if (out < 12) return 0;
  return out;
}

// -------- SETUP --------
void setup() {

  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_BRIGHT, INPUT_PULLUP);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.setDither(true);

  currentBrightness = EEPROM.read(1);
  if (currentBrightness < MIN_BRIGHTNESS || currentBrightness > 255)
    currentBrightness = DEFAULT_BRIGHTNESS;

  targetBrightness = currentBrightness;
  FastLED.setBrightness(currentBrightness);
}

// -------- LOOP --------
void loop() {

  handleButtons();
  updateBrightnessSmooth();

  if (millis() - lastFrame >= frameInterval) {
    lastFrame = millis();
    runEffect();
    FastLED.show();
  }
}

// -------- BUTTON HANDLING --------
void handleButtons() {

  static unsigned long lastDebounceTime = 0;
  static const unsigned long debounceDelay = 50;
  static bool stableState = HIGH;

  bool reading = digitalRead(BTN_MODE);

  if (reading != lastModeState)
    lastDebounceTime = millis();

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (reading != stableState) {
      stableState = reading;

      if (reading == LOW) {
        pressStart = millis();
        longPressHandled = false;
      }

      if (reading == HIGH) {
        if (!longPressHandled) {
          currentEffect++;
          if (currentEffect >= TOTAL_EFFECTS)
            currentEffect = 0;

          bootPhase = 0;
          bootTimer = millis();
        }
      }
    }
  }

// LONG PRESS (3 Sekunden)
if (stableState == LOW && !longPressHandled) {
  if (millis() - pressStart > 3000) {

    nightModeActive = !nightModeActive;

    if (nightModeActive) {
      targetBrightness = NIGHT_BRIGHTNESS;
    } else {
      targetBrightness = currentBrightness;
    }

    longPressHandled = true;
  }
}

  lastModeState = reading;

  // ---- Brightness Button ----
  bool brightReading = digitalRead(BTN_BRIGHT);

  if (brightReading == LOW && lastBrightState == HIGH) {

    currentBrightness += 25;
    if (currentBrightness > 250)
      currentBrightness = MIN_BRIGHTNESS;

    if (!nightModeActive)
      targetBrightness = currentBrightness;

    safeWriteEEPROM(1, currentBrightness);
  }

  lastBrightState = brightReading;
}

// -------- SMOOTH BRIGHTNESS --------
void updateBrightnessSmooth() {

  uint8_t b = FastLED.getBrightness();

  if (b < targetBrightness)
    FastLED.setBrightness(b + 2);
  else if (b > targetBrightness)
    FastLED.setBrightness(b - 2);
}

// -------- EFFECT ENGINE --------
void runEffect() {

  if (nightModeActive) {
    setAllStandard();
    return;
  }

  switch (currentEffect) {
    case 0: psBootExact(); break;
    case 1: breathe(); break;
    case 2: colorBounce(); break;
    case 3: sparkleWhite(); break;
    case 4: sparkleSegmented(); break;
    case 5: slowFadeChase(); break;
  }
}

// -------- STATIC STANDARD --------
void setAllStandard() {
  for (int i = 0; i < 4; i++)
    setSegmentByIndex(i, baseColors[i]);
}

// -------- BREATHE (4 Sekunden) --------
void breathe() {

  uint16_t cycle = millis() % 4000;
  float phase = (float)cycle / 4000.0;
  float wave = (sin(phase * TWO_PI - HALF_PI) + 1.0) / 2.0;

  uint8_t scale = safeCurve(wave * 255);

  FastLED.clear();

  for (int i = 0; i < 4; i++) {
    CRGB c = baseColors[i];
    c.nscale8(scale);
    setSegmentByIndex(i, c);
  }
}

// -------- COLOR BOUNCE --------
void colorBounce() {

  static int offset = 0;
  static int dir = 1;

  for (int i = 0; i < 4; i++)
    setSegmentByIndex(i, baseColors[(i + offset) % 4]);

  EVERY_N_MILLISECONDS(600) {
    offset += dir;
    if (offset == 3 || offset == 0)
      dir *= -1;
  }
}

// -------- SPARKLE WHITE --------
void sparkleWhite() {
  fadeToBlackBy(leds, NUM_LEDS, 30);
  leds[random(NUM_LEDS)] += CRGB::White;
}

// -------- SPARKLE SEGMENTIERT --------
void sparkleSegmented() {

  fadeToBlackBy(leds, NUM_LEDS, 30);

  int segment = random(4);
  int index;

  if (segment == 0) index = random(SQ_START, SQ_START + SQ_COUNT);
  if (segment == 1) index = random(CR_START, CR_START + CR_COUNT);
  if (segment == 2) index = random(CI_START, CI_START + CI_COUNT);
  if (segment == 3) index = random(TR_START, TR_START + TR_COUNT);

  leds[index] += baseColors[segment];
}

// -------- SLOW FADE CHASE MIT ÜBERBLENDUNG --------
void slowFadeChase() {

  static int index = 0;
  static int direction = 1;
  static uint8_t fadeValue = 0;

  int nextIndex = index + direction;

  if (nextIndex > 3) nextIndex = 2;
  if (nextIndex < 0) nextIndex = 1;

  uint8_t fadeIn  = safeCurve(fadeValue);
  uint8_t fadeOut = safeCurve(255 - fadeValue);

  FastLED.clear();

  CRGB c1 = baseColors[index];
  c1.nscale8(fadeOut);
  setSegmentByIndex(index, c1);

  CRGB c2 = baseColors[nextIndex];
  c2.nscale8(fadeIn);
  setSegmentByIndex(nextIndex, c2);

  EVERY_N_MILLISECONDS(20) {

    if (fadeValue < 250)
      fadeValue += 5;
    else {
      fadeValue = 0;
      index = nextIndex;

      if (index == 3) direction = -1;
      if (index == 0) direction = 1;
    }
  }
}

// -------- PS1 BOOT --------
void psBootExact() {

  unsigned long now = millis();

  switch (bootPhase) {

    case 0:
      FastLED.clear();
      if (now - bootTimer > 1000) {
        bootPhase = 1;
        bootTimer = now;
      }
      break;

    case 1:
    {
      float progress = (now - bootTimer) / 2000.0;
      if (progress >= 1.0) {
        progress = 1.0;
        bootPhase = 2;
        bootTimer = now;
      }

      uint8_t scale = safeCurve(progress * 255);

      FastLED.clear();
      for (int i = 0; i < 4; i++) {
        CRGB c = baseColors[i];
        c.nscale8(scale);
        setSegmentByIndex(i, c);
      }
    }
    break;

    case 2:
      fill_solid(leds, NUM_LEDS, CRGB::White);
      if (now - bootTimer > 200) {
        bootPhase = 3;
        bootTimer = now;
      }
      break;

    case 3:
    {
      float progress = (now - bootTimer) / 1500.0;
      if (progress >= 1.0)
        bootPhase = 4;

      uint8_t scale = safeCurve((1.0 - progress) * 255);

      FastLED.clear();
      for (int i = 0; i < 4; i++) {
        CRGB c = baseColors[i];
        c.nscale8(scale);
        setSegmentByIndex(i, c);
      }
    }
    break;

    case 4:
      setAllStandard();
      break;
  }
}

// -------- SEGMENT HELPER --------
void setSegment(int start, int count, CRGB color) {
  for (int i = start; i < start + count; i++)
    leds[i] = color;
}

void setSegmentByIndex(int index, CRGB color) {
  if (index == 0) setSegment(SQ_START, SQ_COUNT, color);
  if (index == 1) setSegment(CR_START, CR_COUNT, color);
  if (index == 2) setSegment(CI_START, CI_COUNT, color);
  if (index == 3) setSegment(TR_START, TR_COUNT, color);
}