#include <Wire.h>
#include <ue_i2c_icp_10111_sen.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"
#include "HX711.h"
#include <U8g2lib.h>
#include <WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#include "secrets.h"

const int LOAD_CELL_CLK = D3;
const int LOAD_CELL_DATA = D2;

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE);
HX711 load_cell;
ICP101xx pressure_sensor;
Adafruit_AM2320 am2320 = Adafruit_AM2320();
Point sensorData("beach_weather_probe");

float current_temp = 0.00;
int current_hum = 0.00;
int current_pressure = 0.00;
float current_force = 0.00;
bool connected_wifi = false;

#define SEND_INTERVAL 500
#define AM_INTERVAL 1000
#define PRESSURE_INTERVAL 2000
unsigned long last_sent_time = 0;
unsigned long last_am_time = 0;
unsigned long last_pressure_time = 0;

void setup()
{
  Serial.begin(115200);
  delay(3000);


  pinMode(D4, OUTPUT_OPEN_DRAIN); 
  pinMode(D5, OUTPUT_OPEN_DRAIN); 
  digitalWrite(D4, HIGH);
  digitalWrite(D5, HIGH);
  delayMicroseconds(10);
  for (int i = 0; i < 9; i++) {
    digitalWrite(D5, LOW); delayMicroseconds(10);
    digitalWrite(D5, HIGH); delayMicroseconds(10);
  }
  digitalWrite(D4, LOW); delayMicroseconds(10);
  digitalWrite(D5, HIGH); delayMicroseconds(10);
  digitalWrite(D4, HIGH); delayMicroseconds(10);


  Wire.begin();

  display.begin();

  display.clearBuffer();
  display.setFont(u8g2_font_ncenB12_tr);
  display.setCursor(0, 30);
  display.sendBuffer();

  if (!pressure_sensor.begin(&Wire))
  {
      display.clearBuffer();
      display.setCursor(0, 30);
      display.print("press. error");
      display.sendBuffer();
      delay(500);
  }
  pressure_sensor.measureStart(pressure_sensor.VERY_ACCURATE);


  load_cell.begin(LOAD_CELL_DATA, LOAD_CELL_CLK);
  float calibration_factor = 23.83382;
  load_cell.set_scale(calibration_factor);

  display.clearBuffer();
  display.setCursor(0, 30);
  display.print("tare...");
  display.sendBuffer();
  
  unsigned long tare_timeout = millis();
  while (!load_cell.is_ready() && millis() - tare_timeout < 3000) {
    delay(10);
  }
  
  if (load_cell.is_ready()) {
    load_cell.tare(); 
  }

  am2320.begin();

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
  while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < 20000)
  {
    delay(500);
    Serial.print(".");
  }

  if (wifiMulti.run() != WL_CONNECTED)
  {
    display.clearBuffer();
    display.setCursor(0, 30);
    display.print("wifi failed");
    display.sendBuffer();
    delay(2000);
  }
  else
  {
    connected_wifi = true;
    display.clearBuffer();
    display.setCursor(0, 30);
    display.print("connected");
    display.setCursor(0, 50);
    display.print(WiFi.SSID());
    display.sendBuffer();
    delay(2000);

    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    client.setInsecure();

    if (client.validateConnection())
    {
        display.clearBuffer();
        display.setCursor(0, 30);
        display.print("server ok");
        display.sendBuffer();
    }
    else
    {
      Serial.println(client.getLastErrorMessage());
      display.clearBuffer();
      display.setCursor(0, 30);
      display.print("server error");
      display.sendBuffer();
      connected_wifi = false;
      delay(2000);
    }

    sensorData.addTag("device", "XIAO");
    sensorData.addTag("lieu", "plage");
  }
}

void loop()
{
  if (wifiMulti.run() != WL_CONNECTED)
  {
    connected_wifi = false;
  } else {
    connected_wifi = true;
  }

  if (millis() - last_am_time >= AM_INTERVAL)
  {
    last_am_time = millis();
    current_temp = am2320.readTemperature();
    current_hum = am2320.readHumidity();
    if (isnan(current_temp)) current_temp = 0.0;
    if (isnan(current_hum)) current_hum = 0;
  }

  if (pressure_sensor.dataReady())
  {
    if (millis() - last_pressure_time >= PRESSURE_INTERVAL)
    {
      last_pressure_time = millis();
      current_pressure = int(pressure_sensor.getPressurePa() / 100);
      pressure_sensor.measureStart(pressure_sensor.VERY_ACCURATE);
    }
  }

  if (load_cell.is_ready())
  {
    float raw_force = load_cell.get_units(5) / 1000;
    if (!isnan(raw_force)) {
      current_force = raw_force;
      if (current_force < 0) current_force = 0;
    }
  }

  if (connected_wifi)
  {
    if (millis() - last_sent_time >= SEND_INTERVAL)
    {
      last_sent_time = millis();

      sensorData.clearFields();
      sensorData.addField("temperature", current_temp);
      sensorData.addField("humidite", current_hum);
      sensorData.addField("pression", current_pressure);
      sensorData.addField("wind_force", current_force);

      Serial.println(sensorData.toLineProtocol());

      if (!client.writePoint(sensorData))
      {
        Serial.print("Envoi erreur: ");
        Serial.println(client.getLastErrorMessage());
      }
    }
  }

  display.clearBuffer();
  display.setFont(u8g2_font_ncenB24_tr);
  display.setCursor(0, 26);
  display.print(current_force);

  display.setFont(u8g2_font_ncenB12_tr);
  display.print(" kg");

  display.setCursor(0, 44);
  display.setFont(u8g2_font_ncenB12_tr);
  display.print(current_pressure);
  display.setFont(u8g2_font_ncenB08_tr);
  display.print(" hPa");
  display.setCursor(0, 58);
  display.setFont(u8g2_font_ncenB12_tr);
  display.print(current_temp);
  display.setFont(u8g2_font_ncenB08_tr);
  display.print(" *C");
  display.setCursor(74, 58);
  display.setFont(u8g2_font_ncenB12_tr);
  display.print(current_hum);
  display.setFont(u8g2_font_ncenB08_tr);
  display.print(" %");

  display.sendBuffer();

  delay(50);
}