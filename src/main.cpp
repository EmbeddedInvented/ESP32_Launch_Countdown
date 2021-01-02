/**
   BasicHTTPSClient.ino

    Created on: 14.10.2018

*/

#include <Arduino.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <LedController.hpp>
#include <WebServer.h>
#include <time.h>
#include <AutoConnect.h>



void setClock();
String spaceDevHttpGet(String URL);
int getLocationId(String rawData);
String getNextURL(String rawData);
int getLaunchTime(String rawData);
int printTime7Seg(tmElements_t countdown);
unsigned long findAndGetNextLaunchTime();
void printZero7Seg();
void test7Seg();
void dispayIPAddress(String rawIP);

const size_t capacity = 5000;

const String baseURL = "https://ll.thespacedevs.com/2.0.0/launch/upcoming/?limit=1&offset=0&search=USA";


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


//Autoconnect setup code
WebServer Server;

AutoConnect       Portal(Server);
AutoConnectConfig Config;       // Enable autoReconnect supported on v0.9.4

void rootPage() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<script type=\"text/javascript\">"
    "</script>"
    "</head>"
    "<body>"
    "<h2 align=\"center\" style=\"color:blue;margin:20px;\">Space Launch Countdown Clock Settings Page updated over the air!</h2>"
    "<p></p><p style=\"padding-top:15px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";
  Server.send(200, "text/html", content);
}

void setup() {

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  pinMode(GPIO_NUM_27, OUTPUT);


  ledcSetup(0,5000,10);
  ledcAttachPin(GPIO_NUM_27, 0);
  ledcWrite(0, 0);

  lc.activateAllSegments();
  /* Set the brightness to a medium values */
  lc.setIntensity(8);
  /* and clear the display */
  lc.clearMatrix();

  test7Seg();



  Config.autoReconnect = true;
  Config.ota = AC_OTA_BUILTIN;
  Config.hostName = "SpaceCountdown";
  Portal.config(Config);

  // Behavior a root path of ESP8266WebServer.
  Server.on("/", rootPage);

  // Establish a connection with an autoReconnect option.
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    dispayIPAddress(WiFi.localIP().toString());
  }


}

void loop() {
 nextLaunchTime_Epoch = findAndGetNextLaunchTime();
  while(1)
  {
    Portal.handleClient();
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
  lc.setDigit(0,0,(int)countdown.Day/10,true);
  lc.setDigit(0,1,(int)countdown.Day%10,true);
  lc.setDigit(0,2,(int)countdown.Hour/10,true);
  lc.setDigit(0,3,(int)countdown.Hour%10,true);
  lc.setDigit(0,4,(int)countdown.Minute/10,true);
  lc.setDigit(0,5,(int)countdown.Minute%10,true);
  lc.setDigit(0,6,(int)countdown.Second/10,true);
  lc.setDigit(0,7,(int)countdown.Second%10,true);

  return 1;
}

void printZero7Seg()
{
  lc.setDigit(0,7,0,true);
  lc.setDigit(0,6,0,true);
  lc.setDigit(0,5,0,true);
  lc.setDigit(0,4,0,true);
  lc.setDigit(0,3,0,true);
  lc.setDigit(0,2,0,true);
  lc.setDigit(0,1,0,true);
  lc.setDigit(0,0,0,true);
}

void test7Seg()
{
  lc.setDigit(0,0,0xb,false);
  lc.setDigit(0,1,0,false);
  lc.setDigit(0,2,0,false);
  lc.setChar(0,3,'t',false);
  lc.setChar(0,4,'I',false);
  lc.setChar(0,5,'n',false);
  lc.setChar(0,6,'g',false);
  lc.setChar(0,7,'-',false);

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

void dispayIPAddress(String rawIP)
{
  int ip_len = rawIP.length() + 1;
  char IP[ip_len];
  rawIP.toCharArray(IP, ip_len);
  char * ipchunk[4];
  char digit;

  ipchunk[0] = strtok(IP,".");
  Serial.println(ipchunk[0]);
  for(int i = 1; i<4; i++)
  {
    ipchunk[i] = strtok(NULL,".");
    Serial.println(ipchunk[i]);
  }
  //Print first digits with "IP"
  lc.setChar(0,0,'I',true);
  lc.setChar(0,1,'P',true);
  for(int i = 0; i<(strlen(ipchunk[0])); i++)
  {
    memcpy(&digit,ipchunk[0] + i,1);
    Serial.println(digit);
    lc.setChar(0,2+i,digit,false);
  }
  for(int i = (strlen(ipchunk[0]) + 2); i<8; i++)
  {
    Serial.println(i);
    lc.setChar(0,i,' ',false);
  }
  delay(2000);
  //print 2nd, 3rd, 4th digits:
  for(int j = 1; j < 4; j++)
  {
    lc.setChar(0,0,' ',false);
    lc.setChar(0,1,' ',false);
    for(int i = 0; i<(strlen(ipchunk[j])); i++)
    {
      memcpy(&digit,ipchunk[j] + i,1);
      Serial.println(digit);
      lc.setChar(0,2+i,digit,false);
    }
    for(int i = (strlen(ipchunk[j]) + 2); i<8; i++)
    {
      Serial.println(i);
      lc.setChar(0,i,' ',false);
    }
    delay(2000);  
  }

}
