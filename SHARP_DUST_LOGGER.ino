#include <RTClib.h>
#include <TimerOne.h>
#include <U8x8lib.h>
#include "Biquad.h"
#include <Wire.h>
#include <SD.h>
#include "Biquad.h"

#define SD_CS 4

//U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

RTC_DS1307 rtc;

File logFile;

char filePath[25];
char dataBuf[12];
bool sdReady = false;

// begin sharp sensor
int measurePin = A6;
int SHARP_LED_POWER = 3;

int samplingTime = 280;
int deltaTime = 40;

int samplingFreq = 10;

volatile float rawValue = 0;
volatile bool dataReady = false;

float filteredValue = 0;
float sumValue = 0;
int measurementsCount = 0;

long displayRefreshInterval;
long sdLoggingInterval;
float dustDensity = 0;

// ------------------

Biquad lpFilter(bq_type_lowpass, 0.2 / samplingFreq, 0.707, 0);
Biquad lpFilter25(bq_type_lowpass, 0.2, 0.707, 0);
Biquad lpFilter002(bq_type_lowpass, 0.02, 0.707, 0);

void setup()
{
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);

  rtc.begin();

  sdReady = SD.begin(SD_CS);

  if (sdReady)
  {
    SD.mkdir("/DATA");
  }

  pinMode(SHARP_LED_POWER, OUTPUT);

  Timer1.initialize(1000000/samplingFreq);
  Timer1.attachInterrupt(pollSensor);

  u8x8.drawString(0,0, sdReady ? "SD OK" : "NO SD");

  delay(200);

  displayRefreshInterval = millis();
  sdLoggingInterval = millis();
}

void pollSensor(void)
{
  digitalWrite(SHARP_LED_POWER,LOW); // power on the LED
  delayMicroseconds(samplingTime);
  rawValue = analogRead(measurePin); // read the dust value
  delayMicroseconds(deltaTime);
  digitalWrite(SHARP_LED_POWER,HIGH); // turn the LED off
  dataReady = true;
}

void loop()
{
  if (dataReady)
  {
    filteredValue = lpFilter.process(rawValue);
    sumValue = sumValue + filteredValue;
    measurementsCount++;
    dataReady = false;
  }

  if (millis() - displayRefreshInterval > 1000)
  {
    // update display
    u8x8.drawString(0,0, sdReady ? "SD OK" : "NO SD");

    // linear eqaution taken from http://www.howmuchsnow.com/arduino/airquality/
    // Chris Nafis (c) 2012
    dustDensity = (0.17 * (filteredValue * (5.0 / 1024)) - 0.1)*1000; // ug/m3

    DateTime now = rtc.now();
    sprintf(dataBuf,"%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    u8x8.drawString(0,1,dataBuf);

    dtostrf(dustDensity, 2, 1, dataBuf);
    u8x8.drawString(0,3,dataBuf);

    displayRefreshInterval = millis();
  }

  if (sdReady && (millis() - sdLoggingInterval > 60000))
  {
    dustDensity = (0.17 * ((sumValue/measurementsCount) * (5.0 / 1024)) - 0.1)*1000; // ug/m3

    DateTime now = rtc.now();
    sprintf(filePath, "/DATA/%04d%02d%02d.csv", now.year(), now.month(), now.day());

    logFile = SD.open(filePath, FILE_WRITE);

    if (logFile)
    {
      sprintf(dataBuf,"%04d-%02d-%02d", now.year(), now.month(), now.day());
      logFile.print(dataBuf);
      logFile.print(" ");

      sprintf(dataBuf,"%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      logFile.print(dataBuf);
      logFile.print(",");

      logFile.print(dustDensity, 2);
      logFile.print(",");
      logFile.print(lpFilter25.process(dustDensity), 2);
      logFile.print(",");
      logFile.println(lpFilter002.process(dustDensity), 2);

      logFile.close();
    }
    else
    {
      u8x8.drawString(0,0, "** SD ERROR **");
    }

    sumValue = 0;
    measurementsCount = 0;
    sdLoggingInterval = millis();
  }
}

