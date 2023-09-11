#include <Arduino.h>
#include <math.h>
#include <CircularBuffer.h>
#include "SPI.h"
#include <Adafruit_GFX.h>
#include "Adafruit_ILI9341.h"

// https://learn.adafruit.com/adafruit-2-8-and-3-2-color-tft-touchscreen-breakout-v2/spi-wiring-and-test
// https://speeduino.com/forum/viewtopic.php?f=14&t=2809
// https://wiki.speeduino.com/en/reference/Interface_Protocol
// https://github.com/paleppp/2SerialToRealdashCAN/blob/main/src/2SerialToRealdashCAN.ino
// https://github.com/pazi88/Speeduino-M5x-PCBs/blob/master/m52tu%2C%20m54%20PnP/Serial3toBMWcan/Serial3toBMWcan.ino
#define DISP_CLK 4
#define DISP_DIO 5
#define RESISTANCE_PIN 2
#define ALT_LIGHT_LED_PIN 2
#define ALT_LIGHT_READ_PIN 3
#define ALT_R1 4700.0
#define ALT_R2 1470.0
#define FUEL_REF_OHM 20
#define WINDOW_SIZE 32
#define VIN 5.0

#define LCD_CS 10
#define LCD_CD 9
#define LCD_MOSI 11
#define LCD_MISO 12
#define LCD_RESET 8
#define LCD_CLK 13
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF


CircularBuffer<double,WINDOW_SIZE> fuelWindow;
// Adafruit_ILI9341 tft = Adafruit_ILI9341(LCD_CS, LCD_CD, -1);

Adafruit_ILI9341 tft = Adafruit_ILI9341(LCD_CS, LCD_CD, LCD_MOSI, LCD_CLK, LCD_RESET, LCD_MISO);
// Adafruit_ILI9341 tft = Adafruit_ILI9341(LCD_CS, TFT_DC);

void setup() {
  // Serial.begin(115200);
  // Serial.println("ILI9341 Test!");

  tft.begin();
  tft.fillScreen(BLACK);

  // Serial.println(F("Done!"));
}

#define BAR_HEIGHT 10
#define BACKGROUND_COLOR ILI9341_BLACK


void loop(void) {
  // Serial.println("LOoping");
  tft.setTextSize(3);
  tft.setRotation(3);

  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE, BACKGROUND_COLOR);
  tft.print("FUEL     ");
  int fuel = readFuel();
  tft.println(fuel);
  showGauge(fuel, 0, 100, ILI9341_WHITE);

  tft.setTextColor(ILI9341_WHITE, BACKGROUND_COLOR);
  tft.print("RPM      ");
  int rpm = readRPM();
  tft.println(rpm);
  showGauge(rpm, 500, 7000, ILI9341_WHITE);

  tft.setTextColor(ILI9341_YELLOW, BACKGROUND_COLOR);
  tft.print("COOLANT ");
  int coolant = readCoolant();
  tft.println(coolant);
  showGauge(coolant, 50, 250, ILI9341_YELLOW);



  tft.setTextColor(ILI9341_YELLOW, BACKGROUND_COLOR);
  tft.print("BAT     ");
  float volts = readBat();
  tft.println(volts);
  showGauge((int)(volts * 100), 100, 150, ILI9341_YELLOW);



  tft.setTextSize(2);

  tft.setTextColor(ILI9341_RED, BACKGROUND_COLOR);
  tft.print("A/F     ");
  float af = readAF();
  tft.println(af);


  tft.setTextColor(ILI9341_LIGHTGREY, BACKGROUND_COLOR);
  tft.print("TIMING  ");
  tft.println(readTiming());

  tft.setTextColor(ILI9341_CYAN, BACKGROUND_COLOR);
  tft.print("IAT     ");
  tft.println(readIAT());

}


void showGauge(int value, int min, int max, int color) {
  int width = map(value, min, max, 0, tft.width());
  tft.fillRect(0, tft.getCursorY(), tft.width(), BAR_HEIGHT, BACKGROUND_COLOR);
  tft.fillRect(
    0, tft.getCursorY(),
    width, BAR_HEIGHT,
    color
  );
  tft.println();
}

int readFuel() {
  return random(0, 100);
}


int readRPM() {
  return random(2000, 6000);
}

float readBat() {
  return random(12, 14);
}

float readCoolant() {
  return random(180, 200);
}

float readAF() {
  return random(13.0, 15.0);
}

float readTiming() {
  return random(5, 15);
}

int readIAT() {
  return random(50, 80);
}


// void readFuel() {
//   double vout = (double)((analogRead(RESISTANCE_PIN) * VIN) / 1024.0);
//   double ohms = FUEL_REF_OHM * (vout / (VIN - vout));
//   fuelWindow.push(ohms);
//   double avgOhms = avg(fuelWindow);

//   double gallons = 29.0207 + (-7.0567 * log(avgOhms));
//   int pct = floor((gallons / 14) * 100);
//   if (pct < 0) {
//     pct = 0;
//   }

// }


// double avg(CircularBuffer<double,WINDOW_SIZE> &cb) {
//   if (cb.size() == 0) return 0;
//   double total = 0;
//   for (int i = 0; i <= cb.size(); i++) {
//     total += cb[i];
//   }
//   return total / cb.size();
// }
