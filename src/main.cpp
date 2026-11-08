#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int PIN_CLK = 1;
const int PIN_DT  = 2;
const int PIN_SW  = 21;

const unsigned long DEBOUNCE_MS   = 30;
const unsigned long LONG_PRESS_MS = 800;
const unsigned long DONE_SHOW_MS  = 3000;

int minutes = 25;
int clkvalue;
int lastButtonState = HIGH;
unsigned long lastChangeTime = 0;
unsigned long pressStart = 0;
unsigned long timeheld;

enum State { SETTING, RUNNING, PAUSED, DONE };   // NEW: PAUSED
State state = SETTING;

unsigned long endtime;
unsigned long remaining;                          // NEW: time left when paused
unsigned long doneAt;
int lastShownSecond = -1;

void drawSetScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SET: ");
  lcd.print(minutes);
  lcd.print(" min  ");
  lcd.setCursor(0, 1);
  lcd.print("Click to start");
}

// NEW: shows the time on row 0 (used by RUNNING and PAUSED)
void drawTime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  char buf[17];
  sprintf(buf, "Time: %02lu:%02lu   ", seconds / 60, seconds % 60);
  lcd.setCursor(0, 0);
  lcd.print(buf);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);
  lcd.init();
  lcd.backlight();
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);
  clkvalue = digitalRead(PIN_CLK);
  drawSetScreen();
}

void loop() {
  // ===== PART A: read inputs -> events =====
  bool clicked = false;
  bool longPressed = false;
  int turn = 0;

  int button = digitalRead(PIN_SW);
  if (button != lastButtonState && millis() - lastChangeTime > DEBOUNCE_MS) {
    lastChangeTime = millis();
    lastButtonState = button;
    if (button == LOW) {
      pressStart = millis();
    } else {
      timeheld = millis() - pressStart;
      if (timeheld > LONG_PRESS_MS) longPressed = true;
      else                          clicked = true;
    }
  }

  int clk = digitalRead(PIN_CLK);
  if (clk != clkvalue) {
    turn = (clk != digitalRead(PIN_DT)) ? 1 : -1;
  }
  clkvalue = clk;

  // ===== PART B: act based on state =====
  if (state == SETTING) {
    if (turn != 0) {
      minutes += turn;
      if (minutes > 60) minutes = 60;
      if (minutes < 1)  minutes = 1;
      lcd.setCursor(0, 0);
      lcd.print("SET: ");
      lcd.print(minutes);
      lcd.print(" min  ");
    }
    if (clicked) {
      endtime = millis() + minutes * 60000UL;
      lastShownSecond = -1;
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Click=pause     ");          // NEW: updated hint
      state = RUNNING;
    } else if (longPressed) {
      minutes = 25;
      drawSetScreen();
    }
  }

  else if (state == RUNNING) {
    if (longPressed) {
      minutes = 25;
      drawSetScreen();
      state = SETTING;
    }
    else if (clicked) {                        // NEW: pause
      remaining = endtime - millis();          // NEW: freeze the time left
      drawTime(remaining);                     // NEW: show the exact paused time
      lcd.setCursor(0, 1);
      lcd.print("PAUSED          ");
      state = PAUSED;
    }
    else if (millis() >= endtime) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DONE!");
      doneAt = millis();
      state = DONE;
    }
    else {
      unsigned long timeleft = endtime - millis();
      int secondsl = (timeleft / 1000) % 60;
      if (secondsl != lastShownSecond) {
        drawTime(timeleft);                    // NEW: uses the helper function
        lastShownSecond = secondsl;
      }
    }
  }

  else if (state == PAUSED) {                  // NEW: whole block
    if (clicked) {                             // resume
      endtime = millis() + remaining;          // rebuild end time from what was left
      lastShownSecond = -1;
      lcd.setCursor(0, 1);
      lcd.print("Click=pause     ");
      state = RUNNING;
    }
    else if (longPressed) {                    // reset
      minutes = 25;
      drawSetScreen();
      state = SETTING;
    }
  }

  else if (state == DONE) {
    if (millis() - doneAt > DONE_SHOW_MS || clicked || longPressed) {
      drawSetScreen();
      state = SETTING;
    }
  }
}