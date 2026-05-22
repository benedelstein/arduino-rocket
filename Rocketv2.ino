#include <Wire.h>
#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_MCP9808.h"
#include <RTClib.h>
#include <SD.h>
#include <I2C.h>

// -----------------------------------------------------------------------------
// Sensor and peripheral instances
// -----------------------------------------------------------------------------
Adafruit_MMA8451 accelerometer = Adafruit_MMA8451();
Adafruit_MCP9808 temperatureSensor = Adafruit_MCP9808();
RTC_PCF8523 rtc;
File logFile;

// -----------------------------------------------------------------------------
// Logging configuration
// -----------------------------------------------------------------------------
const int SD_CHIP_SELECT_PIN = 10;
const char *LOG_FILE_NAME = "datalog.csv";
const unsigned long LOG_SYNC_INTERVAL_MS = 900;
unsigned long lastLogSyncMs = 0;

// -----------------------------------------------------------------------------
// Geiger counter serial parsing
// Expected input is a comma-separated line containing CPS, CPM, and uSv/hr.
// -----------------------------------------------------------------------------
const size_t GEIGER_BUFFER_SIZE = 64;
char geigerReadBuffer[GEIGER_BUFFER_SIZE];
String geigerLine;
int commaLocations[6];

// -----------------------------------------------------------------------------
// Pressure sensor calibration and derived-flight calculations
// -----------------------------------------------------------------------------
const int PRESSURE_SENSOR_ADDRESS = 0x28;
const double PRESSURE_MIN_PSI = -15.0;
const double PRESSURE_MAX_PSI = 15.0;
const double AMBIENT_AIR_PRESSURE_PSI = 14.55;
const double SPECIFIC_GAS_CONSTANT = 53.33;
const double FEET_PER_METER = 3.28084;

uint16_t rawPressureCounts = 0;
uint8_t pressureStatus = 0;
uint8_t pressureDataBuffer[4];

// -----------------------------------------------------------------------------
// Launch detection
// Wait to begin logging until one acceleration axis exceeds this threshold.
// -----------------------------------------------------------------------------
const float LAUNCH_ACCELERATION_THRESHOLD = 30.0;
const unsigned long LAUNCH_POLL_DELAY_MS = 50;
const unsigned long LOOP_DELAY_MS = 100;

void writeTimestampToLog();
void writeTemperatureToLog(float &tempC, float &tempF);
void writeAccelerationToLog();
bool readGeigerLine();
void writeGeigerDataToLog();
void findCommaLocations();
void writePressureVelocityAndMachToLog(float tempC);
bool waitForLaunchDetection();
void writeEmptyGeigerColumns();

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);  // Geiger counter serial connection.
  Wire.begin();
  I2c.begin();

  // Needed only on boards with native USB serial.
  while (!Serial || !Serial1) {
    ;
  }

  // Initialize optional sensors. Failures are ignored so the sketch can keep
  // running and log whatever hardware is available.
  temperatureSensor.begin();
  accelerometer.begin();
  accelerometer.setRange(MMA8451_RANGE_8_G);
  rtc.begin();

  // Keep trying until the SD card is available.
  while (!SD.begin(SD_CHIP_SELECT_PIN)) {
    // Wait here until storage is present.
  }

  logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
  logFile.println("millis,datetime,tempC,tempF,Xaccel,Yaccel,Zaccel,cps,cpm,usv/hr,pressure,velocity,mach");

  // Do not start logging until the vehicle appears to have launched.
  waitForLaunchDetection();
}

void loop() {
  unsigned long nowMs = millis();
  logFile.print(nowMs);
  logFile.print(',');

  writeTimestampToLog();

  float tempC = 0.0;
  float tempF = 0.0;
  writeTemperatureToLog(tempC, tempF);
  writeAccelerationToLog();
  writeGeigerDataToLog();
  writePressureVelocityAndMachToLog(tempC);

  logFile.println();
  delay(LOOP_DELAY_MS);

  if ((millis() - lastLogSyncMs) >= LOG_SYNC_INTERVAL_MS) {
    lastLogSyncMs = millis();
    logFile.flush();
  }
}

void writeTimestampToLog() {
  DateTime now = rtc.now();

  logFile.print('"');
  logFile.print(now.year(), DEC);
  logFile.print('/');
  logFile.print(now.month(), DEC);
  logFile.print('/');
  logFile.print(now.day(), DEC);
  logFile.print(' ');
  logFile.print(now.hour(), DEC);
  logFile.print(':');
  logFile.print(now.minute(), DEC);
  logFile.print(':');
  logFile.print(now.second(), DEC);
  logFile.print('"');
  logFile.print(',');
}

void writeTemperatureToLog(float &tempC, float &tempF) {
  tempC = temperatureSensor.readTempC();
  tempF = tempC * 9.0 / 5.0 + 32.0;

  logFile.print(tempC);
  logFile.print(',');
  logFile.print(tempF);
  logFile.print(',');
}

void writeAccelerationToLog() {
  sensors_event_t accelerationEvent;
  accelerometer.read();
  accelerometer.getEvent(&accelerationEvent);

  logFile.print(accelerationEvent.acceleration.x);
  logFile.print(',');
  logFile.print(accelerationEvent.acceleration.y);
  logFile.print(',');
  logFile.print(accelerationEvent.acceleration.z);
  logFile.print(',');
}

bool readGeigerLine() {
  if (!Serial1.available()) {
    return false;
  }

  memset(geigerReadBuffer, 0, sizeof(geigerReadBuffer));
  Serial1.readBytesUntil('\n', geigerReadBuffer, GEIGER_BUFFER_SIZE);
  geigerLine = geigerReadBuffer;
  findCommaLocations();
  return true;
}

void writeGeigerDataToLog() {
  if (!readGeigerLine()) {
    writeEmptyGeigerColumns();
    return;
  }

  // Input format is assumed to be comma-separated. The original sketch used the
  // 2nd, 4th, and 6th fields for CPS, CPM, and uSv/hr respectively.
  logFile.print(geigerLine.substring(commaLocations[0] + 1, commaLocations[1]));
  logFile.print(',');
  logFile.print(geigerLine.substring(commaLocations[2] + 1, commaLocations[3]));
  logFile.print(',');
  logFile.print(geigerLine.substring(commaLocations[4] + 1, commaLocations[5]));
  logFile.print(',');
}

void writeEmptyGeigerColumns() {
  // Leave CPS, CPM, and uSv/hr blank when no serial data is available.
  logFile.print(",,,");
}

void findCommaLocations() {
  commaLocations[0] = geigerLine.indexOf(',');
  commaLocations[1] = geigerLine.indexOf(',', commaLocations[0] + 1);
  commaLocations[2] = geigerLine.indexOf(',', commaLocations[1] + 1);
  commaLocations[3] = geigerLine.indexOf(',', commaLocations[2] + 1);
  commaLocations[4] = geigerLine.indexOf(',', commaLocations[3] + 1);
  commaLocations[5] = geigerLine.indexOf(',', commaLocations[4] + 1);
}

void writePressureVelocityAndMachToLog(float tempC) {
  uint8_t error = I2c.read(PRESSURE_SENSOR_ADDRESS, 4, pressureDataBuffer);
  if (!error) {
    // The status is stored in the two most-significant bits of the first byte.
    pressureStatus = (pressureDataBuffer[0] & 0xC0) >> 6;

    // The remaining 14 bits contain the pressure reading.
    rawPressureCounts = (pressureDataBuffer[0] & 0x3F) << 8;
    rawPressureCounts += pressureDataBuffer[1];
  }

  double pressurePsi = PRESSURE_MIN_PSI +
      ((rawPressureCounts - 1638.0) / (14745.0 - 1638.0)) *
      (PRESSURE_MAX_PSI - PRESSURE_MIN_PSI);

  // Convert temperature and pressure into estimated air density, velocity, and
  // Mach number. These formulas are preserved from the original sketch.
  double airDensity =
      (AMBIENT_AIR_PRESSURE_PSI * 144.0) /
      (SPECIFIC_GAS_CONSTANT * (tempC + 459.69)) *
      (1.0 / 32.174);
  double velocityFeetPerSecond = sqrt(2.0 * pressurePsi * 144.0 / airDensity);
  double speedOfSoundMetersPerSecond = 331.0 + 0.6 * tempC;
  double speedOfSoundFeetPerSecond = speedOfSoundMetersPerSecond * FEET_PER_METER;
  double machNumber = velocityFeetPerSecond / speedOfSoundFeetPerSecond;

  logFile.print(pressurePsi);
  logFile.print(',');
  logFile.print(velocityFeetPerSecond);
  logFile.print(',');
  logFile.print(machNumber);
}

bool waitForLaunchDetection() {
  sensors_event_t accelerationEvent;

  while (true) {
    accelerometer.read();
    accelerometer.getEvent(&accelerationEvent);

    if (accelerationEvent.acceleration.x >= LAUNCH_ACCELERATION_THRESHOLD ||
        accelerationEvent.acceleration.y >= LAUNCH_ACCELERATION_THRESHOLD ||
        accelerationEvent.acceleration.z >= LAUNCH_ACCELERATION_THRESHOLD) {
      Serial.println("go");
      return true;
    }

    Serial.println("wait");
    Serial.println(accelerationEvent.acceleration.x);
    Serial.println(accelerationEvent.acceleration.y);
    Serial.println(accelerationEvent.acceleration.z);
    delay(LAUNCH_POLL_DELAY_MS);
  }
}
