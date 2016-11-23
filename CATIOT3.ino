/*
   Synchronously retrieves calendar events from Google Calendar using the WiFiClientSecureRedirect library
   Update the values of ssid, passwd and dstPath before use.  REFER TO DOCUMENTATION (see below for URL)

   Platform: ESP8266 using Arduino IDE
   Documentation: http://www.coertvonk.com/technology/embedded/esp8266-clock-import-events-from-google-calendar-15809
   Tested with: Arduino IDE 1.6.11, board package esp8266 2.3.0, Adafruit huzzah feather esp8266

   MIT license, check the file LICENSE for more information
   (c) Copyright 2016, Coert Vonk
   All text above must be included in any redistribution
*/

//#define ARDUINO_OTA
#define DEBUG
//#define USE_MILLIS
#define UPDATE_INTERVAL 5000
#define ENABLE_WATCHDOG
#define WATCHDOG_TIMEOUT 30
#define CLIENT_TIMEOUT 5000
#define NEUTRAL_SERVO_POS 90

// from LarryD, Arduino forum
#ifdef DEBUG    //Macros are usually in all capital letters.
#define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
#define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
#define DPRINT(...)     //now defines a blank line
#define DPRINTLN(...)   //now defines a blank line
#endif

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#ifdef ARDUINO_OTA
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif

#include "WiFiClientSecureRedirect.h"
#include <ArduinoJson.h>
#include <Servo.h>


#ifdef ENABLE_WATCHDOG
#include <Ticker.h>

Ticker secondTick;
volatile int watchdogCount = 0;

void ISRWatchdog() {
  watchdogCount++;
  if (watchdogCount >= WATCHDOG_TIMEOUT)
  {
    ESP.reset();
  }
}

#endif

Servo feederServo; // create servo object to control a servo

// replace with your network credentials
char const * const ssid = "RUMAH"; // ** UPDATE ME **
char const * const passwd = "123456789"; // ** UPDATE ME **

// fetch events from Google Calendar
char const * const dstHost = "script.google.com";
const char * googleRedirHost = "script.googleusercontent.com";
int const dstPort = 443;

char const * const dstPath = "/macros/s/AKfycbwMYHPMUuBhlNEHBfeBc8O2qJKMyMIhg384xF5CPTuqRMIJXBhC/exec"; // ** UPDATE ME **

// On a Linux system with OpenSSL installed, get the SHA1 fingerprint for the destination and redirect hosts:
//   echo -n | openssl s_client -connect script.google.com:443 2>/dev/null | openssl x509  -noout -fingerprint | cut -f2 -d'='
//   echo -n | openssl s_client -connect script.googleusercontent.com:443 2>/dev/null | openssl x509  -noout -fingerprint | cut -f2 -d'='
char const * const dstFingerprint = "EF:3D:5B:04:F5:1D:97:A3:CD:ED:29:35:84:AF:62:21:D5:D0:C2:62";
char const * const redirFingerprint = "42:06:30:DE:E4:62:45:98:7D:3D:D1:AD:FC:D7:40:4A:F9:D6:C3:0F";

String event_prev_id = "";

ESP8266WebServer server(80);

#ifdef USE_MILLIS
unsigned long previousMillis = 0;
const long interval = UPDATE_INTERVAL;
#endif

#ifdef ARDUINO_OTA
bool enable_ota = false;
#endif

bool firstRun = true;

void setup() {

  Serial.begin(115200);
  delay(500);
  DPRINT("\n\nWifi ");
  WiFi.begin(ssid, passwd);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  feederServo.attach(2); // attaches the servo on GIO2 to the servo object

#ifdef ENABLE_WATCHDOG
  secondTick.attach(1, ISRWatchdog);
#endif

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  DPRINTLN(" done");

  server.on("/", handleManual);
  server.begin();

#ifdef ARDUINO_OTA
  ArduinoOTA.onStart([]() {
    enable_ota = true;
    Serial.println("Start");
  });
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.setPassword((const char *)"123");
  ArduinoOTA.begin();
#endif

}

void handleManual() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "feed")
    {
      feedCat(server.arg(i).toInt());
      DPRINTLN("SEND FEED LOG");
      feederServo.write(NEUTRAL_SERVO_POS); // tell servo to go to position in variable 'pos'      
      sendLog(server.arg(i).toInt());
    }
  }
  server.send(200, "text/plain", "use ?feed= to manual feed");
}

void feedCat(int param) {
  int pos;
  for (int j = 0; j < param; j++) {
    for (pos = 0; pos <= 180; pos += 20) // goes from 0 degrees to 180 degrees
    { // in steps of 1 degree
      feederServo.write(pos); // tell servo to go to position in variable 'pos'
      delay(30); // waits 15ms for the servo to reach the position
    }
    for (pos = 180; pos >= 0; pos -= 20) // goes from 180 degrees to 0 degrees
    {
      feederServo.write(pos); // tell servo to go to position in variable 'pos'
      delay(30); // waits 15ms for the servo to reach the position
    }
  }
}


void sendLog(int qty) {
  
  WiFiClientSecureRedirect client;

  String query_path(dstPath);
  query_path +=  "?qty=" + String(qty);
  
  if (!client.connected()) client.connect(dstHost, dstPort);
  
  if (client.connected())
  {
    client.request(query_path.c_str(), dstHost, CLIENT_TIMEOUT, dstFingerprint, redirFingerprint);
  }
  
}


String getSchedule() {
  
  WiFiClientSecureRedirect client;
  if (!client.connected()) client.connect(dstHost, dstPort);

  if (client.connected())
  {
    DPRINTLN("Send request..");
    client.request(dstPath, dstHost, CLIENT_TIMEOUT, dstFingerprint, redirFingerprint);
    String resp = client.getRedir();
    DPRINTLN(resp);
    return resp;
  }else{
    return "";
  }
  
}

void parseJsonCommand(String json_txt)
{

  StaticJsonBuffer <400> jsonBuffer;
  JsonObject & root = jsonBuffer.parseObject(json_txt);

  if (!root.success()) {

    DPRINTLN("parseObject() failed");

  } else {

    const char * event_id = root["id"];
    const char * event_title = root["title"];

    int param = atoi(root["param"]);
    if (param == 0) param = 5;

    if (strcmp(event_prev_id.c_str(), event_id) == 0) {

      DPRINTLN("same event");

    } else {

      event_prev_id = String(event_id);

      DPRINTLN("updated event");
      DPRINTLN(event_id);
      DPRINTLN(event_title);

      if (strstr(event_title, "makan") != NULL) {
        DPRINTLN("FEED CAT");
        feedCat(param);
        DPRINTLN("SEND FEED LOG");
        feederServo.write(NEUTRAL_SERVO_POS); // tell servo to go to position in variable 'pos'
        sendLog(param);
      }

    }
  }
}



void loop() {

  server.handleClient();

#ifdef ENABLE_WATCHDOG
  watchdogCount = 0;
#endif

#ifdef ARDUINO_OTA
  ArduinoOTA.handle();
#endif

//send log if first start
if (firstRun == true) {
  DPRINTLN("First run..");  
  firstRun = false;
  sendLog(0);
}

#ifdef USE_MILLIS
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
#endif
    DPRINT("Free heap .. ");
    DPRINTLN(ESP.getFreeHeap());
    DPRINTLN("Connecting..");
    String resp = getSchedule();
    parseJsonCommand(resp);
#ifdef USE_MILLIS
  }
#endif

#ifndef USE_MILLIS
  delay(UPDATE_INTERVAL);
#endif
}
