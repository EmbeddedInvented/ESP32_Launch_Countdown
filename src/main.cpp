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

//***************************WIFI MANAGER CONFIGURATION****************************
#define _WIFIMGR_LOGLEVEL_    3

#include <esp_wifi.h>

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_ON      HIGH
#define LED_OFF     LOW

String Router_SSID = "Panic! At the Cisco";
String Router_Pass = "8015463911";

#define USE_AVAILABLE_PAGES     false

#define USE_STATIC_IP_CONFIG_IN_CP          false

#define USE_ESP_WIFIMANAGER_NTP     false

#define USE_CLOUDFLARE_NTP          false

#define USE_DHCP_IP     true

#define USE_CONFIGURABLE_DNS      false

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

//**********************END WIFI MANAGER CONFIGURATION **********************************




void setClock();
String spaceDevHttpGet(String URL);
int getLocationId(String rawData);
String getNextURL(String rawData);
int getLaunchTime(String rawData);
int printTime7Seg(tmElements_t countdown);
unsigned long findAndGetNextLaunchTime();
void setupWiFi();
void printZero7Seg();

const size_t capacity = 5000;

const String baseURL = "https://ll.thespacedevs.com/2.0.0/launch/upcoming/?limit=1&offset=0&search=USA";

WiFiMulti multiWiFi;


LedController lc =LedController(GPIO_NUM_23,GPIO_NUM_18,GPIO_NUM_5,1);

const int Cape_Id = 12;
const int Kennedy_Id = 27;

unsigned long nextLaunchTime_Epoch;

tmElements_t nextLaunchTime;  // time elements structure
tmElements_t countdownTime;  // time elements structure
tmElements_t countdownTimePrev;  // time elements structure
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

  setupWiFi();

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

    if( (nextLaunchTime_Epoch - timeClient.getEpochTime()) <= 0 )
    {
      if( (nextLaunchTime_Epoch - timeClient.getEpochTime() + 1800) <0 )
      {
        nextLaunchTime_Epoch = findAndGetNextLaunchTime();
      }
      else
      {
        printZero7Seg();
      }
    }

    else
    {
      if((nextLaunchTime_Epoch - timeClient.getEpochTime())%7200 == 0)
      {
        nextLaunchTime_Epoch = findAndGetNextLaunchTime();
      }
      countdownTime.Day = countdownTime.Day - 1;

      if(countdownTime.Second != countdownTimePrev.Second)
      {
        printTime7Seg(countdownTime);
        countdownTimePrev = countdownTime;

        Serial.print(countdownTime.Day);
        Serial.print(":");
        Serial.print(countdownTime.Hour);
        Serial.print(":");
        Serial.print(countdownTime.Minute);
        Serial.print(":");
        Serial.println(countdownTime.Second);
      }

    }



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
          return "";
        }
  
        https.end();
      } else {
        Serial.printf("[HTTPS] Unable to connect\n");
        return "";
      }

      // End extra scoping block
    }
  
    delete client;
  } else {
    Serial.println("Unable to create client");
    return "";
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

  return 1;
}

void printZero7Seg()
{
  lc.setDigit(0,7,0,false);
  lc.setDigit(0,6,0,true);
  lc.setDigit(0,5,0,false);
  lc.setDigit(0,4,0,true);
  lc.setDigit(0,3,0,false);
  lc.setDigit(0,2,0,true);
  lc.setDigit(0,1,0,false);
  lc.setDigit(0,0,0,false);
}

unsigned long findAndGetNextLaunchTime()
{
        
  String FullResponse = spaceDevHttpGet(baseURL);
  
  int locationId = getLocationId(FullResponse);
  Serial.println(locationId);

  timeClient.update();
  unsigned long temp_launch_epoch = getLaunchTime(FullResponse);

  while( !((locationId == Cape_Id) || (locationId == Kennedy_Id)) || !((int)temp_launch_epoch > timeClient.getEpochTime()) )
  {
    delay(1000);
    Serial.println(locationId);
    Serial.println(getNextURL(FullResponse));
    FullResponse = spaceDevHttpGet(getNextURL(FullResponse));
    locationId = getLocationId(FullResponse);
    Serial.println(FullResponse);
    temp_launch_epoch = getLaunchTime(FullResponse);
  }
  return temp_launch_epoch;

}

void setupWiFi()
{
    // Use this to personalize DHCP hostname (RFC952 conformed)
  ESP_WiFiManager ESP_wifiManager("AutoConnectAP");

  ESP_wifiManager.setDebugOutput(true);

  //set custom ip for portal
  ESP_wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 69, 1), IPAddress(192, 168, 69, 1), IPAddress(255, 255, 255, 0));

  ESP_wifiManager.setMinimumSignalQuality(-1);

  // From v1.0.10 only
  // Set config portal channel, default = 1. Use 0 => random channel from 1-13
  ESP_wifiManager.setConfigPortalChannel(0);

  // We can't use WiFi.SSID() in ESP32 as it's only valid after connected.
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();

  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);

  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(30); //If no access point name has been previously entered disable timeout.
    Serial.println("Got stored Credentials. Timeout 30s");
  }
  else
  {
    Serial.println("No stored Credentials. No timeout");
  }

  String chipID = String(ESP_getChipId(), HEX);
  chipID.toUpperCase();

  // SSID and PW for Config Portal
  String AP_SSID = "ESP_" + chipID + "_LaunchCountdownClock";
  String AP_PASS = "MyESP_" + chipID;

  // Get Router SSID and PASS from EEPROM, then open Config portal AP named "ESP_XXXXXX_AutoConnectAP" and PW "MyESP_XXXXXX"
  // 1) If got stored Credentials, Config portal timeout is 60s
  // 2) If no stored Credentials, stay in Config portal until get WiFi Credentials
  ESP_wifiManager.autoConnect(AP_SSID.c_str(), AP_PASS.c_str());
  //or use this for Config portal AP named "ESP_XXXXXX" and NULL password
  //ESP_wifiManager.autoConnect();

  //if you get here you have connected to the WiFi
  Serial.println("WiFi connected");

}

