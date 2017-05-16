
/* This sketch connects to IOTAppStory and downloads the assigned firmware. The firmware assignment is done on the server, and is based on the MAC address of the board

    On the server, you need to upload the PHP script "iotappstory.php", and put your .bin files in the .\bin folder

    This sketch is based on the ESPhttpUpdate examples

   To add new constants in WiFiManager search for "NEW CONSTANTS" and insert them according the "boardName" example

  Copyright (c) [2016] [Andreas Spiess]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#define SKETCH "IOTappStoryLoader "
#define VERSION "V1.3"
#define FIRMWARE SKETCH VERSION

//#define SERIALDEBUG       // Serial is used to present debugging messages 
//#define REMOTEDEBUGGING   // http://sockettest.sourceforge.net/ 


#define LEDS_INVERSE   // LEDS on = GND

// #include <credentials.h>
#include <ESP8266WiFi.h>            // add this to arduino perferages additional libraries http://arduino.esp8266.com/stable/package_esp8266com_index.json and install from Sketch Libs lib mgmt.. esp8266
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include "WiFiManagerMod.h"        // used the modded version from the repo " means look in same folder as sketch   https://github.com/kentaylor/WiFiManager
#include <Ticker.h>
#include <EEPROM.h>
#include "FS.h"
#include <ArduinoJson.h>  //install by Arduino library manager

extern "C" {
#include "user_interface.h" // this is for the RTC memory read/write functions
}

// -------- PIN DEFINITIONS ------------------
#ifdef ARDUINO_ESP8266_ESP01           // Generic ESP's 
#define MODEBUTTON 0
#define LEDgreen 13
//#define LEDred 12
#else
#define MODEBUTTON D3
#define LEDgreen D7
//#define LEDred D6
#endif

//---------- DEFINITIONS for SKETCH ----------
#define STRUCT_CHAR_ARRAY_SIZE 50  // length of config variables
#define MAX_WIFI_RETRIES 50
#define RTCMEMBEGIN 68
#define MAGICBYTE 85

// --- Optional -----
#define SERVICENAME "LOADER"  // name of the MDNS service used in this group of ESPs


//-------- SERVICES --------------



//--------- ENUMS AND STRUCTURES  -------------------
/*
 * This is the configuration shown in the WiFi Manager
 * I know python so I use pythonish terms
 * Here a dictionary data type / object / thingy is defined, it is a the "struct"ure
 * and named strConfig 
 * 
 * Here you can add more item / variables for your sketch like the blinkLEDPin
 */
String automaticUpdate, blinkLEDPin; // You need to define the dict key type 

typedef struct {
  char ssid[STRUCT_CHAR_ARRAY_SIZE];
  char password[STRUCT_CHAR_ARRAY_SIZE];
  char boardName[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory2[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP2[STRUCT_CHAR_ARRAY_SIZE];
  // insert NEW CONSTANTS according boardname example HERE!
  char automaticUpdate[STRUCT_CHAR_ARRAY_SIZE];
  char blinkLEDPin[STRUCT_CHAR_ARRAY_SIZE];
  // end
  char magicBytes[4];
} strConfig;

/*
 * So lets create the config dict and fill with values, order see above.
 */
strConfig config = {
  "",
  "",
  "INITloader",
  "iotappstory.com",
  "/ota/esp8266-v1.php",
  "iotappstory.com",
  "/ota/esp8266-v1.php",
  "test",
  "2",
  "CFG"  // Magic Bytes
};



//---------- VARIABLES ----------



//---------- FUNCTIONS ----------
// to help the compiler, sometimes, functions have  to be declared here
/* 
 * This is caused by the order of functions in the code. 
 * 
 * e.g. this will give errors, so put void awesomefunc(){... or a dummy like void awesomefunc();
 * before the (main)loop
 * 
 * void loop(){
 *  awesomefunc();
 * }
 * 
 * void awesomefunc(){
 *   println("Hello World");
 * }
 */
void initialize(void);
void connectNetwork(void);
void loopWiFiManager(void);
void eraseFlash(void);


//---------- OTHER .H FILES ----------
#include "ESP_Helpers.h"           // General helpers for all IOTappStory sketches. Is also placed inside this directory for ease of use, but should be the same as the library file
#include "IOTappStoryHelpers.h"    // Sketch specific helpers for all IOTappStory sketches


//-------------------------- SETUP -----------------------------------------
void setup() {
  #ifdef SERIALDEBUG
  Serial.begin(115200);
  #endif
  for (int i = 0; i < 5; i++) DEBUG_PRINTLN("");
  DEBUG_PRINTLN("Start "FIRMWARE);

  // ----------- PINS ----------------
  pinMode(MODEBUTTON, INPUT_PULLUP);  // MODEBUTTON as input for Config mode selection

#ifdef LEDgreen
  pinMode(LEDgreen, OUTPUT);
  digitalWrite(LEDgreen, LEDOFF);
#endif
#ifdef LEDred
  pinMode(LEDred, OUTPUT);
  digitalWrite(LEDred, LEDOFF);
#endif


  // ------------- INTERRUPTS ----------------------------
  blink.detach();

  //------------- LED and DISPLAYS ------------------------
  LEDswitch(GreenBlink);

  for (int ii = 0; ii < 3; ii++) {
    for (int i = 0; i < 2; i++) Serial.println("");
    Serial.println("!!!!!!!!!!!!!!!  Please press reset button. ONLY FIRST TIME AFTER SERIAL LOAD!   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    for (int i = 0; i < 2; i++) Serial.println("");
    delay(5000);
  }
  getMACaddress();
  printMacAddress();
  
  initSpiffs();

  eraseFlash();
  writeConfig();  // configuration in EEPROM
  initWiFiManager();
  Serial.println("setup done");
}

// ----------------------- LOOP --------------------------------------------

void loop() {
  yield();
  loopWiFiManager();
  //-------- Standard Block ---------------
}

//------------------------ END LOOP ------------------------------------------------


void  initSpiffs() {

  SPIFFS.begin();
  SPIFFS.format();
  FSInfo fs_info;
  SPIFFS.info(fs_info);
  Serial.print("initializing SPIFFS size:");
  Serial.println(fs_info.totalBytes);
  Serial.println();
  Serial.println();
  SPIFFS.end();
}

