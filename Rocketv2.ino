#include <Wire.h>
#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_MCP9808.h"
#include <RTClib.h>
#include <SD.h>
#include <I2C.h>

Adafruit_MMA8451 mma = Adafruit_MMA8451();
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();
File logfile;
RTC_PCF8523 rtc;

const int chipSelect = 10;
const int accelerationThreshold = 30;
const uint8_t geigerBufferSize = 64;
const uint8_t pressureSensorAddress = 0x28;
const double pmin = -15.0;
const double pmax = 15.0;
const double airpressure = 14.55;
const double R = 53.33;

#define SYNC_INTERVAL 900  // ms between calls to flush()

uint32_t syncTime = 0;
char readBuffer[geigerBufferSize];
String readString;
int commaLocations[6];
uint16_t pressure;
double pressurePSI;
uint8_t status;
uint8_t dataBuf[4];

void FindCommaLocations();

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  Wire.begin();
  I2c.begin();

  while (!Serial || !Serial1) {
    ;  // wait for serial ports to connect
  }

  if (!tempsensor.begin()) {
    // Serial.println("Couldn't find MCP9808!");
    // while (1);
  }

  if (!mma.begin()) {
    // Serial.println("Couldnt start accelerometer");
    // while (1);
  }

  mma.setRange(MMA8451_RANGE_8_G);

  if (!rtc.begin()) {
    // Serial.println("RTC failed.");
  }

  if (!rtc.initialized()) {
    // Serial.println("RTC is NOT running!");
  }

  while (!SD.begin(chipSelect)) {
    // Serial.println("Card failed, or not present");
  }

  logfile = SD.open("datalog.csv", FILE_WRITE);
  logfile.println(
      "millis,datetime,tempC,tempF,Xaccel,Yaccel,Zaccel,cps,cpm,usv/hr,pressure,velocity,mach");

  mma.read();
  sensors_event_t launchEvent;
  mma.getEvent(&launchEvent);

  while (launchEvent.acceleration.x < accelerationThreshold &&
         launchEvent.acceleration.y < accelerationThreshold &&
         launchEvent.acceleration.z < accelerationThreshold) {
    Serial.println("wait");
    mma.read();
    mma.getEvent(&launchEvent);
    Serial.println(launchEvent.acceleration.x);
    Serial.println(launchEvent.acceleration.y);
    Serial.println(launchEvent.acceleration.z);
    delay(50);
  }

  Serial.println("go");
}

void loop() {
  Serial.println("in");

  DateTime now = rtc.now();
  uint32_t m = millis();

  logfile.print(m);
  logfile.print(",");
  logfile.print('"');
  logfile.print(now.year(), DEC);
  logfile.print("/");
  logfile.print(now.month(), DEC);
  logfile.print("/");
  logfile.print(now.day(), DEC);
  logfile.print(" ");
  logfile.print(now.hour(), DEC);
  logfile.print(":");
  logfile.print(now.minute(), DEC);
  logfile.print(":");
  logfile.print(now.second(), DEC);
  logfile.print('"');
  logfile.print(",");

  float c = tempsensor.readTempC();
  float f = c * 9.0 / 5.0 + 32;
  logfile.print(c);
  logfile.print(",");
  logfile.print(f);
  logfile.print(",");

  mma.read();
  sensors_event_t event;
  mma.getEvent(&event);

  logfile.print(event.acceleration.x);
  logfile.print(",");
  logfile.print(event.acceleration.y);
  logfile.print(",");
  logfile.print(event.acceleration.z);
  logfile.print(",");

  if (Serial1.available()) {
    Serial1.readBytesUntil('\n', readBuffer, geigerBufferSize);
    readString = readBuffer;
    FindCommaLocations();

    logfile.print(readString.substring(commaLocations[0] + 1, commaLocations[1]));
    logfile.print(",");
    logfile.print(readString.substring(commaLocations[2] + 1, commaLocations[3]));
    logfile.print(",");
    logfile.print(readString.substring(commaLocations[4] + 1, commaLocations[5]));
    logfile.print(",");
  } else {
    logfile.print(",,,");
  }

  uint8_t error = 0;
  error |= I2c.read(pressureSensorAddress, 4, dataBuf);
  if (!error) {
    status = (dataBuf[0] & 0xC0) >> 6;
    pressure = (dataBuf[0] & 0x3F) << 8;
    pressure += dataBuf[1];
  }

  pressurePSI = pmin + double((pressure - 1638.0) / (14745.0 - 1638.0) * (pmax - pmin));
  logfile.print(pressurePSI);
  logfile.print(",");

  double airdensity = (airpressure * 144.0) / ((R * (c + 459.69))) * (1 / 32.174);
  double velocity = sqrt(2.0 * pressurePSI * 144.0 / airdensity);
  logfile.print(velocity);
  logfile.print(",");

  double soundspeed = 331 + 0.6 * c;
  double soundspeedftpers = soundspeed * 3.28084;
  double mach = velocity / soundspeedftpers;
  logfile.print(mach);
  logfile.println();

  delay(100);

  if ((millis() - syncTime) < SYNC_INTERVAL) {
    return;
  }

  syncTime = millis();
  logfile.flush();
}

void FindCommaLocations() {
  commaLocations[0] = readString.indexOf(',');
  commaLocations[1] = readString.indexOf(',', commaLocations[0] + 1);
  commaLocations[2] = readString.indexOf(',', commaLocations[1] + 1);
  commaLocations[3] = readString.indexOf(',', commaLocations[2] + 1);
  commaLocations[4] = readString.indexOf(',', commaLocations[3] + 1);
  commaLocations[5] = readString.indexOf(',', commaLocations[4] + 1);
}
