#include <Arduino.h>
#include "Credentials.h"

// WIFI, MQTT & OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#define HOSTNAME "aquarium40"

// If not using the Credentials.h file you can add credentials here
#ifndef STASSID 
#define STASSID "ssid"
#define STAPSK  "passkey"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
//const char* mqtt_server = "broker.mqtt-dashboard.com";
IPAddress mqtt_server(192, 168, 10, 161);
boolean noWifiMode = false;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Neopixel
#include <Adafruit_NeoPixel.h>
#define NEOPIN 14
#define NUMPIXELS 14  //40L: 14, 70L 22 
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

// RTC
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
int sunriseHour = 9;
int sunriseMinute = 0;
int sunsetHour = 21;
int sunsetMinute = 0;
int moonStartHour = 22;
int moonStartMinute = 0;
int moonStopHour = 2;
int moonStopMinute = 0;
int duration = 15; // in minutes
int waitRGB = duration * 60 * 1000 / 255 / 2;
int waitWhite = duration * 60 * 1000 / 1024 / 2;

int moonBrightness = 150;
int moonNumberofleds = 4;

boolean daylight = false;
boolean EEPRomOverwrite = false;

// Millis
int period = 10000;
unsigned long time_now = 0;

// EEPROM
#include <EEPROM.h>

// Display
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };

static const unsigned char PROGMEM signal4_icon16x16[] =
{
	0b00000000, 0b00000000, //                 
	0b00000000, 0b00000100, //              #  
	0b00000000, 0b00001100, //             ##  
	0b00000000, 0b00011100, //            ###  
	0b00000000, 0b00011100, //            ###  
	0b00000000, 0b01011100, //          # ###  
	0b00000000, 0b11011100, //         ## ###  
	0b00000001, 0b11011100, //        ### ###  
	0b00000001, 0b11011100, //        ### ###  
	0b00000101, 0b11011100, //      # ### ###  
	0b00001101, 0b11011100, //     ## ### ###  
	0b00011101, 0b11011100, //    ### ### ###  
	0b00011101, 0b11011100, //    ### ### ###  
	0b01011101, 0b11011100, //  # ### ### ###  
	0b11011101, 0b11011100, // ## ### ### ###  
	0b11011101, 0b11011100, // ## ### ### ###  
};

// DISPLAY FUNCTIONS
	
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
		if (EEPRomOverwrite) {
			Serial.println("Reset EEPROM, saving default alarm times. Using following data:");
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
		else {
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

void clearRGB() {
  pixels.clear();
  pixels.show();
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

void setMoon1() {
	pixels.clear();
  for( int i = 2 ; i < NUMPIXELS / 2 ; i = i + 3 ) {
      pixels.setPixelColor( i, 0, 0, 100 );
   }  
   pixels.show();
}

void setMoon2() {
	pixels.clear();
  	for( int i = 2 ; i < NUMPIXELS / 2 ; i = i + 3 ) {
    	pixels.setPixelColor( i, 0, 0, 100 );
   	}  
   	for( int i = 3 ; i < NUMPIXELS ; i = i + 6 ) {
    	pixels.setPixelColor( i, 20, 20, 20 );
   	}  
   	pixels.show();
}

void setMoon3() {
	pixels.clear();
    pixels.setPixelColor( 13, 0, 0, 100 );
    pixels.setPixelColor( 14, 20, 20, 20 );
   pixels.show();
}

void moonrise (){
  for(size_t j = 0; j < moonBrightness ; j++){
    for( int i = 2 ; i < NUMPIXELS ; i = i + 3 ) {
      pixels.setPixelColor( i, 0, 0, j );
    }  
    pixels.show();
  }
} 

void moonset (){

}

// WIFI FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID 
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      Serial.println("send wakeupmassege");
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      Serial.println("subscribe");
      client.subscribe("homie/aquarium40/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
    Serial.println("MQTT reconnect done");
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

	int wifiTimeOut = 0;
  while (WiFi.status() != WL_CONNECTED) {
		if (wifiTimeOut < 60){
			delay(500);
			Serial.print(".");
			noWifiMode = false;
			wifiTimeOut++;
		}
		else {
			noWifiMode = true;
			break;
		}
  }

  randomSeed(micros());

	if (noWifiMode == false) {
		Serial.println("");
  	Serial.println("WiFi connected");
  	Serial.println("IP address: ");
  	Serial.println(WiFi.localIP());
	}
	else {
		Serial.println("");
		Serial.print("Could not connect to ");
		Serial.println(ssid);
		Serial.println("Started without wifi");
	}

}

// MQTT FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

void mqttSendInfo () {
  char tempBuffer[100] = "";
    sprintf(tempBuffer, "IP: %s\n", WiFi.localIP().toString().c_str());
    client.publish("homie/aquarium40/info", tempBuffer);
    sprintf(tempBuffer, "Suntrise: %02d:%02d\n", sunriseHour, sunriseMinute);
    client.publish("homie/aquarium40/info", tempBuffer);
    sprintf(tempBuffer, "Suntset: %02d:%02d\n", sunsetHour, sunsetMinute);
    client.publish("homie/aquarium40/info", tempBuffer);
    sprintf(tempBuffer, "duration: %02d\n", duration);
    client.publish("homie/aquarium40/info", tempBuffer);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char buf[20] = "";
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    buf[i] = (char)payload[i];
  }
  buf[length] = '\0';
  Serial.println();

  // Overwrite main light over mqtt
  if(strcmp(topic, "homie/aquarium40/light") == 0){
    int payloadAsInt = atoi ((char*)payload);
    analogWrite(12, map(payloadAsInt, 0, 100, 0, 1023 ));
    Serial.printf ("Main light at %d percent", payloadAsInt);
  } 
  // Overwrite RGB light over mqtt
  else if(strcmp(topic, "homie/aquarium40/rgb") == 0){
    int payloadAsInt = atoi ((char*)payload);
    switch (payloadAsInt) {
      case 0:
        Serial.println("RGB Off");
        clearRGB();
        break;
      case 1:
        Serial.println("RGB set 1");
        setRGB1();
        break;
      case 2:
        Serial.println("RGB set 2");
        break;
      default:
        // statements
        break;
    }
  } 
  // show moonlight over mqtt
  else if(strcmp(topic, "homie/aquarium40/moonlight") == 0){
    int payloadAsInt = atoi ((char*)payload);
    switch (payloadAsInt) {
      case 0:
        Serial.println("Moonlight Off");
		client.publish("homie/aquarium40/info","Moon off");
        clearRGB();
        break;
      case 1:
        Serial.println("Moon 1");
		client.publish("homie/aquarium40/info","Moon 1 set");
        setMoon1();
        break;
      case 2:
        Serial.println("Moon 2");
		client.publish("homie/aquarium40/info","Moon 2 set");
        setMoon2();
        break;
      case 3:
        Serial.println("Moon 2");
		client.publish("homie/aquarium40/info","Moon 2 set");
        setMoon3();
        break;
      default:
        // statements
        break;
    }
  } 
  // set new sunrise time over mqtt
  else if (strcmp(topic, "homie/aquarium40/sunrise") == 0) {
    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok((char*)payload,":");
    sunriseHour = atoi(strtokIndx); 

    strtokIndx = strtok(NULL, ":");
    sunriseMinute = atoi(strtokIndx);     

    EEPROM.begin(512);
    EEPROM.write(1, sunriseHour);
		EEPROM.write(2, sunriseMinute);
		EEPROM.commit();
		EEPROM.end();

    Serial.printf ("New sunrise time: %02d:%02d", sunriseHour, sunriseMinute);
		Serial.println();  
  }
  // set new sunset time over mqtt
  else if (strcmp(topic, "homie/aquarium40/sunset") == 0) {
    char * strtokIndx; 

    strtokIndx = strtok((char*)payload,":");  
    sunsetHour = atoi(strtokIndx); 
    strtokIndx = strtok(NULL, ":");
    sunsetMinute = atoi(strtokIndx);     

    EEPROM.begin(512);
    EEPROM.write(3, sunsetHour);
		EEPROM.write(4, sunsetMinute);
		EEPROM.commit();
		EEPROM.end();

    Serial.printf ("New sunset time: %02d:%02d", sunriseHour, sunriseMinute);
		Serial.println();  
  }
  // Set new sunrise and sunset duration
  else if (strcmp(topic, "homie/aquarium40/duration") == 0) {
    duration = atoi ((char*)payload);

    EEPROM.begin(512);
    EEPROM.write(5, duration);
		EEPROM.commit();
		EEPROM.end();
    Serial.printf ("New duration time: %d", duration);
		Serial.println();  
  } 
  // Request Ip from this controller
  else if(strcmp(topic, "homie/aquarium40/requestip") == 0){
    char tempBuffer[20] = "";
    sprintf(tempBuffer, "IP: %s\n", WiFi.localIP().toString().c_str());
    client.publish("homie/aquarium40/ip", tempBuffer);
  }
  // Request Ip from this controller
  else if(strcmp(topic, "homie/aquarium40/requestinfo") == 0){
    mqttSendInfo();
  }
  // Simulate sunrise
  else if(strcmp(topic, "homie/aquarium40/dosunrise") == 0){
    sunrise();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect. Needed for native USB on ESP8266
  delay(5000);
  Serial.println("Serial started");

  // OLED
  // start display
	// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();

  // Draw a single pixel in white
  display.drawPixel(10, 10, SSD1306_WHITE);

  // Show the display buffer on the screen. You MUST call display() after
  // drawing commands to make them visible on screen!
  display.display();
  delay(2000);


  // WIFI
  setup_wifi();
	if (noWifiMode == false) {
		client.setServer(mqtt_server, 1883);
		client.setCallback(callback);

		/*
		WiFi.mode(WIFI_STA);
		WiFi.begin(ssid, password);
		while (WiFi.waitForConnectResult() != WL_CONNECTED) {
			Serial.println("Connection Failed! Rebooting...");
			delay(5000);
			ESP.restart();
		}*/

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
	}	
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
  
	if (noWifiMode = false) {
		if (!client.connected()) {
			reconnect();
		}
		client.loop();
		ArduinoOTA.handle();
	}

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