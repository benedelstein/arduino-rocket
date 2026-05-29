# Rocketv2

Arduino Mega rocket flight data logger.

Logs sensor readings to `datalog.csv` on an SD card, including:

- RTC timestamp and milliseconds since startup
- MCP9808 temperature in Celsius and Fahrenheit
- MMA8451 acceleration on X/Y/Z axes
- Geiger counter CPS, CPM, and uSv/hr from `Serial1`
- Pressure-derived velocity and Mach estimate

## Hardware target

The sketch is configured for an Arduino/Genuino Mega or Mega 2560 in `sketch.json`.

## Notes

- SD card chip select is pin 10.
- Logging begins after the accelerometer trigger threshold is reached.
- Output file: `datalog.csv`.
