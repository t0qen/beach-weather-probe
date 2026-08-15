#include <Wire.h>
#include <ue_i2c_icp_10111_sen.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"
#include "HX711.h"
#include <U8g2lib.h>

const int LOAD_CELL_CLK = D1;
const int LOAD_CELL_DATA = D0;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, /* clock=*/SCL, /* data=*/SDA, /* reset=*/U8X8_PIN_NONE); // High speed I2C
HX711 load_cell;
ICP101xx pressure_sensor;
Adafruit_AM2320 am2320 = Adafruit_AM2320();

float current_temp = 0;
int current_hum = 0;
int current_pressure = 0;
float current_force = 0;

void setup()
{
  Serial.begin(115200);
  delay(5000);
  Wire.begin();

  display.begin();

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
  load_cell.tare(); // Met à zéro la valeur actuelle
  Serial.println("tare ok");



  am2320.begin();

  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  pressure_sensor.measureStart(pressure_sensor.VERY_ACCURATE);
}

void loop()
{

  current_temp = am2320.readTemperature();
  current_hum = am2320.readHumidity();
  if (pressure_sensor.dataReady())
  {
    current_pressure = int(pressure_sensor.getPressurePa() / 100);
    pressure_sensor.measureStart(pressure_sensor.VERY_ACCURATE);
  }

  if (load_cell.is_ready())
  {
    Serial.println("load cell");
    current_force = load_cell.get_units(5) / 1000;
    Serial.println(current_force);
    if (current_force < 0) {
      current_force = 0;
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
  

  delay(500);
}
