#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <TimeLib.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <InfluxDbClient.h>
#include <LiquidCrystal_PCF8574.h>
#include <Wire.h>

#include "secrets.h"
Secrets sec;

//////////////////////////////////////////////
////////////CONFIG////////////////////////////
static String name = "noszlop_kijelzo";
static String ver = "2_8";

// Sensor data variables
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

float telikert_hutes_homerseklet = 100;
float noszlop_telikert_hutes_alarm = 2;
int telikert_hutes_last_on;
int telikert_hutes_timeout = 300;
boolean telikert_hutes_alarm_on = true;
int telikert_hutes_futes = -1;


int network_timeout = 0;
int pinginterval = 1;
int update_interval = 5;

const String update_server = sec.update_server;
#define USE_SERIAL Serial

// InfluxDB configuration
#define INFLUXDB_ORG "influxdata"
#define INFLUXDB_BUCKET "noszlop"
InfluxDBClient influx_client(sec.influx_url, INFLUXDB_ORG, INFLUXDB_BUCKET, sec.influx_token);
Point influxdb_line("noszlop_kijelzo"); // measurement name


////////////CONFIG////////////////////////////
//////////////////////////////////////////////

ESP8266WiFiMulti WiFiMulti;
WiFiClient client;

LiquidCrystal_PCF8574 lcd(0x27); // set the LCD address to 0x27 for a 16 chars and 2 line display

//////////////////////////////////////////////
////////////SETUP ////////////////////////////
void setup()
{
  // Initialize LCD
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

  // Initialize Serial
  USE_SERIAL.begin(115200);
  USE_SERIAL.setDebugOutput(true);
  USE_SERIAL.println();
  USE_SERIAL.print("name: ");
  USE_SERIAL.println(name);
  USE_SERIAL.print("ver: ");
  USE_SERIAL.println(ver);

  lcd.clear();
  lcd.print("mindjart kesz...");

  WiFiMulti.addAP(sec.known_ap.c_str(), sec.known_ap_pw.c_str());
  WiFiMulti.run();

  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.autoConnect("mocsigoncska_ap");
  USE_SERIAL.println("connected...yeey :)");


  // Print WiFi diagnostics
  USE_SERIAL.print("IP address: ");
  USE_SERIAL.println(WiFi.localIP());
  USE_SERIAL.print("Gateway: ");
  USE_SERIAL.println(WiFi.gatewayIP());
  USE_SERIAL.print("DNS: ");
  USE_SERIAL.println(WiFi.dnsIP());
  USE_SERIAL.print("Signal strength (RSSI): ");
  USE_SERIAL.print(WiFi.RSSI());
  USE_SERIAL.println(" dBm");

  // Test DNS resolution
  USE_SERIAL.println("Testing DNS resolution...");
  IPAddress testIP;
  if (WiFi.hostByName("discord.com", testIP)) {
    USE_SERIAL.print("discord.com resolved to: ");
    USE_SERIAL.println(testIP);
  } else {
    USE_SERIAL.println("DNS resolution failed for discord.com! Set DNS to 1.1.1.1");
    // Set custom DNS servers (Cloudflare DNS) to fix DNS resolution issues
    IPAddress dns1(1, 1, 1, 1);
    IPAddress dns2(1, 1, 1, 1);
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);
    if (WiFi.hostByName("discord.com", testIP)) {
      USE_SERIAL.print("discord.com resolved to: ");
      USE_SERIAL.println(testIP);
    } else {
      USE_SERIAL.println("DNS resolution failed for discord.com! uh-oh");
    }
      // Print WiFi diagnostics
  USE_SERIAL.print("IP address: ");
  USE_SERIAL.println(WiFi.localIP());
  USE_SERIAL.print("Gateway: ");
  USE_SERIAL.println(WiFi.gatewayIP());
  USE_SERIAL.print("DNS: ");
  USE_SERIAL.println(WiFi.dnsIP());
  USE_SERIAL.print("Signal strength (RSSI): ");
  USE_SERIAL.print(WiFi.RSSI());
  USE_SERIAL.println(" dBm");
  }

  // Configure WiFi settings for better stability
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // Initialize InfluxDB
  influxdb_line.addTag("name", name);
  influxdb_line.addTag("version", ver);
  influxdb_line.addField("event", "Startup");
  influx_client.writePoint(influxdb_line);
  influxdb_line.clearFields();

  discordPost("startup: " + name + " " + ver);
  
  getconfig();
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
  USE_SERIAL.println("loop...");
  delay(1000 * pinginterval);
  
  getdataUhaz();
  getdataInkub();
  getdataCsir();
  getdataTelikert();
  getdataTelikertHutes();
  getconfig();
  kijelzo();
  
  USE_SERIAL.print("----csir last on: ");
  USE_SERIAL.println(csir_last_on);

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

  if (telikert_hutes_alarm_on == true)
  {
    if (telikert_hutes_homerseklet > noszlop_telikert_hutes_alarm)
    {
      alarm("Telikert Hutes Magas! " + String(telikert_hutes_homerseklet));
    }
    if (telikert_hutes_last_on > telikert_hutes_timeout)
    {
      alarm("nem latom a Telikert Huteset!");
    }
  }

  USE_SERIAL.print("----uhaz hom: ");
  USE_SERIAL.print(uhaz_homerseklet);
  USE_SERIAL.print(" -- treshold: ");
  USE_SERIAL.println(noszlop_uveghaz_alarm);

  USE_SERIAL.print("----inkub hom: ");
  USE_SERIAL.print(inkub_homerseklet);
  USE_SERIAL.print(" -- treshold: ");
  USE_SERIAL.println(noszlop_inkub_alarm);

  USE_SERIAL.print("----uhaz last on: ");
  USE_SERIAL.print(uhaz_last_on);
  USE_SERIAL.print(" -- treshold: ");
  USE_SERIAL.println(uhaz_timeout);

  USE_SERIAL.print("----inkub last on: ");
  USE_SERIAL.print(inkub_last_on);
  USE_SERIAL.print(" -- treshold: ");
  USE_SERIAL.println(inkub_timeout);

  USE_SERIAL.print("----telikert last on: ");
  USE_SERIAL.print(telikert_last_on);
  USE_SERIAL.print(" -- treshold: ");
  USE_SERIAL.println(telikert_timeout);

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

  // csir
  lcd.setCursor(0, 0);
  lcd.print("csr: ");
  if (csir_last_on > csir_timeout)
  {
    lcd.print("--");
  }
  else
  {
    lcd.print(csir_homerseklet);
  }

  lcd.print("R:");
  lcd.print(csir_alarm_on);
  lcd.print("F:");
  lcd.print(csir_futes);

  // telikert
  lcd.setCursor(0, 1);
  lcd.print("tkr: ");
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
  lcd.print("F:");
  lcd.print(telikert_futes);

  // telikert_hutes
  lcd.setCursor(0, 2);
  lcd.print("thu: ");
  if (telikert_hutes_last_on > telikert_hutes_timeout)
  {
    lcd.print("--");
  }
  else
  {
    lcd.print(telikert_hutes_homerseklet);
  }
  lcd.print("R:");
  lcd.print(telikert_hutes_alarm_on);
  lcd.print("F:");
  lcd.print(telikert_hutes_futes);

  // lcd.setCursor(0, 3);
  // lcd.print("uhaz: ");
  // if (uhaz_last_on > uhaz_timeout)
  // {
  //   lcd.print("--");
  // }
  // else
  // {
  //   lcd.print(uhaz_homerseklet);
  // }
  // lcd.print(" R:");
  // lcd.print(uhaz_alarm_on);
  // lcd.print(" F:");
  // lcd.print(uhaz_futes);

  //increment last_on counters
  csir_last_on ++;
  uhaz_last_on ++;
  inkub_last_on ++;
  telikert_last_on ++;
  telikert_hutes_last_on ++;
}

void alarm(String message)
{
  tone(D8, 1800, 1000);
  USE_SERIAL.println("alarm: " + message);
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
  USE_SERIAL.print("[HTTP] check at " + url);
  if (http.begin(client, url))
  { // HTTP

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = http.getString();
        USE_SERIAL.println(payload);
        if (payload.indexOf("bin") > 0)
        {
          httpUpdateFunc(update_server + payload);
        }
      }
    }
    else
    {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
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

void discordPost(String message)
{
  String payload = "{\"content\": \"" + message + "\"}";
  String url = sec.discord_url;
  USE_SERIAL.println(POSTTask(url, payload));
}

String GETTask(String url)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure(); //This will set the http connection to insecure! This is not advised, but I have found no good way to use real SSL, and my application doesn't need the added security
  HTTPClient https;
  if (https.begin(*client, url))
  {
    USE_SERIAL.print(F("[HTTPS] GET "));
    USE_SERIAL.println(url);

    int httpCode = https.GET();

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTPS] GET... code: %d\n", httpCode);
      if (httpCode == 302)
      {
        String redirectUrl = https.getLocation();
        https.end();
        return redirectUrl;
      }
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        String payload = https.getString();
        USE_SERIAL.println(payload);
        https.end();

        return payload;
      }
    }
    else
    {
      USE_SERIAL.print(F("[HTTPS] GET... failed, error: "));
      USE_SERIAL.println(httpCode);
      https.end();
      return "";
    }

    https.end();
  }
  else
  {
    USE_SERIAL.println(F("[HTTPS] Unable to connect"));
    return "";
  }
  return "";
}

String POSTTask(String url, String payload)
{
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient https;
  if (https.begin(*client, url))
  {
    USE_SERIAL.print(F("[HTTPS] POST "));
    USE_SERIAL.print(url);
    USE_SERIAL.print(" --> ");
    USE_SERIAL.println(payload);
    https.addHeader(F("Content-Type"), F("application/json"));

    int httpCode = https.POST(payload);

    // httpCode will be negative on error
    if (httpCode > 0)
    {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.print(F("[HTTPS] POST... code: "));
      USE_SERIAL.println(httpCode);

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
      USE_SERIAL.print(F("[HTTPS] POST... failed, error: "));
      USE_SERIAL.println(httpCode);
      https.end();
      return "";
    }

    https.end();
  }
  else
  {
    USE_SERIAL.println(F("[HTTPS] Unable to connect"));
    return "";
  }
  return "";
}

////////////HTTP  ////////////////////////////
//////////////////////////////////////////////

//////////////////////////////////////////////
////////////GETDATA FROM INFLUXDB/////////////

// Helper function to query temperature from InfluxDB
bool queryTemperature(String deviceName, float &temperature, int &lastOn)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"" + deviceName + "\" and r._field == \"temp\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  USE_SERIAL.println("queryTemperature for " + deviceName);
  if (result.next())
  {
    temperature = result.getValueByName("_value").getDouble();
    lastOn = 0; // You may want to calculate the actual time difference
    USE_SERIAL.println(deviceName + " temperature = " + String(temperature));
    result.close();
    return true;
  }
  result.close();
  return false;
}

// Helper function to query heating status from InfluxDB
bool queryHeatingStatus(String deviceName, int &heatingStatus)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -1y) |> filter(fn: (r) => r.name == \"" + deviceName + "\" and r._field == \"event\" and (r._value == \"Heater start\" or r._value == \"Heater stop\")) |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next())
  {
    String event = result.getValueByName("_value").getString();
    heatingStatus = (event == "Heater start") ? 1 : 0;
    USE_SERIAL.println(deviceName + " heating = " + String(heatingStatus));
    result.close();
    return true;
  }
  result.close();
  return false;
}

// Helper function to get device data (temperature and heating status)
bool getDeviceData(String deviceName, float &temperature, int &lastOn, int &heatingStatus)
{
  if (!influx_client.validateConnection())
  {
    USE_SERIAL.print("InfluxDB connection failed: ");
    USE_SERIAL.println(influx_client.getLastErrorMessage());
    network_timeout++;
    return false;
  }

  queryTemperature(deviceName, temperature, lastOn);
  queryHeatingStatus(deviceName, heatingStatus);
  network_timeout = 0;
  return true;
}

void getdataUhaz()
{
  USE_SERIAL.println("Getting data for Uhaz from InfluxDB...");
  getDeviceData("noszlop_uveghaz", uhaz_homerseklet, uhaz_last_on, uhaz_futes);
}

void getdataInkub()
{
  USE_SERIAL.println("Getting data for Inkub from InfluxDB...");
  getDeviceData("noszlop_inkubator", inkub_homerseklet, inkub_last_on, inkub_futes);
}

void getdataCsir()
{
  USE_SERIAL.println("Getting data for Csir from InfluxDB...");
  getDeviceData("noszlop_csiraztato", csir_homerseklet, csir_last_on, csir_futes);
}

void getdataTelikert()
{
  USE_SERIAL.println("Getting data for Telikert from InfluxDB...");
  getDeviceData("noszlop_telikert", telikert_homerseklet, telikert_last_on, telikert_futes);
}

void getdataTelikertHutes()
{
  USE_SERIAL.println("Getting data for Telikert Hutes from InfluxDB...");
  getDeviceData("noszlop_telikert_hutes", telikert_hutes_homerseklet, telikert_hutes_last_on, telikert_hutes_futes);
}


//////////////////////////////////////////////
////////////GETCONFIG FROM INFLUXDB///////////

// Helper function to query config value (long/int)
bool queryConfigLong(String fieldName, long &value)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"" + fieldName + "\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next())
  {
    value = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got " + fieldName + " = " + String(value));
    result.close();
    return true;
  }
  result.close();
  return false;
}

// Helper function to query config value (double/float)
bool queryConfigDouble(String fieldName, float &value)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"" + fieldName + "\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next())
  {
    value = result.getValueByName("_value").getDouble();
    USE_SERIAL.println("Config got " + fieldName + " = " + String(value));
    result.close();
    return true;
  }
  result.close();
  return false;
}

// Helper function to query config value (boolean)
bool queryConfigBool(String fieldName, boolean &value)
{
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"" + fieldName + "\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  
  if (result.next())
  {
    value = result.getValueByName("_value").getBool();
    USE_SERIAL.println("Config got " + fieldName + " = " + String(value));
    result.close();
    return true;
  }
  result.close();
  return false;
}

void getconfig()
{
  /*
  The config data is retrieved from InfluxDB bucket 'noszlop'.
  We query for the latest config values using the device name as a tag filter.
  Config values are stored as separate fields in the 'config' measurement.
  */
  
  USE_SERIAL.println("Getting config from InfluxDB...");

  // Check InfluxDB connection
  if (!influx_client.validateConnection())
  {
    USE_SERIAL.print("InfluxDB connection failed: ");
    USE_SERIAL.println(influx_client.getLastErrorMessage());
    return;
  }
  
  // Query integer/long config values
  long pinginterval_temp = pinginterval;
  long update_interval_temp = update_interval;
  long csir_timeout_temp = csir_timeout;
  long uhaz_timeout_temp = uhaz_timeout;
  long inkub_timeout_temp = inkub_timeout;
  long telikert_timeout_temp = telikert_timeout;
  long telikert_hutes_timeout_temp = telikert_hutes_timeout;
  
  queryConfigLong("pinginterval", pinginterval_temp);
  queryConfigLong("update_interval", update_interval_temp);
  queryConfigLong("csir_timeout", csir_timeout_temp);
  queryConfigLong("uhaz_timeout", uhaz_timeout_temp);
  queryConfigLong("inkub_timeout", inkub_timeout_temp);
  queryConfigLong("telikert_timeout", telikert_timeout_temp);
  queryConfigLong("telikert_hutes_timeout", telikert_hutes_timeout_temp);
  
  pinginterval = (int)pinginterval_temp;
  update_interval = (int)update_interval_temp;
  csir_timeout = (int)csir_timeout_temp;
  uhaz_timeout = (int)uhaz_timeout_temp;
  inkub_timeout = (int)inkub_timeout_temp;
  telikert_timeout = (int)telikert_timeout_temp;
  telikert_hutes_timeout = (int)telikert_hutes_timeout_temp;
  
  // Query float/double config values
  queryConfigDouble("noszlop_uveghaz_alarm", noszlop_uveghaz_alarm);
  queryConfigDouble("noszlop_inkub_alarm", noszlop_inkub_alarm);
  queryConfigDouble("noszlop_telikert_alarm", noszlop_telikert_alarm);
  queryConfigDouble("noszlop_telikert_hutes_alarm", noszlop_telikert_hutes_alarm);

  // Query boolean config values
  queryConfigBool("csir_alarm_on", csir_alarm_on);
  queryConfigBool("uhaz_alarm_on", uhaz_alarm_on);
  queryConfigBool("inkub_alarm_on", inkub_alarm_on);
  queryConfigBool("telikert_alarm_on", telikert_alarm_on);
  queryConfigBool("telikert_hutes_alarm_on", telikert_hutes_alarm_on);

  USE_SERIAL.println("Config retrieval completed from InfluxDB");
}
