# Arduino Rocket Data Logger

An Arduino-based flight data logger built for an instrumented model rocket. The
sketch records environmental, motion, radiation, and airspeed data to an SD
card during flight and writes it as a single CSV file for later analysis.

The system is built around an Arduino Mega 2560 and a small suite of I2C
sensors plus a serial-connected Geiger counter. Logging begins automatically
once the accelerometer detects a hard launch event, so the device can be armed
on the pad and left alone until recovery.

## Hardware

| Component | Purpose | Interface |
| --- | --- | --- |
| Arduino Mega 2560 | Main controller | — |
| Adafruit MMA8451 | 3-axis accelerometer (±8 G range) | I2C |
| Adafruit MCP9808 | High-precision temperature sensor | I2C |
| PCF8523 RTC | Real-time clock for timestamping samples | I2C |
| SD card module | Flight log storage | SPI (CS on pin 10) |
| Honeywell-style differential pressure sensor (0x28) | Pitot tube → airspeed | I2C (via `I2C` library) |
| Pocket Geiger counter | Ionizing radiation (cps, cpm, μSv/hr) | UART on `Serial1` |

### Wiring notes

- All I2C sensors share the Mega's `SDA`/`SCL` lines (pins 20/21).
- The SD card chip-select is **pin 10**. If you change boards, update
  `chipSelect` in the sketch.
- The Geiger counter must be wired to **Serial1** (pins 18 TX / 19 RX on the
  Mega). The sketch expects ASCII frames terminated by `\n` in the standard
  Pocket Geiger comma-separated format.
- The differential pressure sensor is addressed at `0x28`. It is read with the
  `I2C` library (DSSCircuits) rather than `Wire`, which is why both libraries
  are initialized in `setup()`.

## Software

### Required libraries

Install these via the Arduino Library Manager (or by hand):

- `Wire` (bundled)
- `SD` (bundled)
- `Adafruit_MMA8451`
- `Adafruit_Sensor`
- `Adafruit_MCP9808`
- `RTClib`
- `I2C` by Wayne Truchsess (DSSCircuits) — not the same as `Wire`

### Building and flashing

The repo includes `sketch.json` targeting `arduino:avr:mega` (ATmega2560).
With the Arduino CLI:

```bash
arduino-cli compile --fqbn arduino:avr:mega:cpu=atmega2560 .
arduino-cli upload  --fqbn arduino:avr:mega:cpu=atmega2560 -p /dev/ttyACM0 .
```

Or open `Rocketv2.ino` in the Arduino IDE, select **Tools → Board → Arduino
Mega or Mega 2560**, and click **Upload**.

## How it works

### Startup

`setup()` initializes Serial (USB) and Serial1 (Geiger), brings up both I2C
buses, and probes each sensor. Sensor init failures are silently ignored —
flight code is deliberately non-blocking so a single sensor dropout does not
prevent logging. The accelerometer range is set to ±8 G.

The SD card is mounted and a new log file `datalog.csv` is opened in
write/append mode. The header row is written immediately:

```
millis,datetime,tempC,tempF,Xaccel,Yaccel,Zaccel,cps,cpm,usv/hr,pressure,velocity,mach
```

### Launch detection

Before the main loop runs, the sketch spins in a tight polling loop reading
the accelerometer and waiting until any axis exceeds **30 m/s²** (~3 G). This
acts as a launch trigger so the SD card is not filled with idle pad data.
While waiting, the sketch prints `wait` and the current axis readings over
USB serial; once the threshold is crossed it prints `go` and falls through
into `loop()`.

### Per-sample logging

Every iteration of `loop()` (roughly every 100 ms) appends one CSV row with:

1. **`millis`** — milliseconds since reset.
2. **`datetime`** — RTC timestamp quoted as `"YYYY/M/D H:M:S"`.
3. **`tempC` / `tempF`** — MCP9808 temperature in Celsius and Fahrenheit.
4. **`Xaccel` / `Yaccel` / `Zaccel`** — MMA8451 acceleration in m/s².
5. **`cps` / `cpm` / `usv/hr`** — parsed out of the latest Geiger frame on
   `Serial1` if one is available; otherwise three empty fields are written.
6. **`pressure`** — differential pressure in PSI from the pitot sensor,
   computed from the 14-bit raw count using the standard ±15 PSI transfer
   function:
   `P = Pmin + (raw − 1638) / (14745 − 1638) × (Pmax − Pmin)`.
7. **`velocity`** — airspeed in ft/s derived from Bernoulli's equation,
   `v = sqrt(2·ΔP / ρ)`, where air density `ρ` is computed from the ideal gas
   law using the measured temperature and a fixed ambient pressure of
   14.55 PSI (`R = 53.33` ft·lbf/(lbm·°R)).
8. **`mach`** — `velocity / a`, where the local speed of sound is
   `a = 331 + 0.6·T_C` m/s, converted to ft/s.

### Buffering and durability

To minimize SD write amplification mid-flight, `logfile.flush()` is called at
most once every **`SYNC_INTERVAL` = 900 ms**. Between flushes, samples sit in
the FAT driver's buffer. If the rocket loses power before a flush, you may
lose up to the last ~900 ms of data; everything older is on-card.

## Output format

`datalog.csv` is a plain CSV with the header described above. Empty Geiger
fields appear as consecutive commas (`,,,`). The file can be opened directly
in Excel, Pandas, MATLAB, or any other CSV tool.

A quick Pandas example:

```python
import pandas as pd
df = pd.read_csv("datalog.csv")
df["t_s"] = df["millis"] / 1000.0
df.plot(x="t_s", y=["Xaccel", "Yaccel", "Zaccel"])
```

## Configuration

A handful of constants near the top of `Rocketv2.ino` are worth knowing
about:

| Constant | Default | Meaning |
| --- | --- | --- |
| `chipSelect` | `10` | SD card CS pin |
| `SYNC_INTERVAL` | `900` | ms between `flush()` calls |
| `sensor_addr` | `0x28` | Pressure sensor I2C address |
| `pmin` / `pmax` | `-15.0` / `15.0` | Pressure sensor range, PSI |
| `airpressure` | `14.55` | Assumed ambient pressure, PSI |
| `R` | `53.33` | Specific gas constant for air, ft·lbf/(lbm·°R) |
| Launch threshold | `30` m/s² | Acceleration that arms the main loop |

The launch threshold is hard-coded inside `setup()` rather than as a `#define`
— if your motor produces less than ~3 G of initial thrust you will need to
lower it or the logger will never arm.

## Limitations and known issues

- **No altitude sensor.** Altitude must be inferred from integrated
  acceleration or computed externally from airspeed.
- **Ambient pressure is hard-coded.** `airpressure = 14.55 PSI` is assumed
  constant; real density (and therefore airspeed) drifts as the rocket gains
  altitude. For low-altitude flights this is a small error.
- **Launch trigger is one-shot.** The arming loop only checks
  `x < 30 && y < 30 && z < 30`. Once it falls through, there is no way to
  re-arm without a reset.
- **Sensor init failures are silent.** If a sensor is missing, the
  corresponding column will contain junk or stale values rather than a
  flagged error. Check sample plots for flatlines.
- **`Serial1` parsing assumes a specific Geiger frame format.** Frames must
  have at least six commas; malformed frames will produce empty strings for
  cps/cpm/μSv·hr.

## Repository layout

```
.
├── README.md       — this file
├── Rocketv2.ino    — the Arduino sketch
└── sketch.json     — Arduino IDE board metadata (Mega 2560)
```

## License

No license file is included in this repository. Treat the contents as
"all rights reserved" by the original author unless and until a license is
added.
