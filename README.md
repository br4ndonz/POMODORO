# Pomodoro Timer

A study timer I built with an ESP32-S3, a 16x2 LCD, and a rotary knob.
Turn the knob to set the minutes, click to start or pause, and hold to reset.

![The timer counting down](images/running.jpg)

## How to use it
1. Turn the knob to pick 1–60 minutes.
2. Click to start.
3. Click again to pause, and click once more to resume.
4. Hold the knob down to reset.

![Picking the time](images/setting.jpg)

## How it works
The code uses a state machine with four modes: setting, running, paused, and done.
Each loop, it first reads the knob and button, then decides what to do based on the mode.
Timing uses millis() instead of delay(), so the knob keeps working while the timer runs.

## Parts
- ESP32-S3 (Freenove)
- 16x2 LCD with I2C backpack
- KY-040 rotary encoder

## Wiring
| Part | Pin | ESP32 |
|---|---|---|
| LCD | SDA | 8 |
| LCD | SCL | 9 |
| LCD | VCC | 5V |
| LCD | GND | GND |
| Knob | CLK | 1 |
| Knob | DT | 2 |
| Knob | SW | 21 |
| Knob | + | 3.3V |
| Knob | GND | GND |

![Wiring](images/wiring.jpg)

## What I learned
- Reading a rotary encoder and debouncing a button
- Timing with millis() instead of delay()
- Organizing code as a state machine