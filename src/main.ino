static String name = "noszlop_kijelzo"; //to csiraztato
static String ver = "1_9";              //diff to 1_8: fix nethiba riasztás, gscript id frissítés
//////////////////////////////////////////////
////////////CONFIG////////////////////////////
#include "secrets.h"
Secrets sec;
float csir_homerseklet;
int csir_last_on;
int csir_timeout = 300;
boolean csir_alarm_on = true;
int csir_futes = -1;

float uhaz_homerseklet = 100;
float noszlop_uveghaz_alarm = 2;
int uhaz_last_on;
int uhaz_timeout = 300;
boolean uhaz_alarm_on = true;
int uhaz_futes = -1;

float inkub_homerseklet = 100;
float noszlop_inkub_alarm = 2;
int inkub_last_on;
int inkub_timeout = 300;
boolean inkub_alarm_on = true;
int inkub_futes = -1;

float telikert_homerseklet = 100;
float noszlop_telikert_alarm = 2;
int telikert_last_on;
int telikert_timeout = 300;
boolean telikert_alarm_on = true;
int telikert_futes = -1;

int network_timeout = 0;
int pinginterval = 1;
int update_interval = 5;
const uint8_t Googlefingerprint[20] = {0x63, 0x92, 0xD6, 0x31, 0x89, 0x30, 0x9B, 0x7A, 0x94, 0x33, 0x71, 0x67, 0xB5, 0x9F, 0xDE, 0x99, 0x69, 0xA1, 0x88, 0xF7};
const uint8_t Discordfingerprint[20] = {0x2D, 0x08, 0xE9, 0x2D, 0x0A, 0x54, 0x5A, 0xD5, 0xB4, 0x0A, 0x57, 0xF6, 0x68, 0x85, 0x1A, 0x79, 0x52, 0xE1, 0xFA, 0x65};
const String update_server = sec.update_server;
#define USE_SERIAL Serial

const String GScriptId = sec.gID;
const String discord_chanel = sec.discord_chanel;

////////////CONFIG////////////////////////////
//////////////////////////////////////////////

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
ESP8266WiFiMulti WiFiMulti;
WiFiClient client;

#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

LiquidCrystal_PCF8574 lcd(0x27); // set the LCD address to 0x27 for a 16 chars and 2 line display

//////////////////////////////////////////////
////////////SETUP ////////////////////////////
void setup()
{
  //test_lcd
  lcd.begin(20, 4);
  lcd.setBacklight(255);
  lcd.home();
  lcd.clear();
  lcd.print("Hello!");
  delay(1000);
  lcd.setBacklight(0);
  delay(400);
  lcd.setBacklight(255);
  alarm("Teszt riasztas!");
  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);
  USE_SERIAL.println();
  USE_SERIAL.print("name: ");
  USE_SERIAL.println(name);
  USE_SERIAL.print("ver: ");
  USE_SERIAL.println(ver);
  lcd.clear();
  lcd.print("mindjart kesz...");
  WiFiManager wifiManager;
  wifiManager.setTimeout(30);
  wifiManager.autoConnect("mocsigoncska_ap");
  Serial.println("connected...yeey :)");
  delay(1000);

  GsheetPost(F("log"), "startup: " + name + " " + ver);
  discordPost("startup: " + name + " " + ver);

  updateFunc(name, ver);
}

////////////SETUP ////////////////////////////
//////////////////////////////////////////////

//////////////////////////////////////////////
////////////LOOP ////////////////////////////
int i = 0;
int j = 0;
void loop()
{
  Serial.println("loop...");
  delay(1000 * pinginterval);
  getdataUhazInkub();
  delay(1500);
  getdataCsir();
  delay(1500);
  getdataTelikert();
  delay(1500);
  getconfig();
  kijelzo();
  Serial.print("----csir last on: ");
  Serial.println(csir_last_on);

  if (csir_alarm_on == true)
  {
    if (csir_last_on > csir_timeout)
    {
      alarm("nem latom a csiraztatot!");
    }
  }

  if (uhaz_alarm_on == true)
  {
    if (uhaz_homerseklet < noszlop_uveghaz_alarm)
    {
      alarm("Uhaz Hideg! " + String(uhaz_homerseklet));
    }
    if (uhaz_last_on > uhaz_timeout)
    {
      alarm("nem latom az uveghazat!");
    }
  }

  if (inkub_alarm_on == true)
  {
    if (inkub_homerseklet < noszlop_inkub_alarm)
    {
      alarm("Inkub Hideg! " + String(inkub_homerseklet));
    }
    if (inkub_last_on > inkub_timeout)
    {
      alarm("nem latom az Inkubatort!");
    }
  }

    if (telikert_alarm_on == true)
  {
    if (telikert_homerseklet < noszlop_telikert_alarm)
    {
      alarm("Telikert Hideg! " + String(telikert_homerseklet));
    }
    if (telikert_last_on > telikert_timeout)
    {
      alarm("nem latom az Telikertet!");
    }
  }

  Serial.print("----uhaz hom: ");
  Serial.print(uhaz_homerseklet);
  Serial.print(" -- treshold: ");
  Serial.println(noszlop_uveghaz_alarm);

  Serial.print("----inkub hom: ");
  Serial.print(inkub_homerseklet);
  Serial.print(" -- treshold: ");
  Serial.println(noszlop_inkub_alarm);

  Serial.print("----uhaz last on: ");
  Serial.print(uhaz_last_on);
  Serial.print(" -- treshold: ");
  Serial.println(uhaz_timeout);

  Serial.print("----inkub last on: ");
  Serial.print(inkub_last_on);
  Serial.print(" -- treshold: ");
  Serial.println(inkub_timeout);

  Serial.print("----telikert last on: ");
  Serial.print(telikert_last_on);
  Serial.print(" -- treshold: ");
  Serial.println(telikert_timeout);

  j++;
  if (j > update_interval)
  {
    j = 0;
    updateFunc(name, ver);
  }
  if (network_timeout > 5)
    alarm("net hiba");
}

////////////LOOP ////////////////////////////
//////////////////////////////////////////////

/////////////////////////////////////////////
////////////KIJELZO//////////////////////////
void kijelzo()
{
  lcd.setBacklight(255);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("csir: ");
  if (csir_last_on > csir_timeout)
  {
    lcd.print("--");
  }
  else
  {
    lcd.print(csir_homerseklet);
  }

  lcd.print(" R:");
  lcd.print(csir_alarm_on);
  lcd.print(" F:");
  lcd.print(csir_futes);

  lcd.setCursor(0, 1);
  lcd.print("tkert: ");
  if (telikert_last_on > telikert_timeout)
  {
    lcd.print("--");
  }
  else
  {
    lcd.print(telikert_homerseklet);
  }
  lcd.print(" R:");
  lcd.print(telikert_alarm_on);
  lcd.print(" F:");
  lcd.print(telikert_futes);

  lcd.setCursor(0, 2);
  lcd.print("inkub: ");
  if (inkub_last_on > inkub_timeout)
  {
    lcd.print("--");
  }
  else
  {
    lcd.print(inkub_homerseklet);
  }
  lcd.print(" R:");
  lcd.print(inkub_alarm_on);
  lcd.print(" F:");
  lcd.print(inkub_futes);

  lcd.setCursor(0, 3);
  lcd.print("uhaz: ");
  if (uhaz_last_on > uhaz_timeout)
  {
    lcd.print("--");
  }
  else
  {
    lcd.print(uhaz_homerseklet);
  }
  lcd.print(" R:");
  lcd.print(uhaz_alarm_on);
  lcd.print(" F:");
  lcd.print(uhaz_futes);
}

void alarm(String message)
{
  tone(D8, 1800, 1000);
  Serial.println("alarm: " + message);
  lcd.setBacklight(255);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("     !JUJUJ!");
  lcd.setCursor(0, 1);
  lcd.print(message);
  lcd.setBacklight(0);
  delay(400);
  lcd.setBacklight(255);
  delay(1000);
  lcd.setBacklight(0);
  delay(400);
  lcd.setBacklight(255);
  lcd.setBacklight(0);
  delay(400);
  lcd.setBacklight(255);
  tone(D8, 1500, 4000);
}

////////////KIJELZO///////////////////////
/////////////////////////////////////////////

/////////////////////////////////////////////
////////////HTTPUPDATE////////////////////////
void updateFunc(String Name, String Version)
{
  HTTPClient http;

  String url = update_server + "/check?" + "name=" + Name + "&ver=" + Version;
  Serial.print("[HTTP] check at " + url);
  if (http.begin(client, url))
  { // HTTP

    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    delay(10000); //wait for bootup of the server
    httpCode = http.GET();
    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = http.getString();
        Serial.println(payload);
        if (payload.indexOf("bin") > 0)
        {
          httpUpdateFunc(update_server + payload);
        }
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }
}

void httpUpdateFunc(String update_url)
{
  if ((WiFiMulti.run() == WL_CONNECTED))
  {

    // The line below is optional. It can be used to blink the LED on the board during flashing
    // The LED will be on during download of one buffer of data from the network. The LED will
    // be off during writing that buffer to flash
    // On a good connection the LED should flash regularly. On a bad connection the LED will be
    // on much longer than it will be off. Other pins than LED_BUILTIN may be used. The second
    // value is used to put the LED on. If the LED is on with HIGH, that value should be passed
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    // Add optional callback notifiers
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, update_url);
    // Or:
    //t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      USE_SERIAL.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      USE_SERIAL.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      USE_SERIAL.println("HTTP_UPDATE_OK");
      break;
    }
  }
}

void update_started()
{
  USE_SERIAL.println("CALLBACK:  HTTP update process started");
}

void update_finished()
{
  USE_SERIAL.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total)
{
  USE_SERIAL.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err)
{
  USE_SERIAL.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}
////////////HTTPUPDATE////////////////////////
/////////////////////////////////////////////

//////////////////////////////////////////////
////////////HTTP  ////////////////////////////
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>

void GsheetPost(String sheet_name, String datastring)
{
  Serial.println(F("POST to spreadsheet:"));
  String url = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec";
  String payload = String("{\"command\": \"appendRow\", \  \"sheet_name\": \"") + sheet_name + "\", \ \"values\": " + "\"" + datastring + "\"}";

  Serial.println(POSTTask(url, Googlefingerprint, payload));
};

void discordPost(String message)
{

  String payload = "{\"content\": \"" + message + "\"}";
  String url = discord_chanel;
  Serial.println(POSTTask(url, Discordfingerprint, payload));
};

String GETTask(String url, const uint8_t Fingeprint[20], uint32_t neededHeap)
{
  if (ESP.getFreeHeap() < neededHeap)
  {
    Serial.println(F("too few heap left"));
    discordPost("too few heap:" + String(ESP.getFreeHeap()));
    ESP.reset();
  }
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  https.setFollowRedirects(true);
  if (https.begin(*client, url))
  {
    Serial.print(F("[HTTPS] GET "));
    Serial.println(url);
    HeapPrintTask();

    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode == 302)
      {
        String redirectUrl = https.getLocation();
        https.end();
        return redirectUrl;
      }
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = https.getString();
        Serial.println(payload);
        https.end();

        return payload;
      }
    }
    else
    {
      Serial.print(F("[HTTPS] GET... failed, error: "));
      Serial.println(httpCode);
      https.end();
      return "";
    }

    https.end();
  }
  else
  {
    Serial.println(F("[HTTPS] Unable to connect"));
    return "";
  }
  return "";
};

String POSTTask(String url, const uint8_t Fingeprint[20], String payload)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  if (https.begin(*client, url))
  {
    Serial.print(F("[HTTPS] POST "));
    Serial.print(url);
    Serial.print(" --> ");
    Serial.println(payload);
    https.addHeader(F("Content-Type"), F("application/json"));
    https.setFollowRedirects(true);

    int httpCode = https.POST(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      Serial.print(F("[HTTPS] POST... code: "));
      Serial.println(httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = https.getString();
        https.end();
        return payload;
      }
    }
    else
    {
      Serial.print(F("[HTTPS] POST... failed, error: "));
      Serial.println(httpCode);
      https.end();
      return "";
    }

    https.end();
  }
  else
  {
    Serial.println(F("[HTTPS] Unable to connect"));
    return "";
  }
  return "";
};

////////////HTTP  ////////////////////////////
//////////////////////////////////////////////

//////////////////////////////////////////////
////////////GETCONFIG/////////////////////////

const size_t capacity = JSON_OBJECT_SIZE(13) + 950;
DynamicJsonDocument doc(capacity);

void getdataUhazInkub()
{
  String baseurl = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec?";

  String params = String("uhaz_homerseklet=0&uhaz_last_on=0&uhaz_futes=0")+ "&" + String("inkub_homerseklet=0&inkub_last_on=0&inkub_futes=0");

  String url = baseurl + params;
  String response = GETTask(url, Googlefingerprint, 1200);
  network_timeout++;
  if (response.length() > 1)
  {
    network_timeout = 0;
    deserializeJson(doc, response);

    uhaz_homerseklet = doc["uhaz_homerseklet"];
    uhaz_last_on = doc["uhaz_last_on"];
    uhaz_futes = doc["uhaz_futes"];

    inkub_homerseklet = doc["inkub_homerseklet"];
    inkub_last_on = doc["inkub_last_on"];
    inkub_futes = doc["inkub_futes"];

    Serial.println("data got:");

    Serial.println("uhaz_homerseklet =" + String(uhaz_homerseklet));
    Serial.println("uhaz_last_on =" + String(uhaz_last_on));
    Serial.println("uhaz_futes =" + String(uhaz_futes));

    Serial.println("inkub_homerseklet =" + String(inkub_homerseklet));
    Serial.println("inkub_last_on =" + String(inkub_last_on));
    Serial.println("inkub_futes =" + String(inkub_futes));
  }
}

void getdataCsir()
{
  String baseurl = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec?";

  String params = "csir_homerseklet=0&csir_last_on=0&csir_futes=0";

  String url = baseurl + params;
  String response = GETTask(url, Googlefingerprint, 1200);
  network_timeout++;
  if (response.length() > 1)
  {
    network_timeout = 0;
    deserializeJson(doc, response);

    csir_homerseklet = doc["csir_homerseklet"];
    csir_last_on = doc["csir_last_on"];
    csir_futes = doc["csir_futes"];

    Serial.println("data got:");

    Serial.println("csir_homerseklet =" + String(csir_homerseklet));
    Serial.println("csir_last_on =" + String(csir_last_on));
    Serial.println("csir_futes =" + String(csir_futes));
  }
}

void getdataTelikert()
{
  String baseurl = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec?";

  String params = "telikert_homerseklet=0&telikert_last_on=0&telikert_futes=0";

  String url = baseurl + params;
  String response = GETTask(url, Googlefingerprint, 1200);
  network_timeout++;
  if (response.length() > 1)
  {
    network_timeout = 0;
    deserializeJson(doc, response);

    telikert_homerseklet = doc["telikert_homerseklet"];
    telikert_last_on = doc["telikert_last_on"];
    telikert_futes = doc["telikert_futes"];

    Serial.println("data got:");
    Serial.println("telikert_homerseklet =" + String(telikert_homerseklet));
    Serial.println("telikert_last_on =" + String(telikert_last_on));
    Serial.println("telikert_futes =" + String(telikert_futes));

  }
}


void getconfig()
{
  String baseurl = String(F("https://script.google.com/macros/s/")) + String(GScriptId) + "/exec?";

  String params = "update_interval=0&pinginterval=0&csir_timeout=0&telikert_timeout=0&noszlop_uveghaz_alarm=0&uhaz_timeout=0&inkub_alarm_on=0&uhaz_alarm_on=0&csir_alarm_on=0&telikert_alarm_on=0";

  String url = baseurl + params;
  String response = GETTask(url, Googlefingerprint, 1200);

  if (response.length() > 1)
  {
    deserializeJson(doc, response);
    noszlop_uveghaz_alarm = doc["noszlop_uveghaz_alarm"];
    pinginterval = doc["pinginterval"];
    update_interval = doc["update_interval"];
    csir_timeout = doc["csir_timeout"];
    uhaz_timeout = doc["uhaz_timeout"];
    uhaz_timeout = doc["telikert_timeout"];

    inkub_alarm_on = doc["inkub_alarm_on"];
    csir_alarm_on = doc["csir_alarm_on"];
    uhaz_alarm_on = doc["uhaz_alarm_on"];
    telikert_alarm_on = doc["telikert_alarm_on"];

    Serial.println("Config got:");
    Serial.println("noszlop_uveghaz_alarm =" + String(noszlop_uveghaz_alarm));
    Serial.println("pinginterval =" + String(pinginterval));
    Serial.println("update_interval =" + String(update_interval));
    Serial.println("csir_timeout =" + String(csir_timeout));
    Serial.println("uhaz_timeout =" + String(uhaz_timeout));
    Serial.println("inkub_alarm_on =" + String(inkub_alarm_on));
    Serial.println("csir_alarm_on =" + String(csir_alarm_on));
    Serial.println("uhaz_alarm_on =" + String(uhaz_alarm_on));
  }
}

////////////GETCONFIG/////////////////////////
//////////////////////////////////////////////
void HeapPrintTask()
{
  /* lcd.setCursor(0, 2);
    lcd.print("                                        ");
    lcd.setCursor(0, 2);
    lcd.print("Free heap: ");
    lcd.print(ESP.getFreeHeap());*/

  // we could use getFreeHeap() getMaxFreeBlockSize() and getHeapFragmentation()
  // or all at once:
  uint32_t free;
  uint16_t max;
  uint8_t frag;
  ESP.getHeapStats(&free, &max, &frag);
  float freepercent = free;
  freepercent = freepercent / 81920 * 100;
  Serial.printf("free: %5d - max: %5d - freepercent: %2.1f%% - frag: %3d%% <- ", free, max, freepercent, frag);
  Serial.println();
}
