// Pre-installed libraries
#include <Arduino.h>
#include <Ticker.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "time.h"
#include <Wire.h>
// External Libraries: (NEED TO INSTALL)
#include <FastLED.h>                  // For Neopixels
#include <iarduino_APDS9930.h>        // For APDS9930 ambient light sensor

// Configs related to LED Strip
#define NUM_DIGITS            4       // Number of Digits
#define NUM_LEDS_PER_DIGIT    14      // Number of LEDs per digit
#define NUM_SEGMENTS          7       // Number of segments (7 for 7 segment display)
#define NUM_LEDS_PER_SEGMENT  2       // Number of LEDs in each segment
#define SERIAL_DEBUG          true    // true - serial 
#define STATUS_LED            false   // true - enable status led
#define ENABLE_OTA_UPDATE     true    
#define FORMAT_12HR           true
// #define FIXED_BRIGHTNESS      255     // Comment for auto brightness adjustment
#define SHOW_FIRST_ZERO       false

CRGB leds[NUM_DIGITS][NUM_LEDS_PER_DIGIT];      // Declaring FastLED leds for the digits, as an array of led arrays
CRGB statusLED[1];                              // Status LED (neopixel)

// LED Segment map (describes what segments need to be lit up for a specific digit)
uint8_t digits[][7] = {
  {1,1,1,0,1,1,1},    // 0
  {0,0,1,0,0,0,1},    // 1
  {0,1,1,1,1,1,0},    // 2
  {0,1,1,1,0,1,1},    // 3
  {1,0,1,1,0,0,1},    // 4
  {1,1,0,1,0,1,1},    // 5
  {1,1,0,1,1,1,1},    // 6
  {0,1,1,0,0,0,1},    // 7
  {1,1,1,1,1,1,1},    // 8
  {1,1,1,1,0,1,1}     // 9
};

// Wifi credentials 
const char* ssid     = "-----------------";
const char* password = "-----------------";

// NTP configs
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;      // Set for your respective location
const int   daylightOffset_sec = 0;     // Use if daylight offset is a thing in ur place

// Ticker objects. Tickers are used for running tasks at specific intervals without delay statements
Ticker statusLEDTicker;
Ticker timeTicker;

//Variables for storing time: 
static int hour = 0;
static int minute = 0;

//FastLED LED Variables: 
static CRGB statusLEDColor        = CRGB::Red;
int refreshInterval               = 25;       // milliseconds - interval of LED refreshes

// Position of LEDs with respect to their logical indices: 
// [2, 1, 0, 0, 1, 2, 3, 3, 4, 5, 6, 6, 5, 4]
uint8_t ledSequenceMap[14]     = {2, 3, 4, 5, 13, 12, 11, 10, 9, 8, 0, 1, 7, 6}; 
uint8_t hueArrayRainbow[14]    = {20, 10, 0, 0, 10, 20, 30, 30, 40, 50, 60, 60, 50, 40};
uint8_t hueArray[14]    = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint8_t hueArrayIncrement      = 1;

// Ambient Light Sensor Variables: 
float luxFactor           = 50;       // Increase/decrease factor for brightness change (higher - slower reaction to light change)
float luxValue            = 0;        
float minLuxValue         = 0;        // Minimum Lux value (need to experiment)
float maxLuxValue         = 25;      // Maximum Lux value (need to experiment)
uint8_t ledLightValue     = 0;        // Light value used for brightness by FastLEDs
uint8_t minBrightness     = 60;       // Min brightness of the LEDs
uint8_t maxBrightness     = 220;      // Max brightness of the LEDs

int buttonPin             = 6;
// bool enableOTA            = false;
// int OTAStartDelay         = 2000;

iarduino_APDS9930 apds; 

//-------------------------------------------Necessary Functions (Setup and Loop)-------------------------------------------------//

void setup() {
  Serial.begin(115200);
  Serial.println("ARGB Clock V2");
  pinMode(buttonPin, INPUT);

  // Change led type based on what u are using
  // Digit 1
  FastLED.addLeds<NEOPIXEL, 3>(leds[0], NUM_LEDS_PER_DIGIT);
  // Digit 2
  FastLED.addLeds<NEOPIXEL, 4>(leds[1], NUM_LEDS_PER_DIGIT);
  // Digit 3
  FastLED.addLeds<NEOPIXEL, 5>(leds[2], NUM_LEDS_PER_DIGIT);
  // Digit 4
  FastLED.addLeds<NEOPIXEL, 6>(leds[3], NUM_LEDS_PER_DIGIT);
  //Status LED
  FastLED.addLeds<WS2811, 21, RGB>(statusLED, 1);

  // Show the loading state
  showHorzBar(CHSV(35, 255, 128), CHSV(230, 255, 128));

  // Set status LED to startup mode (orange - constant)
  #if STATUS_LED
    statusLEDColor = CRGB::Purple;
    statusLED[0] = statusLEDColor;
    FastLED.show();
  #endif

  //Light sensor init
  while(!apds.begin(&Wire)) {
    Serial.println("Light sensor init fail!");
    delay(100);
  }
  Serial.println("Light sensor active");
  getAmbientLight();

  // WiFi config: 

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected.");
  delay(500);

  // OTA Config: 
  #if ENABLE_OTA_UPDATE
    ArduinoOTA.setHostname("ARGB_CLOCK");
    ArduinoOTA.setPassword("clock");

    ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
    ArduinoOTA.begin();
    Serial.print("OTA Available on IP: ");
    Serial.println(WiFi.localIP());
  #endif

  // Init NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(500);

  //get time from ntp until success, then storing in hour and minute variables
  struct tm timeinfo;

  while(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time. Trying again");
    delay(50);
  }

  Serial.println("Time obtain success.");
  getTimeFromNTP();
  // char timeHour[3];
  // char timeMinute[3];
  // strftime(timeHour,3, "%H", &timeinfo);
  // // strftime(timeHour,3, "%I", &timeinfo);    // Replace prev line with this to change to 12-hour format
  // strftime(timeMinute,3, "%M", &timeinfo);
  // sscanf(timeHour, "%d", &hour);
  // sscanf(timeMinute, "%d", &minute);

  //disconnect WiFi as it's no longer needed
  #if !ENABLE_OTA_UPDATE
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("Wifi Disconnected.");
  #endif
  delay(100);

  // Init Ticker for Time update (every 1000ms)
  timeTicker.attach_ms(1000, getTimeFromNTP);

}

void loop() {
  #if ENABLE_OTA_UPDATE
    ArduinoOTA.handle();
  #endif
  // Set global brigtness 
  #ifndef FIXED_BRIGHTNESS
    FastLED.setBrightness(ledLightValue);
    getAmbientLight();
  #else
    FastLED.setBrightness(FIXED_BRIGHTNESS);
  #endif
  // Display the time

  // displayTimeRainbow();
  // displayTimeRandom();
  displayTimeTwinklingRainbow();
  
  // Increment the Hue array for Rainbow function
  incrementHueArray();
  FastLED.show();
  // Update Ambient Light sensor value
  
  FastLED.delay(refreshInterval);
  
}

//----------------------------------------------Custom Defined Functions-------------------------------------------// 


// Show a Horizontal bar of given color: 
void showHorzBar(CHSV color1, CHSV color2) {
  FastLED.clear(true);
  for(int dig = 0; dig < NUM_DIGITS; dig++) {
    leds[dig][6] = color2;
    leds[dig][7] = color1;
  }
  FastLED.show();
}

// Loading loop during startup
void ledStartupLooping() {
  static CHSV color = CHSV(135, 255, 255);
  static int index = 0;
  for(int dig = 0; dig< NUM_DIGITS; dig++) {
    leds[dig][ledSequenceMap[index]] = CHSV(0,0,0); //black;
  }
  index = index == 11 ? 0 : index+1; 
  for(int dig = 0; dig< NUM_DIGITS; dig++) {
    leds[dig][ledSequenceMap[index]] = color; //black;
  }
  FastLED.show();
}

// Used to blink status LED with the help of a statusLEDTicker.
void statusLEDBlink() {
  static int state = 1;
  CHSV color = state ? CHSV(0, 0, ledLightValue) : CHSV(0, 0, 0);        // dim white or black
  statusLED[0] = color;
  // FastLED.show();
  state = state ? 0 : 1;
} 

// Display the time digits in a vertical rainbow
void displayTimeRainbow() {
  FastLED.clear();
  if(hour/10 == 0) {
    #if SHOW_FIRST_ZERO
      displayDigitRainbow(0, 0);
    #endif
  }
  else {
    displayDigitRainbow(0, hour/10);
  }
  displayDigitRainbow(1, hour%10);
  displayDigitRainbow(2, minute/10);
  displayDigitRainbow(3, minute%10);
}

void displayTimeRandom() {
  FastLED.clear();
  if(hour/10 == 0) {
    #if SHOW_FIRST_ZERO
      displayDigitRandom(0, 0);
    #endif
  }
  else {
    displayDigitRandom(0, hour/10);
  }
  displayDigitRandom(1, hour%10);
  displayDigitRandom(2, minute/10);
  displayDigitRandom(3, minute%10);
}

void displayTimeTwinklingRainbow() {
  FastLED.clear();
  if(hour/10 == 0) {
    #if SHOW_FIRST_ZERO
      displayDigitTwinklingRainbow(0, 0);
    #endif
  }
  else {
    displayDigitTwinklingRainbow(0, hour/10);
  }
  displayDigitTwinklingRainbow(1, hour%10);
  displayDigitTwinklingRainbow(2, minute/10);
  displayDigitTwinklingRainbow(3, minute%10);
}

// Display digit as form of rainbow
void displayDigitRainbow(int pos, int digit) {
  
  for(int seg = 0; seg < NUM_SEGMENTS; seg++) {
    if(digits[digit][seg]) {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        // leds[pos][pixelIdx] = CHSV(hueArrayRainbow[pixelIdx], 255, ledLightValue);
        leds[pos][pixelIdx] = CHSV(hueArrayRainbow[pixelIdx], 255, 255);        // To be used with global brightness
      }
    }
    else {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = CRGB::Black;
      }
    }
  }
}

// Increment the hue array for Rainbow effect.
// Since the Data Type is 8bit uint, the values automatically reset to 0 when they cross 255
void incrementHueArray() {
  for(int i=0; i<14; i++) {
    hueArrayRainbow[i] += hueArrayIncrement;
  }
}

// Get and store Ambient Light from the sensor
void getAmbientLight() {
  luxValue *= luxFactor-1;
  luxValue += apds.getLight();
  luxValue /= luxFactor;
  // Serial.print(luxValue);
  // Serial.print("-----------");
  luxValue = luxValue > maxLuxValue ? maxLuxValue : luxValue;
  ledLightValue = (uint8_t)map(luxValue, minLuxValue, maxLuxValue, minBrightness, maxBrightness);
  // Serial.println(ledLightValue);
}

// Get and store the current time
void getTimeFromNTP() {
  
  #if STATUS_LED
    statusLEDBlink();
  #endif

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  char timeHour[3];
  #if FORMAT_12HR
    strftime(timeHour,3, "%I", &timeinfo);
  #else 
    strftime(timeHour,3, "%H", &timeinfo);
  #endif
  // Serial.println(timeHour);
  sscanf(timeHour, "%d", &hour);
  char timeMinute[3];
  strftime(timeMinute,3, "%M", &timeinfo);
  // Serial.println(timeMinute);
  sscanf(timeMinute, "%d", &minute);
  // Blinking of the status LED once every second
  #if STATUS_LED
    statusLEDTicker.once_ms(500, statusLEDBlink);
  #endif
}

// DisplayDigit function that uses externally taken color
void displayDigitSolid(int pos, int digit, CHSV color) {

  for(int seg = 0; seg < NUM_SEGMENTS; seg++) {
    if(digits[digit][seg]) {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = color;
      }
    }
    else {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = CRGB::Black;
      }
    }
  }
  // FastLED.show();
}

void displayDigitRandom(int pos, int digit) {
  for(int seg = 0; seg < NUM_SEGMENTS; seg++) {
    if(digits[digit][seg]) {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = CHSV(random8(), 255, 255);
      }
    }
    else {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = CRGB::Black;
      }
    }
  }
}

void displayDigitTwinklingRainbow(int pos, int digit) {
  int decayFactor = 1;
  int baseValue = 150;
  if(random8() > 64) {
    hueArray[random8(14)] = 255;
  }
  for(int i = 0; i<14; i++) {
    if(hueArray[i] > baseValue) {
      hueArray[i] -= decayFactor;
      if(hueArray[i] < baseValue) hueArray[i] == baseValue;
    }
  }
  for(int seg = 0; seg < NUM_SEGMENTS; seg++) {
    if(digits[digit][seg]) {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = CHSV(hueArrayRainbow[pixelIdx], 255, hueArray[pixelIdx]);
      }
    }
    else {
      for(int bit = 0; bit<NUM_LEDS_PER_SEGMENT; bit++) {
        int pixelIdx = seg * NUM_LEDS_PER_SEGMENT + bit;
        leds[pos][pixelIdx] = CRGB::Black;
      }
    }
  }
}
