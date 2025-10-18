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
static String ver = "2_1"; // InfluxDB integration

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

  // Connect to WiFi
  WiFiManager wifiManager;
  wifiManager.setTimeout(180);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect("mocsigoncska_kijelzo_ap");
  USE_SERIAL.println("connected...yeey :)");
  delay(1000);

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
  
  getdataUhazInkub();
  delay(1500);
  getdataCsir();
  delay(1500);
  getdataTelikert();
  delay(1500);
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
    delay(10000); //wait for bootup of the server
    httpCode = http.GET();
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

void getdataUhazInkub()
{
  USE_SERIAL.println("Getting data for Uhaz and Inkub from InfluxDB...");

  // Check InfluxDB connection
  if (!influx_client.validateConnection())
  {
    USE_SERIAL.print("InfluxDB connection failed: ");
    USE_SERIAL.println(influx_client.getLastErrorMessage());
    network_timeout++;
    return;
  }

  // Query for uhaz_homerseklet (from uhaz device)
  String query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"uhaz\" and r._field == \"temp\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  if (result.next())
  {
    uhaz_homerseklet = result.getValueByName("_value").getDouble();
    // Calculate time difference in seconds
    String timeStr = result.getValueByName("_time").getString();
    uhaz_last_on = 0; // You may want to calculate the actual time difference
    USE_SERIAL.println("uhaz_homerseklet = " + String(uhaz_homerseklet));
  }
  result.close();

  // Query for uhaz_futes (heating status)
  query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"uhaz\" and r._field == \"event\" and (r._value == \"Heater start\" or r._value == \"Heater stop\")) |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    String event = result.getValueByName("_value").getString();
    uhaz_futes = (event == "Heater start") ? 1 : 0;
    USE_SERIAL.println("uhaz_futes = " + String(uhaz_futes));
  }
  result.close();

  // Query for inkub (telikert_hutes device)
  query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"noszlop_telikert_hutes\" and r._field == \"temp\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    inkub_homerseklet = result.getValueByName("_value").getDouble();
    inkub_last_on = 0;
    USE_SERIAL.println("inkub_homerseklet = " + String(inkub_homerseklet));
  }
  result.close();

  // Query for inkub_futes
  query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"noszlop_telikert_hutes\" and r._field == \"event\" and (r._value == \"Heater start\" or r._value == \"Heater stop\")) |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    String event = result.getValueByName("_value").getString();
    inkub_futes = (event == "Heater start") ? 1 : 0;
    USE_SERIAL.println("inkub_futes = " + String(inkub_futes));
  }
  result.close();

  network_timeout = 0;
}

void getdataCsir()
{
  USE_SERIAL.println("Getting data for Csir from InfluxDB...");

  if (!influx_client.validateConnection())
  {
    USE_SERIAL.print("InfluxDB connection failed: ");
    USE_SERIAL.println(influx_client.getLastErrorMessage());
    network_timeout++;
    return;
  }

  // Query for csir_homerseklet
  String query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"csir\" and r._field == \"temp\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  if (result.next())
  {
    csir_homerseklet = result.getValueByName("_value").getDouble();
    csir_last_on = 0;
    USE_SERIAL.println("csir_homerseklet = " + String(csir_homerseklet));
  }
  result.close();

  // Query for csir_futes
  query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"csir\" and r._field == \"event\" and (r._value == \"Heater start\" or r._value == \"Heater stop\")) |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    String event = result.getValueByName("_value").getString();
    csir_futes = (event == "Heater start") ? 1 : 0;
    USE_SERIAL.println("csir_futes = " + String(csir_futes));
  }
  result.close();

  network_timeout = 0;
}

void getdataTelikert()
{
  USE_SERIAL.println("Getting data for Telikert from InfluxDB...");

  if (!influx_client.validateConnection())
  {
    USE_SERIAL.print("InfluxDB connection failed: ");
    USE_SERIAL.println(influx_client.getLastErrorMessage());
    network_timeout++;
    return;
  }

  // Query for telikert_homerseklet
  String query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"telikert\" and r._field == \"temp\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  if (result.next())
  {
    telikert_homerseklet = result.getValueByName("_value").getDouble();
    telikert_last_on = 0;
    USE_SERIAL.println("telikert_homerseklet = " + String(telikert_homerseklet));
  }
  result.close();

  // Query for telikert_futes
  query = "from(bucket: \"noszlop\") |> range(start: -1h) |> filter(fn: (r) => r.name == \"telikert\" and r._field == \"event\" and (r._value == \"Heater start\" or r._value == \"Heater stop\")) |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    String event = result.getValueByName("_value").getString();
    telikert_futes = (event == "Heater start") ? 1 : 0;
    USE_SERIAL.println("telikert_futes = " + String(telikert_futes));
  }
  result.close();

  network_timeout = 0;
}

//////////////////////////////////////////////
////////////GETCONFIG FROM INFLUXDB///////////

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
  
  // Query for pinginterval
  String query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"pinginterval\") |> last()";
  FluxQueryResult result = influx_client.query(query);
  if (result.next())
  {
    pinginterval = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got pinginterval = " + String(pinginterval));
  }
  result.close();
  
  // Query for update_interval
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"update_interval\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    update_interval = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got update_interval = " + String(update_interval));
  }
  result.close();
  
  // Query for csir_timeout
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"csir_timeout\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    csir_timeout = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got csir_timeout = " + String(csir_timeout));
  }
  result.close();
  
  // Query for uhaz_timeout
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"uhaz_timeout\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    uhaz_timeout = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got uhaz_timeout = " + String(uhaz_timeout));
  }
  result.close();
  
  // Query for inkub_timeout
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"inkub_timeout\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    inkub_timeout = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got inkub_timeout = " + String(inkub_timeout));
  }
  result.close();
  
  // Query for telikert_timeout
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"telikert_timeout\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    telikert_timeout = result.getValueByName("_value").getLong();
    USE_SERIAL.println("Config got telikert_timeout = " + String(telikert_timeout));
  }
  result.close();
  
  // Query for noszlop_uveghaz_alarm
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"noszlop_uveghaz_alarm\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    noszlop_uveghaz_alarm = result.getValueByName("_value").getDouble();
    USE_SERIAL.println("Config got noszlop_uveghaz_alarm = " + String(noszlop_uveghaz_alarm));
  }
  result.close();
  
  // Query for noszlop_inkub_alarm
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"noszlop_inkub_alarm\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    noszlop_inkub_alarm = result.getValueByName("_value").getDouble();
    USE_SERIAL.println("Config got noszlop_inkub_alarm = " + String(noszlop_inkub_alarm));
  }
  result.close();
  
  // Query for noszlop_telikert_alarm
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"noszlop_telikert_alarm\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    noszlop_telikert_alarm = result.getValueByName("_value").getDouble();
    USE_SERIAL.println("Config got noszlop_telikert_alarm = " + String(noszlop_telikert_alarm));
  }
  result.close();

  // Query for csir_alarm_on
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"csir_alarm_on\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    csir_alarm_on = result.getValueByName("_value").getBool();
    USE_SERIAL.println("Config got csir_alarm_on = " + String(csir_alarm_on));
  }
  result.close();

  // Query for uhaz_alarm_on
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"uhaz_alarm_on\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    uhaz_alarm_on = result.getValueByName("_value").getBool();
    USE_SERIAL.println("Config got uhaz_alarm_on = " + String(uhaz_alarm_on));
  }
  result.close();

  // Query for inkub_alarm_on
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"inkub_alarm_on\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    inkub_alarm_on = result.getValueByName("_value").getBool();
    USE_SERIAL.println("Config got inkub_alarm_on = " + String(inkub_alarm_on));
  }
  result.close();

  // Query for telikert_alarm_on
  query = "from(bucket: \"noszlop\") |> range(start: -10y) |> filter(fn: (r) => r._measurement == \"config\" and r.name == \"" + name + "\" and r._field == \"telikert_alarm_on\") |> last()";
  result = influx_client.query(query);
  if (result.next())
  {
    telikert_alarm_on = result.getValueByName("_value").getBool();
    USE_SERIAL.println("Config got telikert_alarm_on = " + String(telikert_alarm_on));
  }
  result.close();

  USE_SERIAL.println("Config retrieval completed from InfluxDB");
}
