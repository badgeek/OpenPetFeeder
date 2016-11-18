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

#include <ESP8266WiFi.h> 

#include "WiFiClientSecureRedirect.h" 
#include <ArduinoJson.h> 
#include <Servo.h> 

Servo myservo; // create servo object to control a servo
// twelve servo objects can be created on most boards

// replace with your network credentials
char const * const ssid = "RUMAH"; // ** UPDATE ME **
char const * const passwd = "123456789"; // ** UPDATE ME **

// fetch events from Google Calendar
char const * const dstHost = "script.google.com";
const char * googleRedirHost = "script.googleusercontent.com";
int const dstPort = 443;
int32_t const timeout = 5000;

char const * const dstPath = "/macros/s/AKfycbwMYHPMUuBhlNEHBfeBc8O2qJKMyMIhg384xF5CPTuqRMIJXBhC/exec"; // ** UPDATE ME **

// On a Linux system with OpenSSL installed, get the SHA1 fingerprint for the destination and redirect hosts:
//   echo -n | openssl s_client -connect script.google.com:443 2>/dev/null | openssl x509  -noout -fingerprint | cut -f2 -d'='
//   echo -n | openssl s_client -connect script.googleusercontent.com:443 2>/dev/null | openssl x509  -noout -fingerprint | cut -f2 -d'='
char const * const dstFingerprint = "EF:3D:5B:04:F5:1D:97:A3:CD:ED:29:35:84:AF:62:21:D5:D0:C2:62";
char const * const redirFingerprint = "42:06:30:DE:E4:62:45:98:7D:3D:D1:AD:FC:D7:40:4A:F9:D6:C3:0F";

String event_prev_id = "";

// from LarryD, Arduino forum
#define DEBUG   //If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.
#ifdef DEBUG    //Macros are usually in all capital letters.
  #define DPRINT(...)    Serial.print(__VA_ARGS__)     //DPRINT is a macro, debug print
  #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   //DPRINTLN is a macro, debug print with new line
#else
  #define DPRINT(...)     //now defines a blank line
  #define DPRINTLN(...)   //now defines a blank line
#endif

void setup() {
  myservo.attach(2); // attaches the servo on GIO2 to the servo object

  Serial.begin(115200);
  delay(500);
  DPRINT("\n\nWifi ");
  WiFi.begin(ssid, passwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DPRINT(".");
  }
  DPRINTLN(" done");

}

void feedCat(int param) {
  int pos;
  for (int j = 0; j < param; j++) {
      for (pos = 0; pos <= 180; pos += 20) // goes from 0 degrees to 180 degrees
      { // in steps of 1 degree
        myservo.write(pos); // tell servo to go to position in variable 'pos'
        delay(30); // waits 15ms for the servo to reach the position
      }
      for (pos = 180; pos >= 0; pos -= 20) // goes from 180 degrees to 0 degrees
      {
        myservo.write(pos); // tell servo to go to position in variable 'pos'
        delay(30); // waits 15ms for the servo to reach the position
      }
  }
}


void parseJsonCommand(String json_txt)
{
  
 StaticJsonBuffer <400> jsonBuffer;   
 JsonObject & root = jsonBuffer.parseObject(json_txt);
 
 if (!root.success()) {
 
   Serial.println("parseObject() failed");
 
 } else {
   
   const char * event_id = root["id"];
   const char * event_title = root["title"];
   
   int param = atoi(root["param"]);
   if (param == 0) param = 5;
   
   if (strcmp(event_prev_id.c_str(), event_id) == 0) {
   
     Serial.println("same event");
   
   } else {
     
     event_prev_id = String(event_id);
     
     Serial.println("updated event");
     Serial.println(event_id);
     Serial.println(event_title);
     
     if (strstr(event_title, "makan") != NULL) {
      
       Serial.println("MAKAN");
       feedCat(param);
       myservo.write(90); // tell servo to go to position in variable 'pos'
       
     }
     
   }
 }
}

void loop() {

  DPRINT("Free heap .. ");
  DPRINTLN(ESP.getFreeHeap());
  WiFiClientSecureRedirect client;

  if (!client.connected()) client.connect(dstHost, dstPort);  
  client.request(dstPath, dstHost, 2000, dstFingerprint, redirFingerprint);

  String resp = client.getRedir();
  Serial.println(resp);
  
  parseJsonCommand(resp);
 
  delay(1500);

}
