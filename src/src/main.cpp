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


const int LOAD_CELL_CLK = D1;
const int LOAD_CELL_DATA = D0;

WiFiMulti wifiMulti;
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE); // High speed I2C
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
#define AM_INTERVAL 200
#define PRESSURE_INTERVAL 2000
unsigned long last_sent_time = 0;
unsigned long last_am_time = 0;
unsigned long last_pressure_time = 0;

void setup()
{
  Serial.begin(115200);
  delay(5000);
  Wire.begin();

  display.begin();

  display.clearBuffer();
  display.setFont(u8g2_font_ncenB12_tr);
  display.setCursor(0, 30);
  display.print("starting...");
  display.sendBuffer();

  if (!pressure_sensor.begin(&Wire))
  {
    Serial.println("ERROR: Could not initialize sensor!");
    Serial.println("Check I2C wiring and connections.");
  }
  load_cell.begin(LOAD_CELL_CLK, LOAD_CELL_DATA);

  Serial.println("TARE !!");
  delay(3000);

  float calibration_factor = 23.83382;
  load_cell.set_scale(calibration_factor);

  display.clearBuffer();
  display.setFont(u8g2_font_ncenB12_tr);
  display.setCursor(0, 30);
  display.print("tare...");
  display.sendBuffer();
  delay(2000);

  load_cell.tare(); // Met à zéro la valeur actuelle
  Serial.println("tare ok");

  am2320.begin();
  pressure_sensor.measureStart(pressure_sensor.VERY_ACCURATE);

  display.clearBuffer();
  display.setFont(u8g2_font_ncenB12_tr);
  display.setCursor(0, 30);
  display.print("connection wifi...");
  display.sendBuffer();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion au Wi-Fi");

  unsigned long startAttemptTime = millis();
  while (wifiMulti.run() != WL_CONNECTED && millis() - startAttemptTime < 20000)
  {
    Serial.print(".");
    delay(500);
  }

  if (wifiMulti.run() != WL_CONNECTED)
  {
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB12_tr);
    display.setCursor(0, 30);
    display.print("pas de wifi");
    display.sendBuffer();

    delay(5000);
  }
  else
  {
    connected_wifi = true;

    display.clearBuffer();
    display.setFont(u8g2_font_ncenB12_tr);
    display.setCursor(0, 30);
    display.print("connecte");
    display.sendBuffer();

    delay(3000);

    display.clearBuffer();
    display.setFont(u8g2_font_ncenB12_tr);
    display.setCursor(0, 30);
    display.print("setup...");
    display.sendBuffer();

    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
    client.setInsecure();

    if (client.validateConnection())
    {
      Serial.println(client.getServerUrl());
    }
    else
    {
      display.clearBuffer();
      display.setFont(u8g2_font_ncenB12_tr);
      display.setCursor(0, 30);
      display.print("erreur serveur");
      display.sendBuffer();

      Serial.println(client.getLastErrorMessage());

      connected_wifi = false;
      delay(5000);
    }

    sensorData.addTag("device", "XIAO");
    sensorData.addTag("lieu", "plage");
  }
}

void loop()
{
  if (wifiMulti.run() != WL_CONNECTED)
  {
    display.clearBuffer();
    display.setFont(u8g2_font_ncenB12_tr);
    display.setCursor(0, 30);
    display.print("wifi perdu");
    display.sendBuffer();
    connected_wifi = false;
    delay(1000);
    return;
  }

  if (millis() - last_am_time >= AM_INTERVAL)
  {
    last_am_time = millis();
    current_temp = am2320.readTemperature();
    current_hum = am2320.readHumidity();
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
      current_force = load_cell.get_units(5) / 1000;
      if (current_force < 0)
      {
        current_force = 0;
      }
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
        display.clearBuffer();
        display.setFont(u8g2_font_ncenB12_tr);
        display.setCursor(0, 30);
        display.print("erreur envoie");
        display.sendBuffer();

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

  delay(100);
}
