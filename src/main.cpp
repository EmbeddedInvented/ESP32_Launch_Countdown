/**
   BasicHTTPSClient.ino

    Created on: 14.10.2018

*/

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <LedController.hpp>


void setClock();
String spaceDevHttpGet(String URL);
int getLocationId(String rawData);
String getNextURL(String rawData);
int getLaunchTime(String rawData);
int printTime7Seg(tmElements_t countdown);
unsigned long findAndGetNextLaunchTime();

const size_t capacity = 5000;

const String baseURL = "https://ll.thespacedevs.com/2.0.0/launch/upcoming/?limit=1&offset=0&search=USA";

WiFiMulti multiWiFi;


LedController lc =LedController(GPIO_NUM_23,GPIO_NUM_18,GPIO_NUM_5,1);

const int Cape_Id = 12;
const int Kennedy_Id = 27;

unsigned long nextLaunchTime_Epoch;

tmElements_t nextLaunchTime;  // time elements structure
tmElements_t countdownTime;  // time elements structure
time_t nextLaunch_timestamp; // a timestamp


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);



void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  WiFi.mode(WIFI_STA);
  multiWiFi.addAP("Panic! At the Cisco", "8015463911");

  // wait for WiFi connection
  Serial.print("Waiting for WiFi to connect...");
  while ((multiWiFi.run() != WL_CONNECTED)) {
    Serial.print(".");
  }
  Serial.println(" connected");

  lc.activateAllSegments();
  /* Set the brightness to a medium values */
  lc.setIntensity(8);
  /* and clear the display */
  lc.clearMatrix();
}

void loop() {
 nextLaunchTime_Epoch = findAndGetNextLaunchTime();
  while(1)
  {
    timeClient.update();
    breakTime((nextLaunchTime_Epoch - timeClient.getEpochTime()),countdownTime);

    countdownTime.Day = countdownTime.Day - 1;

    printTime7Seg(countdownTime);

    Serial.print(countdownTime.Day);
    Serial.print(":");
    Serial.print(countdownTime.Hour);
    Serial.print(":");
    Serial.print(countdownTime.Minute);
    Serial.print(":");
    Serial.println(countdownTime.Second);

    delay(1000);  
  }
}


//function to HTTP GET the data from the spacedev API
String spaceDevHttpGet(String URL)
{
    WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    //client -> setCACert(rootCACertificate); //do not add a certificate so we can connect insecurely indefinitely

    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, URL)) {  // HTTPS
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        int httpCode = https.GET();
  
        // httpCode will be negative on error
        if (httpCode > 0) {
          // HTTP header has been send and Server response header has been handled
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
  
          // file found at server
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            String payload = https.getString();
            return payload;
          }
        } else {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
      }

      // End extra scoping block
    }
  
    delete client;
  } else {
    Serial.println("Unable to create client");
  }
}

int getLocationId(String rawData)
{
  DynamicJsonDocument doc(capacity);

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, rawData);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
  }
  
  Serial.println(doc["results"][0]["pad"]["location"]["id"].as<String>());
  return doc["results"][0]["pad"]["location"]["id"].as<int>();
}

String getNextURL(String rawData)
{
  DynamicJsonDocument doc(capacity);

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, rawData);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
  }
  
  Serial.println(doc["next"].as<String>());
  return doc["next"].as<String>();
}

//returns launch time in UTC
int getLaunchTime(String rawData)
{
  DynamicJsonDocument doc(capacity);

  // Parse JSON object
  DeserializationError error = deserializeJson(doc, rawData);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
  }

  String launchTimeString = doc["results"][0]["net"].as<String>();
  
  String sYear = launchTimeString.substring(0,4);
  Serial.println(sYear);
  String sMonth = launchTimeString.substring(5,7);
  Serial.println(sMonth);
  String sDay = launchTimeString.substring(8,10);
  Serial.println(sDay);
  String sHour = launchTimeString.substring(11,13);
  Serial.println(sHour);
  String sMinute = launchTimeString.substring(14,16);
  Serial.println(sMinute);
  String sSecond = launchTimeString.substring(17,19);
  Serial.println(sSecond);

  // convert a date and time into unix time, offset 1970
  nextLaunchTime.Second = sSecond.toInt();
  nextLaunchTime.Hour = sHour.toInt();
  nextLaunchTime.Minute = sMinute.toInt();
  nextLaunchTime.Day = sDay.toInt();
  nextLaunchTime.Month = sMonth.toInt();      // months start from 0, so deduct 1
  nextLaunchTime.Year = sYear.toInt() - 1970; // years since 1970, so deduct 1970

  nextLaunch_timestamp =  makeTime(nextLaunchTime);
  Serial.println(nextLaunch_timestamp);
  
  return nextLaunch_timestamp;
}

int printTime7Seg(tmElements_t countdown)
{
  lc.setDigit(0,7,(int)countdown.Day/10,false);


  lc.setDigit(0,6,(int)countdown.Day%10,true);


  lc.setDigit(0,5,(int)countdown.Hour/10,false);


  lc.setDigit(0,4,(int)countdown.Hour%10,true);


  lc.setDigit(0,3,(int)countdown.Minute/10,false);


  lc.setDigit(0,2,(int)countdown.Minute%10,true);


  lc.setDigit(0,1,(int)countdown.Second/10,false);


  lc.setDigit(0,0,(int)countdown.Second%10,false);

}

unsigned long findAndGetNextLaunchTime()
{
        
  String FullResponse = spaceDevHttpGet(baseURL);
  
  int locationId = getLocationId(FullResponse);
  Serial.println(locationId);

  while( !((locationId == Cape_Id) || (locationId == Kennedy_Id)) )
  {
    delay(1000);
    Serial.println(locationId);
    Serial.println(getNextURL(FullResponse));
    FullResponse = spaceDevHttpGet(getNextURL(FullResponse));
    locationId = getLocationId(FullResponse);
    Serial.println(FullResponse);
  }
  return getLaunchTime(FullResponse);

}

