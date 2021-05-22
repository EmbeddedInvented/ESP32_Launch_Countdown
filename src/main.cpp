/**
   Main.cpp

    Created on: 10.10.2020

    Main program for the Space Launch countdown clock. 

    This program uses the Space Devs (thespacedevs.com) free API to retrieve the next space launch. 
    
    It then uses ntp time to calculate the current countdown and display it on 8 7-segment displays.

    All configuration (WiFi, settings) is performed by the Autoconnect library (https://hieromon.github.io/AutoConnect/)

    All code is done using the Arduino framework
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
#include <Preferences.h>


//predefine all of our functions 
void setClock();
String spaceDevHttpGet(String URL);
String getNextURL(String rawData);
int getLaunchTime(String rawData);
int printTime7Seg(tmElements_t countdown);
unsigned long findAndGetNextLaunchTime();
void dispayIPAddress(String rawIP);
void deleteAllCredentials();
void printString7Seg(String input, bool colon);
bool portalStartFn(IPAddress& ip);
bool connectedFn(IPAddress& ip);
String saveURL(AutoConnectAux& aux, PageArgument& args);
String onAPISettings(AutoConnectAux& aux, PageArgument& args);

//maximum expected size of the JSON responses that we will be parsing
const size_t capacity = 5000;

String baseURL;
int32_t digit_brightness;

//Create our 7-segment interface
LedController lc =LedController(GPIO_NUM_23,GPIO_NUM_18,GPIO_NUM_5,1);


unsigned long nextLaunchTime_Epoch;

tmElements_t nextLaunchTime;  // time elements structure
tmElements_t countdownTime;  // time elements structure
tmElements_t countdownTimePrev;  // time elements structure
time_t nextLaunch_timestamp; // a timestamp


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0);

//Preferences used to save data in non-volatile memory
Preferences prefs;

//Autoconnect setup code
#define AC_DEBUG
WebServer Server;
AutoConnect       Portal(Server);
AutoConnectConfig Config;

//Autoconnect auxilliary pages setup code 
AutoConnectAux    SpaceSettings;
AutoConnectAux    APISettings("/api_settings", "API Settings");
AutoConnectText APIHeader("APIHeader", "TheSpaceDevs API Settings", "font-family:Arial;font-weight:bold;text-align:center;margin-bottom:10px;color:DarkSlateBlue");
AutoConnectText APICaption("APICaption", "Use this page to configure an optional API key, or change the request URL."
"Through the request URL you can change the launch locations of interest. Launch locations are configured through the 'Pad location ID' at the end of the URL."
"Multiple pad location IDs can be defined by adding more separated by commas. The default URL (for only KSC and Cape Canaveral) is https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=1&offset=0&location__ids=27,12"
"The device can only parse json data from the /launch/upcoming API endopoint, for luanches in the future."
"The device must be reset for changes to take effect. For info see the API documentation https://thespacedevs.com/");
AutoConnectAux    APISettings_Save("/api_save", "API Settings Saved", false);
AutoConnectInput URLInput("URLInput", "", "Request URL", "");
AutoConnectInput LEDBrightness("LEDBrightness", "", "Brightness (0-15)", "");
AutoConnectSubmit URLSubmit("URLSubmit", "Save", "/api_save");
AutoConnectText APISaveHeader("APISaveHeader", "Saved Succesfully", "font-family:Arial;font-weight:bold;text-align:center;margin-bottom:10px;color:DarkSlateBlue");

static const char SPACE_SETTINGS[] PROGMEM = R"(
{
  "title": "Device Settings",
  "uri": "/CD",
  "menu": true,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "Deletes all saved WiFi credentials and reboots. Device will need to be re-set-up",
      "style": "font-family:Arial;font-weight:bold;text-align:center;margin-bottom:10px;color:DarkSlateBlue"
    },
    {
      "name": "rWiFi",
      "type": "ACSubmit",
      "value": "RESET WIFI",
      "uri": "/rwifi"
    }
  ]
}
)";

void rootPage() {
  String  content =
    "<html>"
    "<head>"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<script type=\"text/javascript\">"
    "</script>"
    "</head>"
    "<body>"
    "<h2 align=\"center\" style=\"color:blue;margin:20px;\">Space Launch Countdown Clock Settings Page, click below to configure the device.</h2>"
    "<p></p><p style=\"padding-top:15px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
    "</body>"
    "</html>";
  Server.send(200, "text/html", content);
}

//Setup code to be run once on starup
void setup() {

  Serial.begin(115200);
  prefs.begin("spaceCountdown", false);


  //Enable our speaker pin, and disable by default
  pinMode(GPIO_NUM_27, OUTPUT);
  ledcSetup(0,5000,10);
  ledcAttachPin(GPIO_NUM_27, 0);
  ledcWrite(0, 0);

  lc.activateAllSegments();

  /* Set the brightness to a medium values */
  digit_brightness = prefs.getInt("brightness", 1);
  lc.setIntensity(digit_brightness);
  /* and clear the display */
  lc.clearMatrix();

  printString7Seg("booting ",false);

  Config.autoReconnect = true;
  Config.reconnectInterval= 1;
  Config.ota = AC_OTA_BUILTIN;
  Config.hostName = "SpaceCountdown";
  Config.portalTimeout = 120000; //2 minute portal timeout
  Config.apid = "Space_Countdown_" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  Config.psk = "spacecountdown";
  Portal.config(Config);
  
  SpaceSettings.load(SPACE_SETTINGS);
  APISettings.add({APIHeader,APICaption,URLInput,LEDBrightness,URLSubmit});
  APISettings_Save.add({APISaveHeader});

  Portal.join({SpaceSettings,APISettings,APISettings_Save});

  baseURL = prefs.getString("baseURL", "https://lldev.thespacedevs.com/2.2.0/launch/upcoming/?limit=1&offset=0&location__ids=27,12");
  Serial.print("Base URL Is: " + baseURL);

  APISettings.on(onAPISettings);

  Server.on("/rwifi",deleteAllCredentials);
  
  Portal.on("/api_save", saveURL);
  // Behavior a root path of ESP8266WebServer.
  Server.on("/", rootPage);
  
  //Display message on screen when captive portal is properly loaded
  Portal.onDetect(portalStartFn);
  //Display message on screen when Wifi is connected 
  Portal.onConnect(connectedFn);
  // Establish a connection with an autoReconnect option.
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    dispayIPAddress(WiFi.localIP().toString());
  }
}

void loop() {
  if(WiFi.isConnected())
  {
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
          printString7Seg("00000000",true);
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
  else
  {
    Portal.handleClient();
  }
}


//function to HTTP GET the data from the spacedev API
String spaceDevHttpGet(String URL)
{
  WiFiClientSecure *client = new WiFiClientSecure;
  if(client) {
    client->setInsecure(); //connect insecurely, no sensitive data is changing hands
    {
      // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is 
      HTTPClient https;
  
      Serial.print("[HTTPS] begin...\n");
      if (https.begin(*client, URL)) {  // HTTPS
        Serial.print("[HTTPS] GET...\n");
        // start connection and send HTTP header
        //https.addHeader();
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
          return "fail";
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


void printString7Seg(String input, bool colon)
{
  for(int i=0; i<8; i++)
  {
    lc.setChar(0,i,input.substring(i,i+1)[0],colon);
  }
}

unsigned long findAndGetNextLaunchTime()
{
        
  String FullResponse = spaceDevHttpGet(baseURL);
  if(FullResponse == "fail")
  {
    return 0; //TODO
  }
  
  timeClient.update();
  unsigned long temp_launch_epoch = getLaunchTime(FullResponse);

  return temp_launch_epoch;

}

//function to display the IP address on the 7-segment displays
void dispayIPAddress(String rawIP)
{
  int ip_len = rawIP.length() + 1;
  char IP[ip_len];
  rawIP.toCharArray(IP, ip_len);
  char * ipchunk[4];
  char digit;

  ipchunk[0] = strtok(IP,".");
  for(int i = 1; i<4; i++)
  {
    ipchunk[i] = strtok(NULL,".");
  }
  //Print first digits with "IP"
  lc.setChar(0,0,'I',true);
  lc.setChar(0,1,'P',true);
  for(int i = 0; i<(strlen(ipchunk[0])); i++)
  {
    memcpy(&digit,ipchunk[0] + i,1);
    lc.setChar(0,2+i,digit,false);
  }
  for(int i = (strlen(ipchunk[0]) + 2); i<8; i++)
  {
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
      lc.setChar(0,2+i,digit,false);
    }
    for(int i = (strlen(ipchunk[j]) + 2); i<8; i++)
    {
      lc.setChar(0,i,' ',false);
    }
    delay(2000);  
  }
}

void deleteAllCredentials() {
  AutoConnectCredential credential;
  station_config_t config;
  uint8_t ent = credential.entries();
  WiFi.disconnect(true,true);

  while (ent--) {
    credential.load((int8_t)0, &config);
    credential.del((const char*)&config.ssid[0]);
  }
  ESP.restart();
}

bool portalStartFn(IPAddress& ip)
{
  printString7Seg("portal  ",false);
  return true;
}

bool connectedFn(IPAddress& ip)
{
  printString7Seg("success ", false);
  delay(2000);
  return true;
}

String saveURL(AutoConnectAux& aux, PageArgument& args) {
  
  
  AutoConnectAux&   API_setting = *Portal.aux(Portal.where());
  String S_URLInput = API_setting["URLInput"].value;
  S_URLInput.trim();
  prefs.putString("baseURL", S_URLInput);
  
  
  String SLEDBrightness = API_setting["LEDBrightness"].value;
  SLEDBrightness.trim();
  digit_brightness = SLEDBrightness.toInt();
  if(digit_brightness < 16 && digit_brightness >= 0) {
    prefs.putInt("brightness", int32_t(digit_brightness));
    lc.setIntensity(digit_brightness);
  }

  return "";
}

//Use this to fill in the API settings page with the current values 
String onAPISettings(AutoConnectAux& aux, PageArgument& args) {
  AutoConnectInput& input1 = aux.getElement<AutoConnectInput>("URLInput");
  AutoConnectInput& input2 = aux.getElement<AutoConnectInput>("LEDBrightness");

  input1.value = prefs.getString("baseURL", "https://lldev.thespacedevs.com/2.2.0/launch/upcoming/?limit=1&offset=0&location__ids=27,12");
  input2.value = prefs.getInt("brightness", 1);

  return "";
}