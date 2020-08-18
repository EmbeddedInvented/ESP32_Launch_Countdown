


//***************************WIFI MANAGER CONFIGURATION****************************
#define _WIFIMGR_LOGLEVEL_    3

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>

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

#warning Using DHCP IP

#define USE_CONFIGURABLE_DNS      false

#include <ESP_WiFiManager.h>              //https://github.com/khoih-prog/ESP_WiFiManager

//**********************END WIFI MANAGER CONFIGURATION **********************************

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial);

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
  String AP_SSID = "ESP_" + chipID + "_AutoConnectAP";
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

void loop()
{
  // put your main code here, to run repeatedly
}