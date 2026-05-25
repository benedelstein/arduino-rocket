# Rocketv2

Arduino sketch for a model rocket flight data logger. Targets the Arduino Mega 2560.

## Hardware

- **MMA8451** accelerometer (±8G) — also used as the launch trigger
- **MCP9808** temperature sensor
- **PCF8523** real-time clock
- **SD card** module on chip-select pin 10
- **Geiger counter** on `Serial1` (9600 baud, CSV output)
- **Pressure sensor** at I²C address `0x28` (±15 psi), used as a pitot tube

## Behavior

On boot, the sketch initializes all sensors and opens `datalog.csv` on the SD card. It then busy-waits in `setup()` until acceleration on any axis exceeds 30 m/s² — the launch trigger. Once triggered, `loop()` runs every ~100 ms and writes a row of sensor data, flushing to the SD card every 900 ms.

Airspeed is derived from the pitot pressure and air density (computed from temperature and a fixed reference air pressure of 14.55 psi); Mach number is computed from the local speed of sound.

## Log format

`datalog.csv` columns:

```
millis, datetime, tempC, tempF, Xaccel, Yaccel, Zaccel, cps, cpm, usv/hr, pressure, velocity, mach
```

Units: ms, UTC (RTC is 5 hours ahead of local), °C, °F, m/s², m/s², m/s², counts/sec, counts/min, µSv/hr, psi, ft/s, Mach.

## Files

- `Rocketv2.ino` — main sketch
- `sketch.json` — Arduino IDE board config (Mega 2560)
