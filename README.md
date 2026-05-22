# Rocketv2

Arduino Mega 2560 flight/composite-sensor logging sketch.

## Repository layout

- `Rocketv2.ino` — main Arduino sketch
- `sketch.json` — Arduino Web Editor board metadata
- `.gitignore` — generated-file ignore rules

## What the sketch does

This sketch logs flight data to `datalog.csv` on an SD card using:

- MCP9808 temperature sensor
- MMA8451 accelerometer
- PCF8523 RTC
- serial-connected Geiger counter
- I2C pressure sensor

Logged fields:

- millis timestamp
- RTC date/time
- temperature in C/F
- X/Y/Z acceleration
- counts per second / minute
- radiation rate (`uSv/hr`)
- pressure
- velocity
- Mach number

## Hardware / library dependencies

The sketch expects these Arduino libraries to be installed:

- `Adafruit_MMA8451`
- `Adafruit_Sensor`
- `Adafruit_MCP9808`
- `RTClib`
- `SD`
- `I2C`

## Board configuration

`sketch.json` targets:

- Board: Arduino Mega 2560
- FQBN: `arduino:avr:mega:cpu=atmega2560`

## Notes

- The sketch waits for acceleration above a threshold before beginning normal logging.
- Sensor initialization failures are currently silent because debug `Serial` output is commented out.
- Data is appended to `datalog.csv` on the SD card.
