/*
  Rui Santos
  Complete project details at Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-http-get-post-arduino/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Arduino_JSON.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <LedControl.h>
#include <TimeLib.h>
#include <NTPClient.h>

const char* ssid = "Panic! At the Cisco";
const char* password = "8015463911";

LedControl  lc=LedControl(D8,D7,D6,1);

//Your Domain name with URL path or IP address with path
String origServerName = "https://ll.thespacedevs.com/2.0.0/launch/upcoming/?limit=1&offset=0&search=USA";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 10000;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

String FullResponse;
String Response_Results;
String Response_Pad;
int Response_Id;

const int Cape_Id = 12;
const int Kennedy_Id = 27;

unsigned long nextLaunchTime_Epoch;

tmElements_t nextLaunchTime;  // time elements structure
tmElements_t countdownTime;  // time elements structure
time_t nextLaunch_timestamp; // a timestamp


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);


String httpGETRequest(String serverName);
int getLocationId(String rawResponse);
String getNextURL(String rawResponse);
unsigned long getLaunchTime(String rawResponse);
String formatSubJson(String subJson);
unsigned long findAndGetNextLaunchTime();

unsigned long getCurrentTime();
int printTime7Seg(tmElements_t countdown);


void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

   // Initialize the MAX7219 device
  lc.shutdown(0,false);   // Enable display
  lc.setIntensity(0,15);  // Set brightness level (0 is min, 15 is max)
  lc.clearDisplay(0);     // Clear display register

  timeClient.begin();
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


//String httpGETRequest(const char* serverName) {
String httpGETRequest(String serverName) {
  
  
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(*client, serverName);
  
  Serial.println(serverName);
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}


int getLocationId(String rawResponse) //Get the ID of the next launch pad to ensure that it is in Florida
{
  Serial.println("getting location ID");
  JSONVar rawObj = JSON.parse(rawResponse);
  JSONVar rawKeys = rawObj.keys();

  JSONVar resultsObj = JSON.parse(formatSubJson(JSON.stringify(rawObj[rawKeys[3]])));
  JSONVar resultKeys = resultsObj.keys();

  JSONVar padObj = JSON.parse(JSON.stringify(resultsObj[resultKeys[19]]));
  JSONVar padKeys = padObj.keys();

  JSONVar locationObj = JSON.parse(JSON.stringify(padObj[padKeys[9]]));
  JSONVar locationKeys = locationObj.keys();
  
  String locationId = JSON.stringify(locationObj[locationKeys[0]]);

  return locationId.toInt();
}

String getNextURL(String rawResponse)
{
  JSONVar rawObj = JSON.parse(rawResponse);
  JSONVar rawKeys = rawObj.keys();

  return formatSubJson(JSON.stringify(rawObj[rawKeys[1]]));
}


unsigned long getLaunchTime(String rawResponse)
{
  Serial.println("getting launch time");
  JSONVar rawObj = JSON.parse(rawResponse);
  JSONVar rawKeys = rawObj.keys();

  JSONVar resultsObj = JSON.parse(formatSubJson(JSON.stringify(rawObj[rawKeys[3]])));
  JSONVar resultKeys = resultsObj.keys();

  String launchTimeString = formatSubJson(JSON.stringify(resultsObj[resultKeys[6]]));

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

String formatSubJson(String subJson) //Removes enclosing brackets/quotes so that the Json/String can be processed again
{
  int len = subJson.length();
  subJson.remove(0,1);
  subJson.remove(len-2,1);
  return subJson;
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
  //Check WiFi connection status
  if(WiFi.status()== WL_CONNECTED)
  {
            
    FullResponse = httpGETRequest(origServerName);
    int locationId = getLocationId(FullResponse);
    Serial.println(locationId);

    while( !((locationId == Cape_Id) || (locationId == Kennedy_Id)) )
    {
      delay(1000);
      Serial.println(locationId);
      Serial.println(getNextURL(FullResponse));
      FullResponse = httpGETRequest(getNextURL(FullResponse));
      locationId = getLocationId(FullResponse);
      Serial.println(FullResponse);
    }
  return getLaunchTime(FullResponse);
    
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}
