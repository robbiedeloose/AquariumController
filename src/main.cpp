#include <Arduino.h>
#include "Credentials.h"

// WIFI & OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#define HOSTNAME "Aquarium-40"

// If not using the Credentials.h file you can add credentials here
#ifndef STASSID 
#define STASSID "ssid"
#define STAPSK  "passkey"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

// Neopixel
#include <Adafruit_NeoPixel.h>
#define NEOPIN 14
#define NUMPIXELS 22
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

// RTC
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
int sunriseHour = 9;
int sunriseMinute = 0;
int sunsetHour = 21;
int sunsetMinute = 0;
int duration = 1; // in minutes
int waitRGB = duration * 60 * 1000 / 255 / 2;
int waitWhite = duration * 60 * 1000 / 1024 / 2;
boolean daylight = false;

// Millis
int period = 10000;
unsigned long time_now = 0;

// EEPROM
#include <EEPROM.h>


// TIME FUNCTIONS ///////////////////////////////////////////////////////////////////////////////////////////////

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

boolean checkTime(const RtcDateTime& dt,int setHour, int setMinute){
  int hourNow = dt.Hour();
  int minuteNow = dt.Minute();  
  if (hourNow == setHour) {
    if (minuteNow == setMinute) {
      return true;
    }
    else return false;
  }
  else return false;
}

//EEPROM FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////
void startEEPRom () {
	  EEPROM.begin(512);
		int EEPRomIsSet = EEPROM.read(0);
		if ( EEPRomIsSet == 1 ) {
			Serial.println("Alarm time is allready set in EEPROM. I found the following data:");
			sunriseHour = EEPROM.read(1);
			sunriseMinute = EEPROM.read(2);
			sunsetHour = EEPROM.read(3);
			sunsetMinute = EEPROM.read(4);
			duration = EEPROM.read(5);
			Serial.printf ("Sunrise: %02d:%02d, sunset: %02d:%02d, Duration: %d minutes", sunriseHour, sunriseMinute, sunsetHour , sunsetMinute, duration);
			Serial.println();
		} else {
			Serial.println("No Alarm time is EEPROM, saving default alarm times. Using following data:");
			EEPROM.write(0, 1);
			EEPROM.write(1, sunriseHour);
			EEPROM.write(2, sunriseMinute);
			EEPROM.write(3, sunsetHour);
			EEPROM.write(4, sunsetMinute);
			EEPROM.write(5, duration);
			EEPROM.commit();
			EEPROM.end();
			Serial.printf ("Sunrise: %02d:%02d, sunset: %02d:%02d, Duration: %d minutes", sunriseHour, sunriseMinute, sunsetHour , sunsetMinute, duration);
			Serial.println();
		}
}

//RGB FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////

void sunrise() {
  Serial.println("Start Sunrise animation");
  for(int i = 0 ; i<255;i++){
    pixels.fill(pixels.Color(i,0,0),0, 22);
    pixels.show(); 
    delay(waitRGB);
  }
  for(int i = 0; i<1024;i++){
    analogWrite(12, i);
    delay(waitWhite);
    //Serial.println(i);
  }
     for(int i = 255 ; i>=0 ; i--){
    pixels.fill(pixels.Color(i,0,0),0, 22);
    pixels.show(); 
    delay(waitRGB);
  }
  Serial.println("Stop Sunrise animation");
}

void sunset() {
  Serial.println("Start Sunset animation");
  for( int i = 0 ; i<255;i++ ){
    pixels.fill(pixels.Color(i,0,0),0, 22);
    pixels.show(); 
    delay(waitRGB);
  }
  for( int i = 1024; i>=0;i-- ){
    analogWrite(12, i);
    delay(waitWhite);
  }
   for( int i = 255 ; i>=0 ; i-- ){
    pixels.fill(pixels.Color(i,0,0),0, 22);
    pixels.show(); 
    delay(waitRGB);
  }
  Serial.println("Stop Sunset animation");
}

void setRGB1() {
  Serial.println("Set strip to full spectrum v1");
  for( int i = 0 ; i < NUMPIXELS ; i = i + 3 ) {
    pixels.setPixelColor( i, 255, 0, 0 );
  }
   for( int i = 1 ; i < NUMPIXELS ; i = i + 3 ) {
    pixels.setPixelColor( i, 0, 255, 0 );
  }
   for( int i = 2 ; i < NUMPIXELS ; i = i + 3 ) {
    pixels.setPixelColor( i, 0, 0, 255 );
  }
  pixels.show();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect. Needed for native USB on ESP8266
  delay(5000);
  Serial.println("Serial started");

  // WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
	#ifdef HOSTNAME 
	ArduinoOTA.setHostname(HOSTNAME);
	#endif
  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Led strips //
  pinMode(12, OUTPUT);
  pixels.begin();

  // Setup RTC //
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);


  if (!Rtc.IsDateTimeValid()) 
  {
      if (Rtc.LastError() != 0)
      {
		// we have a communications error
		// see https://www.arduino.cc/en/Reference/WireEndTransmission for 
		// what the number means
		Serial.print("RTC communications error = ");
		Serial.println(Rtc.LastError());
      }
      else
      {   
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
      }
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
     Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 

	startEEPRom();

  ////////////////////////
  // check if light should be on at startup
	int minutesSinceMidnight = now.Hour() * 60 + now.Minute();
	int minutesOn = sunriseHour * 60 + sunriseMinute;
	int minutesOff = sunsetHour * 60 + sunsetMinute;

	if (minutesSinceMidnight > minutesOn) {
		 daylight = true;
		 if (minutesSinceMidnight > minutesOff) {
			 daylight = false;
		 }
	}
	else {
		daylight = false;
	}	
	if (daylight) {
		sunrise();
  	setRGB1();
	}

  
}

void loop() {
  ArduinoOTA.handle();

  if (millis() > time_now + period) {
		time_now = millis();
		
		if (!Rtc.IsDateTimeValid()) 
    {
        if (Rtc.LastError() != 0)
        {
            // we have a communications error
            // see https://www.arduino.cc/en/Reference/WireEndTransmission for 
            // what the number means
            Serial.print("RTC communications error = ");
            Serial.println(Rtc.LastError());
        }
        else
        {
            // Common Causes:
            //    1) the battery on the device is low or even missing and the power line was disconnected
            Serial.println("RTC lost confidence in the DateTime!");
        }
    }

  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();

	RtcTemperature temp = Rtc.GetTemperature();
	temp.Print(Serial);
	// you may also get the temperature as a float and print it
    // Serial.print(temp.AsFloatDegC());
    Serial.println("C");

    //// Check sunrise and sunset 
    if (checkTime(now, sunriseHour, sunriseMinute)) {
      daylight = 1;
      sunrise();
      setRGB1();
    }
    if (checkTime(now, sunsetHour, sunsetMinute)) {
      daylight = 0;
      sunset();
    }
    Serial.print("Daylight: ");
    Serial.println(daylight);
	}
}