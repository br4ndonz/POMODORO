/*
  Pomodoro Timer
  by Brandon Zhou

  A study timer. Turn the knob to pick how many minutes,
  click to start, click again to pause, hold to reset.

  Parts:
    - ESP32-S3 board
    - 16x2 LCD screen with I2C backpack
    - KY-040 rotary knob (it turns AND clicks)

  Wiring:
    LCD:   SDA -> pin 8,  SCL -> pin 9,  VCC -> 5V,   GND -> GND
    Knob:  CLK -> pin 1,  DT  -> pin 2,  SW  -> pin 21,
           +   -> 3.3V,   GND -> GND
*/

#include <Arduino.h>
#include <Wire.h>               // lets the board talk to the LCD over 2 wires (I2C)
#include <LiquidCrystal_I2C.h>  // makes printing to the LCD easy

// The LCD. 0x27 is its address, 16 = letters per row, 2 = rows.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Which pins the knob is plugged into.
const int PIN_CLK = 1;   // knob turn signal A
const int PIN_DT  = 2;   // knob turn signal B
const int PIN_SW  = 21;  // knob button (when you push it in)

// Timing settings, in milliseconds (1000 ms = 1 second).
const unsigned long DEBOUNCE_MS   = 30;    // ignore button flickers shorter than this
const unsigned long LONG_PRESS_MS = 800;   // hold longer than this = "long press"
const unsigned long DONE_SHOW_MS  = 3000;  // how long "DONE!" stays on screen

int minutes = 25;   // the timer length you pick with the knob

// Knob and button memory. We compare "now" with "last time"
// to notice when something changes.
int clkvalue;                       // what the knob's CLK pin read last time
int lastButtonState = HIGH;         // HIGH = not pressed, LOW = pressed
unsigned long lastChangeTime = 0;   // when the button last changed
unsigned long pressStart = 0;       // when the button was pushed down
unsigned long timeheld;             // how long it was held down

// The timer is always in exactly ONE of these modes.
enum State { SETTING, RUNNING, PAUSED, DONE };
State state = SETTING;   // start in the "pick your minutes" mode

unsigned long endtime;       // the moment the timer should hit 0
unsigned long remaining;     // time left, saved when you pause
unsigned long doneAt;        // the moment the timer finished
int lastShownSecond = -1;    // the last second we showed (so we only redraw when it changes)


// Draws the "pick your minutes" screen.
void drawSetScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);         // top row, far left
  lcd.print("SET: ");
  lcd.print(minutes);
  lcd.print(" min  ");         // the extra spaces erase leftover digits
  lcd.setCursor(0, 1);         // bottom row
  lcd.print("Click to start");
}

// Shows the time left on the top row, like "Time: 04:07".
// ms = time left in milliseconds.
void drawTime(unsigned long ms) {
  unsigned long seconds = ms / 1000;   // milliseconds -> seconds
  char buf[17];                        // space for 16 letters + 1 hidden end marker
  // %02lu means "a number, always 2 digits, add a 0 in front if needed"
  // seconds / 60 = whole minutes, seconds % 60 = leftover seconds
  sprintf(buf, "Time: %02lu:%02lu   ", seconds / 60, seconds % 60);
  lcd.setCursor(0, 0);
  lcd.print(buf);
}


// setup() runs ONE time when the board turns on.
void setup() {
  Serial.begin(115200);   // lets us print messages to the computer for debugging
  Wire.begin(8, 9);       // start the LCD connection on pins 8 and 9
  lcd.init();
  lcd.backlight();

  // INPUT_PULLUP = the pin reads HIGH normally and LOW when the switch connects it to ground
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);

  clkvalue = digitalRead(PIN_CLK);   // remember where the knob starts
  drawSetScreen();
}


// loop() runs over and over, thousands of times per second.
// It has 2 parts:
//   Part A: check the knob and button, and write down what happened.
//   Part B: decide what to do with that, depending on the current mode.
void loop() {

  // ---------- PART A: what did the user do? ----------

  // These start as "nothing happened" every time loop() runs.
  bool clicked = false;       // quick press?
  bool longPressed = false;   // held down?
  int turn = 0;               // +1 = turned right, -1 = turned left, 0 = didn't turn

  // BUTTON
  int button = digitalRead(PIN_SW);
  // Did the button change? And has it been long enough since the last change?
  // (Buttons "bounce" for a few ms when pressed, which looks like many presses.
  // Waiting 30 ms filters that out.)
  if (button != lastButtonState && millis() - lastChangeTime > DEBOUNCE_MS) {
    lastChangeTime = millis();
    lastButtonState = button;

    if (button == LOW) {
      // Button just went down. Remember the time.
      pressStart = millis();
    } else {
      // Button just came back up. How long was it held?
      timeheld = millis() - pressStart;
      if (timeheld > LONG_PRESS_MS) longPressed = true;
      else                          clicked = true;
    }
  }

  // KNOB
  // When the knob turns, CLK and DT switch on and off slightly out of step.
  // When CLK changes, we check DT. If they're different it went one way,
  // if they're the same it went the other way.
  int clk = digitalRead(PIN_CLK);
  if (clk != clkvalue) {
    turn = (clk != digitalRead(PIN_DT)) ? 1 : -1;   // short way to write an if/else
  }
  clkvalue = clk;   // remember for next time


  // ---------- PART B: what should we do about it? ----------

  // MODE 1: picking the number of minutes
  if (state == SETTING) {
    if (turn != 0) {
      minutes += turn;                  // add 1 or take away 1
      if (minutes > 60) minutes = 60;   // don't go above 60
      if (minutes < 1)  minutes = 1;    // don't go below 1
      lcd.setCursor(0, 0);
      lcd.print("SET: ");
      lcd.print(minutes);
      lcd.print(" min  ");
    }
    if (clicked) {
      // Start the timer. millis() is "how many ms since the board turned on",
      // so the end time = now + the timer length.
      // (60000UL = 60,000 ms in one minute. UL tells C++ it's a big number.)
      endtime = millis() + minutes * 60000UL;
      lastShownSecond = -1;   // makes the first second show up right away
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("Click=pause     ");
      state = RUNNING;
    } else if (longPressed) {
      minutes = 25;   // back to the default
      drawSetScreen();
    }
  }

  // MODE 2: timer is counting down
  else if (state == RUNNING) {
    if (longPressed) {
      // Reset everything
      minutes = 25;
      drawSetScreen();
      state = SETTING;
    }
    else if (clicked) {
      // Pause. The board's clock (millis) never stops, so we save
      // how much time was left right now, and use it when we resume.
      remaining = endtime - millis();
      drawTime(remaining);
      lcd.setCursor(0, 1);
      lcd.print("PAUSED          ");
      state = PAUSED;
    }
    else if (millis() >= endtime) {
      // Time's up!
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DONE!");
      doneAt = millis();
      state = DONE;
    }
    else {
      // Still counting. Only redraw when the seconds number changes,
      // otherwise the screen would flicker from being redrawn too often.
      unsigned long timeleft = endtime - millis();
      int secondsl = (timeleft / 1000) % 60;
      if (secondsl != lastShownSecond) {
        drawTime(timeleft);
        lastShownSecond = secondsl;
      }
    }
  }

  // MODE 3: paused, waiting for you
  else if (state == PAUSED) {
    if (clicked) {
      // Resume. Make a new end time from the time that was left.
      endtime = millis() + remaining;
      lastShownSecond = -1;
      lcd.setCursor(0, 1);
      lcd.print("Click=pause     ");
      state = RUNNING;
    }
    else if (longPressed) {
      // Reset
      minutes = 25;
      drawSetScreen();
      state = SETTING;
    }
  }

  // MODE 4: finished. Show "DONE!" for 3 seconds (or until a press), then go back.
  else if (state == DONE) {
    if (millis() - doneAt > DONE_SHOW_MS || clicked || longPressed) {
      drawSetScreen();
      state = SETTING;
    }
  }
}