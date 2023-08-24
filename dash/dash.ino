// https://arxiv.org/pdf/1901.03811.pdf
#include <Arduino.h>
#include <math.h>
#include <TM1637Display.h>
#include <CircularBuffer.h>

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


TM1637Display display(DISP_CLK, DISP_DIO);
const uint8_t SEG_HI[] = {
  SEG_F | SEG_B | SEG_E | SEG_C | SEG_G,
  SEG_B | SEG_C,
  0x00,
  0x00
};


void setup() {
  display.setBrightness(0x0f);

  // initialize digital pin LED_BUILTIN as an output.
  pinMode(ALT_LIGHT_LED_PIN, OUTPUT);

  display.setSegments(SEG_HI);
  delay(2000);
}

// the loop function runs over and over again forever
CircularBuffer<double,WINDOW_SIZE> fuelWindow;
CircularBuffer<double,WINDOW_SIZE> altVoltageWindow;

void loop() {
  readAltLight();
  readFuel();
  delay(20);
}


void readAltLight() {
  double vout = (analogRead(ALT_LIGHT_READ_PIN) * VIN) / 1024.0;
  double volts = vout / (ALT_R2 / (ALT_R1 + ALT_R2));
  altVoltageWindow.push(volts);
  double avgVolts = avg(altVoltageWindow);

  if (avgVolts < 11) {
    digitalWrite(ALT_LIGHT_LED_PIN, LOW);
  } else {
    digitalWrite(ALT_LIGHT_LED_PIN, HIGH);
  }
}


void readFuel() {
  double vout = (double)((analogRead(RESISTANCE_PIN) * VIN) / 1024.0);
  double ohms = FUEL_REF_OHM * (vout / (VIN - vout));
  fuelWindow.push(ohms);
  double avgOhms = avg(fuelWindow);

  double gallons = 29.0207 + (-7.0567 * log(avgOhms));
  int pct = floor((gallons / 14) * 100);
  if (pct < 0) {
    pct = 0;
  }

  display.showNumberDec(floor(pct));
}


double avg(CircularBuffer<double,WINDOW_SIZE> &cb) {
  if (cb.size() == 0) return 0;
  double total = 0;
  for (int i = 0; i <= cb.size(); i++) {
    total += cb[i];
  }
  return total / cb.size();
}
