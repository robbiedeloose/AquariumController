#include <Arduino.h>
#include "Credentials.h"

// WIFI, MQTT & OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define HOSTNAME "aquarium60"
#define VERSION 0.4

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
boolean mqttServerConnected = false;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// PINS
#define PIN_PUMP1 0
#define PIN_WHITE_LEDS 12
#define PIN_RELAY_CO2 13
#define PIN_RELAY_AIR 15
bool co2;
bool air;

#define NEOPIN 14
#define ONE_WIRE_BUS 13

// Neopixel
#include <Adafruit_NeoPixel.h>
// #define NEOPIN 14 --> see pin at pin defines
#define NUMPIXELS 44  //40L: 14, 70L 22, 60l 44
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIN, NEO_GRB + NEO_KHZ800);

// Temp sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;
float waterTemp;
float tempOffset = -1;
// RTC
#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS3231.h>
RtcDS3231<TwoWire> Rtc(Wire);
int sunriseHour = 9;
int sunriseMinute = 0;
int sunsetHour = 21;
int sunsetMinute = 0;
int moonriseHour = 22;
int moonriseMinute = 0;
int moonsetHour = 2;
int moonsetMinute = 0;
int airStartHour = 21;
int airStartMinute = 0;
int airStopHour = 2;
int airStopMinute = 0;
int duration = 15; // in minutes
int waitRGB = duration * 60 * 1000 / 255 / 2;
int waitWhite = duration * 60 * 1000 / 1024 / 2;

int moonBrightness = 150;
int moonNumberofleds = 15;

boolean daylight = false;
boolean EEPRomOverwrite = false;

int pump1Dosage = 10; // 10 ml check and calculate running time
int pump1RunDays[7] = {1,0,0,0,0,0,0}; // mo, tue, wen, thu, fri, sa, su

// Millis
int period = 5000;
unsigned long time_now = 0;

// EEPROM
#include <EEPROM.h>

// Display
#include <Adafruit_I2CDevice.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
int screenNumber = 4;

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
    Serial.println();
    Serial.println(dt.DayOfWeek());

}

void printDateToDisplay (const RtcDateTime& dt)
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
    display.println(datestring);
    display.display();
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

void writeValuesToEEPRom (){
  		EEPROM.write(0, 1);
			EEPROM.write(1, sunriseHour);
			EEPROM.write(2, sunriseMinute);
			EEPROM.write(3, sunsetHour);
			EEPROM.write(4, sunsetMinute);
			EEPROM.write(5, duration);
      EEPROM.write(6, moonriseHour);
      EEPROM.write(7, moonriseMinute);
      EEPROM.write(8, moonsetHour);
      EEPROM.write(9, moonsetMinute);
      EEPROM.write(10, airStartHour);
      EEPROM.write(11, airStartMinute);
      EEPROM.write(12, airStopHour);
      EEPROM.write(13, airStopMinute);
			EEPROM.commit();
			EEPROM.end();
}

void readValuesFromEEPRom(){
  			sunriseHour = EEPROM.read(1);
				sunriseMinute = EEPROM.read(2);
				sunsetHour = EEPROM.read(3);
				sunsetMinute = EEPROM.read(4);
				duration = EEPROM.read(5);
        moonriseHour = EEPROM.read(6);
        moonriseMinute = EEPROM.read(7);
        moonsetHour = EEPROM.read(8);
        moonsetMinute = EEPROM.read(9);
        airStartHour = EEPROM.read(10);
        airStartMinute = EEPROM.read(11);
        airStopHour = EEPROM.read(12);
        airStopMinute = EEPROM.read(13);
}

//EEPROM FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////
void startEEPRom () {
	  EEPROM.begin(512);
		int EEPRomIsSet = EEPROM.read(0);
    if ( EEPRomIsSet == 1 ) {
      Serial.println("Alarm time is allready set in EEPROM. Found the following data:");
      readValuesFromEEPRom();
    } else {
      Serial.println("No Alarm time is EEPROM, saving default alarm times. Using following data:");
      writeValuesToEEPRom();
    }
    waitRGB = duration * 60 * 1000 / 255 / 2;
    waitWhite = duration * 60 * 1000 / 1024 / 2;
    Serial.printf ("LIGHT: start %02d:%02d, stop: %02d:%02d, Duration: %d minutes\n", sunriseHour, sunriseMinute, sunsetHour , sunsetMinute, duration);
    Serial.printf ("MOON:  start %02d:%02d, stop: %02d:%02d, Duration: %d minutes\n", moonriseHour, moonriseMinute, moonsetHour , moonsetMinute, duration);
}

void resetEEPRom () {
  EEPROM.begin(512);
  EEPROM.write(0, 0);
  EEPROM.commit();
	EEPROM.end();
  ESP.restart();
}

//RGB FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////

void sunrise() {
  Serial.println("Start Sunrise animation");
  for(int i = 0 ; i<255;i++){
    pixels.fill(pixels.Color(i,0,0),0, NUMPIXELS);
    pixels.show(); 
    delay(waitRGB);
  }
  for(int i = 0; i<1024;i++){
    analogWrite(12, i);
    delay(waitWhite);
  }
     for(int i = 255 ; i>=0 ; i--){
    pixels.fill(pixels.Color(i,0,0),0, NUMPIXELS);
    pixels.show(); 
    delay(waitRGB);
  }
  Serial.println("Stop Sunrise animation");
  client.publish("homie/aquarium60/light", "100");
}

void sunset() {
  Serial.println("Start Sunset animation");
  for( int i = 0 ; i<255;i++ ){
    pixels.fill(pixels.Color(i,0,0),0, NUMPIXELS);
    pixels.show(); 
    delay(waitRGB);
  }
  for( int i = 1024; i>=0;i-- ){
    analogWrite(12, i);
    delay(waitWhite);
  }
   for( int i = 255 ; i>=0 ; i-- ){
    pixels.fill(pixels.Color(i,0,0),0, NUMPIXELS);
    pixels.show(); 
    delay(waitRGB);
  }
  Serial.println("Stop Sunset animation");
    client.publish("homie/aquarium60/light", "0");
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
  pixels.setBrightness(255);
  pixels.show();
}
void setRGB2() {
  Serial.println("Set strip to full spectrum v2");
  for( int i = 0 ; i < NUMPIXELS ; i++ ) {
    pixels.setPixelColor( i, 0, 0, 255 );
  }
  pixels.setBrightness(255);
  pixels.show();
}
void setRGB3() {
  Serial.println("Set strip to full spectrum v2");
  for( int i = 0 ; i < NUMPIXELS ; i++ ) {
    pixels.setPixelColor( i, 255, 0, 0 );
  }
  pixels.setBrightness(255);
  pixels.show();
}
void setRGB4() {
  Serial.println("Set strip to full spectrum v2");
  for( int i = 0 ; i < NUMPIXELS ; i++ ) {
    pixels.setPixelColor( i, 0, 255, 0 );
  }
  pixels.setBrightness(255);
  pixels.show();
}
void setRGB5() {
  Serial.println("Set strip to full spectrum v2");
  for( int i = 0 ; i < NUMPIXELS ; i = i + 3 ) {
    pixels.setPixelColor( i, 255, 0, 0 );
  }
   for( int i = 1 ; i < NUMPIXELS ; i = i + 3 ) {
    pixels.setPixelColor( i, 0, 255, 0 );
  }
   for( int i = 2 ; i < NUMPIXELS ; i = i + 3 ) {
    pixels.setPixelColor( i, 0, 0, 255 );
  }
  for( int i = 1 ; i < NUMPIXELS ; i = i + 6 ) {
    pixels.setPixelColor( i, 0, 0, 255 );
  }
  pixels.setBrightness(255);
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

void setMoon4() {
	pixels.clear();
  	for( int i = 2 ; i < NUMPIXELS / 2 ; i = i + 3 ) {
    	pixels.setPixelColor( i, 0, 0, 255 );
   	}  
   	for( int i = 3 ; i < NUMPIXELS ; i = i + 6 ) {
    	pixels.setPixelColor( i, 20, 20, 20 );
   	}  
   	pixels.show();
}

void moonrise (){
  for(int j = 0; j < moonBrightness ; j++){
    for( int i = 2 ; i < NUMPIXELS ; i = i + 3 ) {
      pixels.setPixelColor( i, 0, 0, j );
    }  
    pixels.show();
    delay(waitRGB);
  }
} 

void moonset (){
  for(int j = 255; j < moonBrightness ; j--){
    for( int i = 2 ; i < NUMPIXELS ; i = i + 3 ) {
      pixels.setPixelColor( i, 0, 0, j );
    }  
    pixels.show();
    delay(waitRGB);
  }
}

// WIFI FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

void reconnect() {
	int mqttTimeOut = 0;
  while (!client.connected()) {
    if (mqttTimeOut < 5) {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID 
      String clientId = "ESP8266Client-";
      clientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (client.connect(clientId.c_str())) {
        Serial.println("connected");
        Serial.println("send wakeupmassege");
        client.publish("homie/aquarium60/status", "reconnected");
        // ... and resubscribe
        Serial.println("subscribe");
        client.subscribe("homie/aquarium60/#");
        mqttServerConnected = true;
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 1 seconds");
        delay(1000);
      }
      mqttTimeOut++;
    }
    else{
      Serial.println("Not connected to MQTT");
      mqttServerConnected = false;
      return;
    }
  }
  Serial.println("MQTT reconnect done");
	mqttServerConnected = true;
}

void setup_wifi() {

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  display.println("conencting to");
  display.println(ssid);
  display.display(); 

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

	int wifiTimeOut = 0;
  while (WiFi.status() != WL_CONNECTED) {
		if (wifiTimeOut < 60){
			delay(500);
			Serial.print(".");
      display.print(".");
      display.display(); 
			noWifiMode = false;
			wifiTimeOut++;
		}
		else {
			noWifiMode = true;
      display.println("not connected");
      display.display(); 
			return;
		}
  }

  randomSeed(micros());

	if (noWifiMode == false) {
		Serial.println("");
  	Serial.println("WiFi connected");
  	Serial.print("IP address: ");
  	Serial.println(WiFi.localIP());
	}
	else {
		Serial.println("");
		Serial.print("Could not connect to ");
		Serial.println(ssid);
		Serial.println("Started without wifi");
	}

}

// Display main screens

void showCurrentScreen(){
  switch (screenNumber) {
      case 1:
        // TEMPERATURE
				display.clearDisplay();
				display.setCursor(0,0);
				display.setTextSize(1);
        display.println("Temperature");
        display.drawLine(0,11,display.width()-1,11,WHITE);
        display.setCursor(0, 17);
				display.setTextSize(2);
        display.print("IN   ");
				//display.print(temp.AsFloatDegC(), 1);
				display.println("C");
        display.setCursor(0, 36);
        display.setTextSize(2);
        display.print("OUT  ");
				//display.print(temp.AsFloatDegC(), 1);
				display.println("C");
				display.display();
        break;
      case 2:
        // AIR & CO2
        display.clearDisplay();
				display.setCursor(0,0);
				display.setTextSize(1);
        display.println("Technics");
        display.drawLine(0,11,display.width()-1,11,WHITE);
        display.setCursor(0, 17);
				display.setTextSize(2);
				display.print("CO2: ");
        if (co2) display.println("on");
        else display.println("off");
				display.print("Air: ");
        if (air) display.println("on");
        else display.println("off");
				display.display();
        break;
      case 3:
        // NETWORK
				display.clearDisplay();
				display.setCursor(0,0);
				display.setTextSize(1);
        display.println("Network");
        display.drawLine(0,11,display.width()-1,11,WHITE);
        display.setCursor(0, 17);
        display.setTextSize(2);
				display.print("wifi: ");
				if (noWifiMode){
					display.println("--");
				}
				else {
					display.println("ok");
				}
        display.setCursor(0, 36);
        display.setTextSize(2);
				display.print("mqtt: ");
				if (mqttServerConnected){
					display.println("ok");
				}
				else {
					display.println("--");
				}
				display.setCursor(0, 57);
        display.setTextSize(1);
        display.print("ip: ");
        display.println(WiFi.localIP());
				display.display();
        break;
      case 4:
        // LIGHT
        display.clearDisplay();
				display.setCursor(0,0);
				display.setTextSize(1);
        display.println("Light");
        display.drawLine(0,11,display.width()-1,11,WHITE);
        display.setCursor(0, 17);
				display.setTextSize(1);
        display.printf("booja: %02d:%02d", sunriseHour, sunriseMinute);
				display.print("Sunrise:  ");
				display.println("10:00");
				display.print("Sunset:   ");
				display.println("20:00");
        display.print("Moonrise: ");
				display.println("22:10");
				display.print("Moonset:  ");
				display.println("23:30");
        display.println("Duration: 15 minutes");
				display.display();
        break;
      case 5:
        // MOON
        display.clearDisplay();
				display.setCursor(0,0);
				display.setTextSize(1);
        display.println("Moon");
        display.drawLine(0,11,display.width()-1,11,WHITE);
        display.setCursor(0, 17);
				display.setTextSize(2);
				display.print("On: ");
				display.println("10:00");
				display.print("Off: ");
				display.println("20:00");
				display.display();
        break;
      default:
        // statements
        break;
    }

		screenNumber++;
		if (screenNumber > 4) screenNumber = 1;
}

// MQTT FUNCTIONS ///////////////////////////////////////////////////////////////////////////////

void mqttSendInfo () {
  char tempBuffer[100] = "";
    sprintf(tempBuffer, "%s\n", WiFi.localIP().toString().c_str());
    client.publish("homie/aquarium60/ip", tempBuffer);
    RtcDateTime dt = Rtc.GetDateTime();
    sprintf(tempBuffer, "%02u/%02u/%04u %02u:%02u:%02u\n", dt.Month(), dt.Day(), dt.Year(), dt.Hour(), dt.Minute(), dt.Second() );
    client.publish("homie/aquarium60/time", tempBuffer);

    sprintf(tempBuffer, "%02d:%02d\n", sunriseHour, sunriseMinute);
    client.publish("homie/aquarium60/sunrise", tempBuffer);
    
    sprintf(tempBuffer, "%02d:%02d\n", sunsetHour, sunsetMinute);
    client.publish("homie/aquarium60/sunset", tempBuffer);
    
    sprintf(tempBuffer, "%02d:%02d\n", moonriseHour, moonriseMinute);
    client.publish("homie/aquarium60/moonrise", tempBuffer);
    
    sprintf(tempBuffer, "%02d:%02d\n", moonsetHour, moonsetMinute);
    client.publish("homie/aquarium60/moonset", tempBuffer);
    
    sprintf(tempBuffer, "%02d\n", duration);
    client.publish("homie/aquarium60/duration", tempBuffer);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char buf[20] = "";
  char sendBuffer[20] = "";


  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    buf[i] = (char)payload[i];
  }
  buf[length] = '\0';
  Serial.println();

  ////// Direct controll of controllable items /////////////////////////////////////////////
  // switch main light on/off over mqtt
  if(strcmp(topic, "homie/aquarium60/light/set") == 0){
    int payloadAsInt = atoi ((char*)payload);
    analogWrite(12, map(payloadAsInt, 0, 100, 0, 1023 ));
    Serial.printf ("Main light at %d percent", payloadAsInt);
    sprintf(sendBuffer, "%d\n", payloadAsInt);
    client.publish("homie/aquarium60/light", sendBuffer);
  } 
  // Switch RGB light on/off over mqtt - to change
  else if(strcmp(topic, "homie/aquarium60/rgb/set") == 0){
    int payloadAsInt = atoi ((char*)payload);
    switch (payloadAsInt) {
      case 0:
        Serial.println("RGB Off");
        clearRGB();
        break;
      case 1:
        Serial.println("RGB set 1");
        setRGB1();
        client.publish("homie/aquarium60/rgb","RGB set 1");
        break;
      case 2:
        Serial.println("RGB set 2 - all blue");
        setRGB2();
        client.publish("homie/aquarium60/rgb","RGB set 2");
        break;
      case 3:
        Serial.println("RGB set 3 - all red");
        setRGB3();
        client.publish("homie/aquarium60/rgb","RGB set 3");
        break;
      case 4:
        Serial.println("RGB set 4 - all green");
        setRGB4();
        client.publish("homie/aquarium60/rgb","RGB set 4");
        break;
      case 5:
        Serial.println("RGB set 5");
        setRGB5();
        client.publish("homie/aquarium60/rgb","RGB set 5");
        break;
      default:
        // statements
        break;
    }
  } 
  // Switch moonlight on/off over mqtt
  else if(strcmp(topic, "homie/aquarium60/moonlight/set") == 0){
    int payloadAsInt = atoi ((char*)payload);
    if (payloadAsInt == 0){
      Serial.println("Moonlight Off");
		  client.publish("homie/aquarium60/log","Moonli off");
      clearRGB();
      client.publish("homie/aquarium60/moonlight","0");
    } 
    else if (payloadAsInt == 1) {
      Serial.println("Moonlight On");
		  client.publish("homie/aquarium60/log","Moon on");
      setMoon1();
      client.publish("homie/aquarium60/moonlight","1");  
    }
  } 
  // Switch co2 on/off over mqtt
  else if (strcmp(topic, "homie/aquarium60/co2/set") == 0){
    int payloadAsInt = atoi ((char*)payload);
    if (payloadAsInt == 1) {
      co2 = true;
      digitalWrite(PIN_RELAY_CO2, HIGH);
      client.publish("homie/aquarium60/co2", "1");
    }
    else {
      co2 = false;
      digitalWrite(PIN_RELAY_CO2, LOW); 
      client.publish("homie/aquarium60/co2", "0");     
    }
  }
  // Switch air on/off over mqtt
  else if (strcmp(topic, "homie/aquarium60/air/set") == 0){
    int payloadAsInt = atoi ((char*)payload);
    if (payloadAsInt == 1) {
      air = true;
      digitalWrite(PIN_RELAY_AIR, HIGH);
      client.publish("homie/aquarium60/air", "1");
    }
    else {
      air = false;
      digitalWrite(PIN_RELAY_AIR, LOW);  
      client.publish("homie/aquarium60/air", "0");    
    }
  }

  ////// Set new timers and other options ////////////////////////////////////////
  // set new time over mqtt
  else if (strcmp(topic, "homie/aquarium60/time/set") == 0) {
    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok((char*)payload,":");
    int hour = atoi(strtokIndx); 

    strtokIndx = strtok(NULL, ":");
    int minute = atoi(strtokIndx);

    RtcDateTime now = Rtc.GetDateTime();
    RtcDateTime newTime = RtcDateTime(now.Year(), now.Month(), now.Day(), hour, minute, 0);
    Rtc.SetDateTime(newTime);

    Serial.printf ("New time: %02d:%02d", hour, minute);
		Serial.println();  
    
    sprintf(sendBuffer, "%02d:%02d\n", hour, minute);
    client.publish("homie/aquarium60/time", sendBuffer);
  }
  // set new sunrise time over mqtt
  else if (strcmp(topic, "homie/aquarium60/sunrise/set") == 0) {
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
    sprintf(sendBuffer, "%02d:%02d\n", sunriseHour, sunriseMinute);
    client.publish("homie/aquarium60/sunrise", sendBuffer);
  }
  // set new sunset time over mqtt
  else if (strcmp(topic, "homie/aquarium60/sunset/set") == 0) {
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
    sprintf(sendBuffer, "%02d:%02d\n", sunsetHour, sunsetMinute);
    client.publish("homie/aquarium60/sunset", sendBuffer); 
  }
  // set new moonrise time over mqtt
  else if (strcmp(topic, "homie/aquarium60/moonrise/set") == 0) {
    char * strtokIndx; // this is used by strtok() as an index

    strtokIndx = strtok((char*)payload,":");
    moonriseHour = atoi(strtokIndx); 

    strtokIndx = strtok(NULL, ":");
    moonriseMinute = atoi(strtokIndx);     

    EEPROM.begin(512);
    EEPROM.write(6, moonriseHour);
		EEPROM.write(7, moonriseMinute);
		EEPROM.commit();
		EEPROM.end();

    Serial.printf ("New moonrise time: %02d:%02d", moonriseHour, moonriseMinute);
		Serial.println();  
    sprintf(sendBuffer, "%02d:%02d\n", moonriseHour, moonriseMinute);
    client.publish("homie/aquarium60/moonrise", sendBuffer);
  }
  // set new moonset time over mqtt
  else if (strcmp(topic, "homie/aquarium60/moonset/set") == 0) {
    char * strtokIndx; 

    strtokIndx = strtok((char*)payload,":");  
    moonsetHour = atoi(strtokIndx); 
    strtokIndx = strtok(NULL, ":");
    moonsetMinute = atoi(strtokIndx);     

    EEPROM.begin(512);
    EEPROM.write(8, moonsetHour);
		EEPROM.write(9, moonsetMinute);
		EEPROM.commit();
		EEPROM.end();

    Serial.printf ("New moonset time: %02d:%02d", moonriseHour, moonriseMinute);
		Serial.println();  
    sprintf(sendBuffer, "%02d:%02d\n", moonsetHour, moonsetMinute);
    client.publish("homie/aquarium60/moonset", sendBuffer);
  }
  // Set new sunrise and sunset duration
  else if (strcmp(topic, "homie/aquarium60/duration/set") == 0) {
    duration = atoi ((char*)payload);

    EEPROM.begin(512);
    EEPROM.write(5, duration);
		EEPROM.commit();
		EEPROM.end();
    Serial.printf ("New duration time: %d", duration);
		Serial.println();  
    sprintf(sendBuffer, "%d\n", duration);
    client.publish("homie/aquarium60/duration", sendBuffer);
  } 

  // reset EEPRom to default values
  else if (strcmp(topic, "homie/aquarium60/reseteeprom") == 0) {
    resetEEPRom();
  }

  // Reqest and send information about this node /////////////////////////////////
  // Request Ip from this controller
  else if(strcmp(topic, "homie/aquarium60/requestip") == 0){
    char tempBuffer[20] = "";
    sprintf(tempBuffer, "IP: %s\n", WiFi.localIP().toString().c_str());
    client.publish("homie/aquarium60/ip", tempBuffer);
  }
  // Request all info from this controller
  else if(strcmp(topic, "homie/aquarium60/requestinfo") == 0){
    mqttSendInfo();
  }
  ////// Run actions through mqtt ////////////////////////////////////////////////
  // Simulate sunrise
  else if(strcmp(topic, "homie/aquarium60/dosunrise") == 0){
    sunrise();
    setRGB1();
  }
    // Simulate sunset
  else if(strcmp(topic, "homie/aquarium60/dosunset") == 0){
    sunset();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

// TEMP ////////////////////////////////////////

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

void printTemperature(DeviceAddress deviceAddress)
{

}


void setup() {
  Serial.begin(115200);
  while (!Serial); // wait for serial port to connect. Needed for native USB on ESP8266
  delay(5000);
  Serial.println("Serial started");
  delay(1000);
  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Starting..... ");
  display.display(); 

  // WIFI
  setup_wifi();

	if (noWifiMode == false) {
		client.setServer(mqtt_server, 1883);
		client.setCallback(callback);

    // OTA 
		// Hostname defaults to esp8266-[ChipID]
		#ifdef HOSTNAME 
		ArduinoOTA.setHostname(HOSTNAME);
		#endif

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
	}	

  //display ip address
  Serial.println(WiFi.localIP());
  display.println(WiFi.localIP());
  display.display();
  delay(2000); // display this info for 10 seconds
  display.clearDisplay();
  display.setCursor(0,10);
  display.display();

  // Led strips //
  pinMode(PIN_WHITE_LEDS, OUTPUT); // White
  pinMode(PIN_RELAY_CO2, OUTPUT);
  pinMode(PIN_RELAY_AIR, OUTPUT);
  pixels.begin(); //RGB

  // Setup RTC //
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  Rtc.Begin();
  display.println("Rtc starting:");
  display.display();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);

  if (!Rtc.IsDateTimeValid()) 
  {
      if (Rtc.LastError() != 0)
      {
        Serial.print("RTC communications error = ");
        Serial.println(Rtc.LastError());
        display.print("RTC communications error = ");
        display.println(Rtc.LastError());
        display.display();
      }
      else
      {   
        Serial.println("RTC lost confidence in the DateTime!");
        Rtc.SetDateTime(compiled);
        display.println("RTC lost confidence in the DateTime!");
        display.display();
      }
      delay(2000);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    display.println("RTC lost confidence in the DateTime!");
    display.display();
    Rtc.SetIsRunning(true);
    delay(2000);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    display.println("RTC lost confidence in the DateTime!");
    display.display();
    delay(2000);
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
    display.println("RTC is newer than compile time. (this is expected)");
    display.display();
    delay(2000);
  }
  else if (now == compiled) 
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  printDateToDisplay(now);
  delay(5000);
  display.clearDisplay();
  display.setCursor(0,10);
  display.display();
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 

	startEEPRom();

  ////////////////////////
  // check if light should be on at startup
	/*
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
*/

  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  // locate devices on the bus
  Serial.print("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) {
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }
  if (!sensors.getAddress(insideThermometer, 0)) {
    Serial.println("Unable to find address for Device 0"); 
  }
  // show the addresses we found on the bus
  Serial.print("Device 0 Address: ");
  printAddress(insideThermometer);
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 10); // 9: 0,5 10: 0,25 11: 0,125 12: 0,0625
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC); 
  Serial.println();

  //// ----- Status : starting ////
  client.publish("homie/aquarium60/status", "starting");

}

void loop() {
  
	if (noWifiMode == false) {
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

    // display stuff

    showCurrentScreen();
  //// Check the time every x seconds and perform an action 
	//// Check sunrise and sunset 
    if (checkTime(now, sunriseHour, sunriseMinute)) {
      daylight = true;
      sunrise();
      setRGB1();
    }
    if (checkTime(now, sunsetHour, sunsetMinute)) {
      daylight = false;
      sunset();
    }
    Serial.print("Daylight: ");
    Serial.println(daylight);
    
    //// check CO2
    if (checkTime(now, sunriseHour - 2, sunriseMinute)) {
      co2 = true;
      digitalWrite(PIN_RELAY_CO2, HIGH);
    }
    if (checkTime(now, sunsetHour - 2, sunsetMinute)) {
      co2 = false;
      digitalWrite(PIN_RELAY_CO2, LOW);
    }

    //// check AIR
    if (checkTime(now, airStartHour, sunriseMinute)) {
      air = true;
      digitalWrite(PIN_RELAY_AIR, HIGH);
    }
    if (checkTime(now, sunsetHour, sunsetMinute)) {
      air = false;
      digitalWrite(PIN_RELAY_AIR, LOW);
    }
    //// Check temperature
    Serial.print("Requesting temperatures...");
    sensors.requestTemperatures(); // Send the command to get temperatures
    printTemperature(insideThermometer); // Use a simple function to print out the data
    waterTemp = sensors.getTempC(insideThermometer);
    if(waterTemp == DEVICE_DISCONNECTED_C) 
    {
      Serial.println("Error: Could not read temperature data");
    }
    else {
      Serial.print("Temp C: ");
      waterTemp = waterTemp + tempOffset;
      Serial.println(waterTemp);

    char tempBuffer[20] = "";
    sprintf(tempBuffer, "%f\n", waterTemp);
    client.publish("homie/aquarium60/watertemperature", tempBuffer);
     //// ----- Status : starting ////
    client.publish("homie/aquarium60/status", "ready");
    }
  }
}