#include <RTClib.h>
#include <TimerOne.h>
#include <U8x8lib.h>
#include "Biquad.h"
#include <Wire.h>
#include <SD.h>
#include "Biquad.h"
#include "MovingAverage.h"

#define SD_CS 4

//U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);

RTC_DS1307 rtc;

File logFile;

char filePath[30];
char dataBuf[20];
bool sdReady = false;

MovingAverage ma(5);

// begin sharp sensor
int SHARP_ADC_PIN = A6;
int SHARP_LED_POWER = 3;
int ADC_DEBUG_PIN = 5;

int samplingTime = 280;
int deltaTime = 40;

int samplingFreq = 100;

volatile int rawValue = 0;
volatile bool dataReady = false;
volatile bool adcStarted = false;

float filteredValue = 0;
long sumValue = 0;
int measurementsCount = 0;

long sdLoggingInterval;
long displayRefreshInterval;
float dustDensity = 0;

// ------------------

Biquad lpFilter(bq_type_lowpass, 0.2 / samplingFreq, 0.707, 0);

void setup()
{
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);

  rtc.begin();

  sdReady = SD.begin(SD_CS);

  if (sdReady)
  {
    SD.mkdir("/GP2Y1014");
  }

  pinMode(SHARP_LED_POWER, OUTPUT);
  pinMode(ADC_DEBUG_PIN, OUTPUT);

  Timer1.initialize(1000000/samplingFreq);
  Timer1.attachInterrupt(pollSensor);

  u8x8.drawString(0,0, sdReady ? "SD OK" : "NO SD");

  delay(200);

  displayRefreshInterval = millis();
  sdLoggingInterval = millis();

  // set the analog reference (high two bits of ADMUX) and select the
  // channel (low 4 bits). this also sets ADLAR (left-adjust result)
  // to 0 (the default).
  ADMUX = bit (REFS0) | ((SHARP_ADC_PIN - PIN_A0) & 0x07);
}

// ADC complete Interupt Service Routine
ISR (ADC_vect)
{
	rawValue = ADC;
	dataReady = true;
}
// end of ADC_vect

void pollSensor(void)
{
	// Check the conversion hasn't been started already
	if (!adcStarted)
	{
		adcStarted = true;
		digitalWrite(SHARP_LED_POWER,LOW); // power on the LED
		delayMicroseconds(samplingTime);
		// start the conversion
		ADCSRA |= bit (ADSC) | bit (ADIE);
		delayMicroseconds(deltaTime);
		digitalWrite(SHARP_LED_POWER,HIGH); // turn the LED off
	}
}

void loop()
{
	if (adcStarted && !dataReady)
	{
		return;
	}

  if (dataReady)
  {
    filteredValue = lpFilter.process(rawValue);
    sumValue = sumValue + rawValue;
    measurementsCount++;
    dataReady = false;
    adcStarted = false;
  }

  if (millis() - displayRefreshInterval > 1000)
  {
    // update display
    u8x8.drawString(0,0, sdReady ? "SD OK" : "NO SD");

    // linear eqaution taken from http://www.howmuchsnow.com/arduino/airquality/
    // Chris Nafis (c) 2012
    dustDensity = constrain((0.17 * (filteredValue * (5.0 / 1024)) - 0.1)*1000, 0, 500); // ug/m3

    DateTime now = rtc.now();
    sprintf(dataBuf,"%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    u8x8.drawString(0,1,dataBuf);

    u8x8.setCursor(0, 3);
    u8x8.print(dustDensity, 3);
    u8x8.setCursor(0, 4);
    u8x8.print(filteredValue);
    u8x8.setCursor(0, 5);
    u8x8.print(rawValue);
    u8x8.setCursor(0, 6);
    u8x8.print(rawValue * (5.0 / 1024), 3);

    displayRefreshInterval = millis();
  }

  if (sdReady && (millis() - sdLoggingInterval > 60000))
  {
	  double voltage = sumValue * (5.0 / 1024)/measurementsCount;

    dustDensity = 172 * voltage - 100; // ug/m3

    DateTime now = rtc.now();
    sprintf(filePath, "/GP2Y1014/%04d%02d%02d.csv", now.year(), now.month(), now.day());

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

      logFile.print(ma.process(dustDensity), 2);
      logFile.print(",");

      logFile.println(voltage);

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

