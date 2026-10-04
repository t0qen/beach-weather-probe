#include <Wire.h>
#include "HX711.h"
#include <U8g2lib.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "secrets.h"

const int LOAD_CELL_CLK = D2;
const int LOAD_CELL_DATA = D3;

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0);
HX711 load_cell;
Point sensorData("beach_weather_probe");

float current_force = 0.00;
bool wifi_ok = false;
bool server_ok = false;
bool time_synced = false;

#define SEND_INTERVAL 500
#define SERVER_RETRY_INTERVAL 60000UL
unsigned long last_sent_time = 0;
unsigned long last_server_retry = 0;


void setup()
{
  Serial.begin(115200);
  delay(3000);

  sensorData.addTag("device", "XIAO");
  sensorData.addTag("lieu", "plage");
  client.setInsecure();

  Wire.begin();

  display.begin();

  load_cell.begin(LOAD_CELL_DATA, LOAD_CELL_CLK);
  //load_cell.set_offset(-260669);
  load_cell.set_scale(61.574112);

  display.clearBuffer();
  display.setFont(u8g2_font_ncenB12_tr);
  display.setCursor(0, 30);
  display.print("tare...");
  display.sendBuffer();

  if (load_cell.wait_ready_timeout(3000)) {
    load_cell.tare();
  } else {
    display.clearBuffer();
    display.setCursor(0, 30);
    display.print("load cell not ok");
    display.sendBuffer();
    delay(1000);
  }

  display.clearBuffer();
  display.setCursor(0, 30);
  display.print("connecting...");
  display.sendBuffer();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID_1, WIFI_PASSWORD_1);
  wifiMulti.addAP(WIFI_SSID_2, WIFI_PASSWORD_2);
  wifiMulti.addAP(WIFI_SSID_3, WIFI_PASSWORD_3);
  wifiMulti.addAP(WIFI_SSID_4, WIFI_PASSWORD_4);
  wifiMulti.addAP(WIFI_SSID_5, WIFI_PASSWORD_5);

  unsigned long startAttemptTime = millis();
  while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    display.clearBuffer();
    display.setCursor(0, 30);
    display.print("wifi failed");
    display.sendBuffer();
    delay(2000);
  } else {
    display.clearBuffer();
    display.setCursor(0, 30);
    display.print("connected");
    display.setCursor(0, 50);
    display.print(WiFi.SSID());
    display.sendBuffer();
    delay(2000);

    if (client.validateConnection()) {
      display.clearBuffer();
      display.setCursor(0, 30);
      display.print("server ok");
      display.sendBuffer();
      server_ok = true;
    } else {
      Serial.println(client.getLastErrorMessage());
      display.clearBuffer();
      display.setCursor(0, 30);
      display.print("server error");
      display.sendBuffer();
      delay(2000);
    }
  }
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED) {
    wifiMulti.run();
  }
  wifi_ok = (WiFi.status() == WL_CONNECTED);

  if (wifi_ok && !time_synced) {
    timeSync(TZ_INFO, "pool.ntp.org", "time.nist.gov");
    time_synced = true;
  }

  if (wifi_ok && !server_ok && millis() - last_server_retry >= SERVER_RETRY_INTERVAL) {
    last_server_retry = millis();
    server_ok = client.validateConnection();
    if (!server_ok) Serial.println(client.getLastErrorMessage());
  }

  if (load_cell.is_ready()) {
    float sample = load_cell.get_units(1) / 1000.0f;  // kg
    current_force = 0.7f * sample + 0.3f * current_force;
    if (current_force < 0) current_force = 0;
  }

  if (wifi_ok && server_ok) {
    if (millis() - last_sent_time >= SEND_INTERVAL) {
      last_sent_time = millis();

      sensorData.clearFields();
      sensorData.addField("wind_force", current_force);

      Serial.println(sensorData.toLineProtocol());

      if (!client.writePoint(sensorData)) {
        Serial.print("Envoi erreur: ");
        Serial.println(client.getLastErrorMessage());
      }
    }
  }

  display.clearBuffer();
  display.setFont(u8g2_font_inb33_mn);
  display.setCursor(20, 50);
  display.print(current_force);
  display.sendBuffer();

  delay(50);
}