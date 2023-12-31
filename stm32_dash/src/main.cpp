/*
  dance to update firmware:
    * set pin 3 to ON
    * turn car power ON
    * plug in USB
    * push button on stm32
    * upload
  wtf:
    * pin3 has to be ON to program the thing
      * press reset each time to program
    * pin3 has to be OFF to run program
    * need delay on powerup or else it doesn't work at all
   * println is over Serial1. The following works:
      void setup() {
        while (!Serial1);
        Serial1.begin(115200);
      }

      void loop() {
        // Serial1.println("Working");
        delay(500);
      }

*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <CircularBuffer.h>

#define WIDTH 320
#define HEIGHT 240

#define FUEL_VIN 3.3
#define RESISTANCE_PIN PB0
#define FUEL_REF_OHM 47
#define WINDOW_SIZE 16

#define BAR_HEIGHT 8
#define WUT ILI9341_CYAN

// 16 bit TFT, 5 bits red, 6 green, 5 blue
#define BACKGROUND_COLOR ILI9341_BLACK
#define BAR_COLOR ILI9341_CYAN
uint8_t timeouts = 0;
bool requestFrame = false;

#define SCREEN_STATE_NO_DATA 1
#define SCREEN_STATE_NO_CONNECTION 2
#define SCREEN_STATE_NORMAL 0
uint8_t screenState = SCREEN_STATE_NO_CONNECTION;
uint8_t lastScreenState = SCREEN_STATE_NO_CONNECTION;

#define LIMIT_COOLANT_UPPER 215
#define LIMIT_OIL_LOWER 10
#define LIMIT_FUEL_LOWER 10
#define LIMIT_RPM_UPPER 5500

struct Colors
{
  int bar;
  int background;
  int text;
} okColors, errorColors;

void bumpTimeout()
{
  timeouts++;
  requestFrame = true;
  screenState = SCREEN_STATE_NO_DATA;
}
void resetTimeout()
{
  timeouts = 0;
  requestFrame = true;
}

bool isNthBitSet(unsigned char c, int n)
{
  // static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
  static unsigned char mask[] = {1, 2, 4, 8, 16, 32, 64, 128};

  return ((c & mask[n]) != 0);
}
// Define bit positions within engine variable
#define BIT_ENGINE_RUN 0    // Engine running
#define BIT_ENGINE_CRANK 1  // Engine cranking
#define BIT_ENGINE_ASE 2    // after start enrichment (ASE)
#define BIT_ENGINE_WARMUP 3 // Engine in warmup
#define BIT_ENGINE_ACC 4    // in acceleration mode (TPS accel)
#define BIT_ENGINE_DCC 5    // in deceleration mode
#define BIT_ENGINE_MAPACC 6 // MAP acceleration mode
#define BIT_ENGINE_MAPDCC 7 // MAP deceleration mode

#define BIT_STATUS4_WMI_EMPTY 0  // Indicates whether the WMI tank is empty
#define BIT_STATUS4_VVT1_ERROR 1 // VVT1 cam angle within limits or not
#define BIT_STATUS4_VVT2_ERROR 2 // VVT2 cam angle within limits or not
#define BIT_STATUS4_FAN 3        // Fan Status
#define BIT_STATUS4_BURNPENDING 4
#define BIT_STATUS4_STAGING_ACTIVE 5
#define BIT_STATUS4_COMMS_COMPAT 6
#define BIT_STATUS4_ALLOW_LEGACY_COMMS 7

struct SpeeduinoStatus
{
  uint8_t secl;
  uint8_t status1;
  byte engine;
  uint8_t syncLossCounter;
  uint16_t MAP;
  uint8_t IAT;
  uint8_t coolant;
  uint8_t batCorrection;
  uint8_t battery10;
  uint8_t O2;
  uint8_t egoCorrection;
  uint8_t iatCorrection;
  uint8_t wueCorrection;
  uint16_t RPM;
  uint8_t TAEamount;
  uint8_t corrections;
  uint8_t ve;
  uint8_t afrTarget;
  uint16_t PW1;
  uint8_t tpsDOT;
  uint8_t advance;
  uint8_t TPS;
  uint16_t loopsPerSecond;
  uint16_t freeRAM;
  uint8_t boostTarget;
  uint8_t boostDuty;
  uint8_t spark;
  uint16_t rpmDOT;
  uint8_t ethanolPct;
  uint8_t flexCorrection;
  uint8_t flexIgnCorrection;
  uint8_t idleLoad;
  uint8_t testOutputs;
  uint8_t O2_2;
  uint8_t baro;
  uint16_t CANin_1;
  uint16_t CANin_2;
  uint16_t CANin_3;
  uint16_t CANin_4;
  uint16_t CANin_5;
  uint16_t CANin_6;
  uint16_t CANin_7;
  uint16_t CANin_8;
  uint16_t CANin_9;
  uint16_t CANin_10;
  uint16_t CANin_11;
  uint16_t CANin_12;
  uint16_t CANin_13;
  uint16_t CANin_14;
  uint16_t CANin_15;
  uint16_t CANin_16;
  uint8_t tpsADC;
  uint8_t getNextError;
  uint8_t launchCorrection;
  uint16_t PW2;
  uint16_t PW3;
  uint16_t PW4;
  uint8_t status3;
  uint8_t engineProtectStatus;
  uint8_t fuelLoad;
  uint8_t ignLoad;
  uint8_t injAngle;
  uint8_t idleDuty;
  uint8_t CLIdleTarget;
  uint8_t mapDOT;
  uint8_t vvt1Angle;
  uint8_t vvt1TargetAngle;
  uint8_t vvt1Duty;
  uint8_t flexBoostCorrection;
  uint8_t baroCorrection;
  uint8_t ASEValue;
  uint8_t vss;
  uint8_t gear;
  uint8_t fuelPressure;
  uint8_t oilPressure;
  uint8_t wmiPW;
  uint8_t status4;
  uint8_t vvt2Angle;
  uint8_t vvt2TargetAngle;
  uint8_t vvt2Duty;
  uint8_t outputsStatus;
  uint8_t fuelTemp;
  uint8_t fuelTempCorrection;
  uint8_t VE1;
  uint8_t VE2;
  uint8_t advance1;
  uint8_t advance2;
  uint8_t nitrous_status;
  uint8_t TS_SD_Status;
  uint8_t fanDuty;
};
SpeeduinoStatus currentStatus;

TFT_eSPI tft = TFT_eSPI();

double avg(CircularBuffer<double, WINDOW_SIZE> &cb)
{
  if (cb.size() == 0)
    return 0;
  double total = 0;
  for (int i = 0; i <= cb.size(); i++)
  {
    total += cb[i];
  }
  return total / cb.size();
}

void clearLine() {
  tft.fillRect(tft.getCursorX(), tft.getCursorY(), tft.width(), tft.getCursorY() + tft.textsize, BACKGROUND_COLOR);
}

void showGauge(int value, int min, int max, int color)
{
  int width = map(value, min, max, 0, tft.width());

  tft.fillRect(0, tft.getCursorY(), tft.width(), BAR_HEIGHT, BACKGROUND_COLOR);

  // WIDTH / (max - min)
  tft.fillRect(
      0, tft.getCursorY(),
      width, BAR_HEIGHT,
      color);
  tft.println();
}

long i = 0;
CircularBuffer<double, WINDOW_SIZE> fuelWindow;
int readFuel()
{
  double vout = (double)((analogRead(RESISTANCE_PIN) * FUEL_VIN) / 1024.0);
  double ohms = FUEL_REF_OHM * (vout / (FUEL_VIN - vout));
  fuelWindow.push(ohms);
  double avgOhms = avg(fuelWindow);

  double gallons = 29.0207 + (-7.0567 * log(avgOhms));
  int pct = floor((gallons / 14) * 100);
  if (pct < 0)
  {
    pct = 0;
  }
  return pct;
}

void moveToHalfWidth()
{
  tft.setCursor(WIDTH / 2 + 8, tft.getCursorY());
  clearLine();
}

void writeStatus(int bottomPanelY)
{
  tft.setCursor(WIDTH / 2 + 8, bottomPanelY);
  // tft.fillRect(WIDTH / 2 + 1, bottomPanelY - 4, WIDTH, HEIGHT, BACKGROUND_COLOR);

  moveToHalfWidth();
  if (isNthBitSet(currentStatus.status4, BIT_STATUS4_FAN))
  {
    tft.println("Fan On");
  }
  else
  {
    tft.println("Fan Off");
  }
  moveToHalfWidth();

  bool hasError = false;

  if (currentStatus.coolant > LIMIT_COOLANT_UPPER)
  {
    tft.setTextColor(errorColors.text);
    tft.println("Eng Hot!");
    moveToHalfWidth();
    hasError = true;
  }
  if (readFuel() < LIMIT_FUEL_LOWER)
  {
    tft.setTextColor(errorColors.text);
    tft.println("Low Gas!");
    moveToHalfWidth();
    hasError = true;
  }
  if (currentStatus.oilPressure < LIMIT_OIL_LOWER)
  {
    tft.setTextColor(errorColors.text);
    tft.println("Low Oil Prs!");
    moveToHalfWidth();
    hasError = true;
  }
  if (currentStatus.RPM > LIMIT_RPM_UPPER)
  {
    tft.setTextColor(errorColors.text);
    tft.println("Over rev!");
    moveToHalfWidth();
    hasError = true;
  }

  if (!hasError)
  {
    tft.setTextColor(okColors.text);
    tft.println("All OK");
  }

  tft.setTextColor(okColors.text);
  bool isRunning = isNthBitSet(currentStatus.engine, BIT_ENGINE_RUN);
  bool isCranking = isNthBitSet(currentStatus.engine, BIT_ENGINE_CRANK);
  if (!isRunning && !isCranking)
  {
    moveToHalfWidth();
    tft.setTextColor(errorColors.text);
    tft.println("Eng Off");
    tft.setTextColor(okColors.text);
  }

  if (isRunning)
  {
    moveToHalfWidth();
    tft.println("Running");
  }
  if (isCranking)
  {
    moveToHalfWidth();
    tft.println("Cranking");
  }
  if (isNthBitSet(currentStatus.engine, BIT_ENGINE_WARMUP))
  {
    moveToHalfWidth();
    tft.println("Warmup");
  }
  if (isNthBitSet(currentStatus.engine, BIT_ENGINE_ASE))
  {
    moveToHalfWidth();
    tft.println("ASE");
  }
}

void writeSecondaries(int bottomPanelY)
{
  tft.setTextColor(ILI9341_WHITE, BACKGROUND_COLOR);

  char o2[8];
  dtostrf(((double)currentStatus.O2) / 10.0, 5, 1, o2);
  tft.print("A/F    ");
  tft.print(o2);
  tft.println(" ");

  tft.print("TIMING  ");
  tft.print(currentStatus.advance);
  tft.println("  ");

  tft.print("IAT     ");
  tft.print(currentStatus.IAT);
  tft.println("  ");

  tft.print("VOLTS  ");
  char volts[8];
  dtostrf(((double)currentStatus.battery10) / 10.0, 5, 1, volts);
  tft.print(volts);
  tft.println(" ");

  tft.print("MET     ");
  tft.print(currentStatus.secl);
  tft.println("  ");
}

void renderGauge(const char *label, int value, int min, int max, Colors colors)
{
  tft.setTextSize(2);
  tft.setTextColor(colors.text, colors.background);
  tft.print(label);
  tft.print(" ");
  tft.print(value);
  tft.println("            ");
  showGauge(value, min, max, colors.bar);
}

static uint32_t oldtime = millis(); // for the timeout

#define RESPONSE_LEN 128
uint8_t speeduinoResponse[RESPONSE_LEN];

void processResponse()
{

  currentStatus.secl = speeduinoResponse[0];
  currentStatus.status1 = speeduinoResponse[1];
  currentStatus.engine = speeduinoResponse[2];
  currentStatus.syncLossCounter = speeduinoResponse[3];
  currentStatus.MAP = ((speeduinoResponse[5] << 8) | (speeduinoResponse[4]));
  currentStatus.IAT = speeduinoResponse[6];
  currentStatus.coolant = speeduinoResponse[7] - 40;
  currentStatus.batCorrection = speeduinoResponse[8];
  currentStatus.battery10 = speeduinoResponse[9];
  currentStatus.O2 = speeduinoResponse[10];
  currentStatus.egoCorrection = speeduinoResponse[11];
  currentStatus.iatCorrection = speeduinoResponse[12];
  currentStatus.wueCorrection = speeduinoResponse[13];

  currentStatus.RPM = ((speeduinoResponse[15] << 8) | (speeduinoResponse[14]));

  currentStatus.TAEamount = speeduinoResponse[16];
  currentStatus.corrections = speeduinoResponse[17];
  currentStatus.ve = speeduinoResponse[18];
  currentStatus.afrTarget = speeduinoResponse[19];

  currentStatus.PW1 = ((speeduinoResponse[21] << 8) | (speeduinoResponse[20]));

  currentStatus.tpsDOT = speeduinoResponse[22];
  currentStatus.advance = speeduinoResponse[23];
  currentStatus.TPS = speeduinoResponse[24];

  currentStatus.loopsPerSecond = ((speeduinoResponse[26] << 8) | (speeduinoResponse[25]));
  currentStatus.freeRAM = ((speeduinoResponse[28] << 8) | (speeduinoResponse[27]));

  currentStatus.boostTarget = speeduinoResponse[29];
  currentStatus.boostDuty = speeduinoResponse[30];
  currentStatus.spark = speeduinoResponse[31];

  currentStatus.rpmDOT = ((speeduinoResponse[33] << 8) | (speeduinoResponse[32]));
  currentStatus.ethanolPct = speeduinoResponse[34];

  currentStatus.flexCorrection = speeduinoResponse[35];
  currentStatus.flexIgnCorrection = speeduinoResponse[36];
  currentStatus.idleLoad = speeduinoResponse[37];
  currentStatus.testOutputs = speeduinoResponse[38];
  currentStatus.O2_2 = speeduinoResponse[39];
  currentStatus.baro = speeduinoResponse[40];
  currentStatus.CANin_1 = ((speeduinoResponse[42] << 8) | (speeduinoResponse[41]));
  currentStatus.CANin_2 = ((speeduinoResponse[44] << 8) | (speeduinoResponse[43]));
  currentStatus.CANin_3 = ((speeduinoResponse[46] << 8) | (speeduinoResponse[45]));
  currentStatus.CANin_4 = ((speeduinoResponse[48] << 8) | (speeduinoResponse[47]));
  currentStatus.CANin_5 = ((speeduinoResponse[50] << 8) | (speeduinoResponse[49]));
  currentStatus.CANin_6 = ((speeduinoResponse[52] << 8) | (speeduinoResponse[51]));
  currentStatus.CANin_7 = ((speeduinoResponse[54] << 8) | (speeduinoResponse[53]));
  currentStatus.CANin_8 = ((speeduinoResponse[56] << 8) | (speeduinoResponse[55]));
  currentStatus.CANin_9 = ((speeduinoResponse[58] << 8) | (speeduinoResponse[57]));
  currentStatus.CANin_10 = ((speeduinoResponse[60] << 8) | (speeduinoResponse[59]));
  currentStatus.CANin_11 = ((speeduinoResponse[62] << 8) | (speeduinoResponse[61]));
  currentStatus.CANin_12 = ((speeduinoResponse[64] << 8) | (speeduinoResponse[63]));
  currentStatus.CANin_13 = ((speeduinoResponse[66] << 8) | (speeduinoResponse[65]));
  currentStatus.CANin_14 = ((speeduinoResponse[68] << 8) | (speeduinoResponse[67]));
  currentStatus.CANin_15 = ((speeduinoResponse[70] << 8) | (speeduinoResponse[69]));
  currentStatus.CANin_16 = ((speeduinoResponse[72] << 8) | (speeduinoResponse[71]));
  currentStatus.tpsADC = speeduinoResponse[73];
  currentStatus.getNextError = speeduinoResponse[74];
  currentStatus.launchCorrection = speeduinoResponse[75];
  currentStatus.PW2 = ((speeduinoResponse[77] << 8) | (speeduinoResponse[76]));
  currentStatus.PW3 = ((speeduinoResponse[79] << 8) | (speeduinoResponse[78]));
  currentStatus.PW4 = ((speeduinoResponse[81] << 8) | (speeduinoResponse[80]));
  currentStatus.status3 = speeduinoResponse[82];
  currentStatus.engineProtectStatus = speeduinoResponse[83];
  currentStatus.fuelLoad = ((speeduinoResponse[85] << 8) | (speeduinoResponse[84]));
  currentStatus.ignLoad = ((speeduinoResponse[87] << 8) | (speeduinoResponse[86]));
  currentStatus.injAngle = ((speeduinoResponse[89] << 8) | (speeduinoResponse[88]));
  currentStatus.idleDuty = speeduinoResponse[90];
  currentStatus.CLIdleTarget = speeduinoResponse[91];
  currentStatus.mapDOT = speeduinoResponse[92];
  currentStatus.vvt1Angle = speeduinoResponse[93];
  currentStatus.vvt1TargetAngle = speeduinoResponse[94];
  currentStatus.vvt1Duty = speeduinoResponse[95];
  currentStatus.flexBoostCorrection = ((speeduinoResponse[97] << 8) | (speeduinoResponse[96]));
  currentStatus.baroCorrection = speeduinoResponse[98];
  currentStatus.ASEValue = speeduinoResponse[99];
  currentStatus.vss = ((speeduinoResponse[101] << 8) | (speeduinoResponse[100]));
  currentStatus.gear = speeduinoResponse[102];
  currentStatus.fuelPressure = speeduinoResponse[103];
  currentStatus.oilPressure = speeduinoResponse[104];
  currentStatus.wmiPW = speeduinoResponse[105];
  currentStatus.status4 = speeduinoResponse[106];
  // currentStatus.vvt2Angle = speeduinoResponse[107];
  // currentStatus.vvt2TargetAngle = speeduinoResponse[108];
  // currentStatus.vvt2Duty = speeduinoResponse[109];
  // currentStatus.outputsStatus = speeduinoResponse[110];
  // currentStatus.fuelTemp = speeduinoResponse[111];
  // currentStatus.fuelTempCorrection = speeduinoResponse[112];
  // currentStatus.VE1 = speeduinoResponse[113];
  // currentStatus.VE2 = speeduinoResponse[114];
  // currentStatus.advance1 = speeduinoResponse[115];
  // currentStatus.advance2 = speeduinoResponse[116];
  // currentStatus.nitrous_status = speeduinoResponse[117];
  // currentStatus.TS_SD_Status = speeduinoResponse[118];
  // currentStatus.fanDuty = speeduinoResponse[121];
}

void clearRx()
{
  while (Serial2.available() > 0)
  {
    Serial2.read();
  }
}

int popHeader()
{
  Serial2.setTimeout(500);
  if (Serial2.find('n'))
  {
    Serial1.println("Found N");

    if (Serial2.find(0x32))
    {
      while (!Serial2.available())
        ;
      Serial1.println("Return packetlen");
      return Serial2.read();
    }
  }
  return -1;
}

int unfucked = 0;
void requestData()
{

  // clearRx();
  Serial2.write("n"); // Send n to request real time data
  Serial1.println("requested data");

  int nLength = popHeader();
  if (nLength > 0)
  {
    Serial1.print("nLength=");
    Serial1.println(nLength);

    uint8_t nRead = Serial2.readBytes(speeduinoResponse, nLength);
    Serial1.print("nRead=");
    Serial1.println(nRead);

    if (nRead < nLength)
    {
      Serial1.println("nRead < nLength");
      bumpTimeout();
    }
    else
    {
      screenState = SCREEN_STATE_NORMAL;
      resetTimeout();
      processResponse();
      requestFrame = true;
    }
  }
  else
  {
    Serial1.println("popHeader -1");
    bumpTimeout();
  }
}

void clearScreen()
{
  tft.fillScreen(ILI9341_BLACK);
}

void renderNoConnection()
{
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextSize(3);
  tft.setCursor(0, 0);
  tft.println("No Connection");
}

void renderNoData()
{
  clearScreen();
  tft.setTextColor(ILI9341_LIGHTGREY);
  tft.setTextSize(4);
  tft.setCursor(0, 0);
  tft.println("No Data");
  tft.setTextSize(2);
  tft.print("Timeouts ");
  tft.print(timeouts);
  tft.println("  ");

  tft.print("Fuel ");
  tft.print(readFuel());
  tft.println("%   ");
}

void render()
{
  tft.setTextSize(3);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_CYAN);

  int fuel = readFuel();
  renderGauge("FUEL    ", fuel, 0, 100, fuel < LIMIT_FUEL_LOWER ? errorColors : okColors);
  renderGauge("RPM     ", currentStatus.RPM, 500, 7000, currentStatus.RPM > LIMIT_RPM_UPPER ? errorColors : okColors);

  int coolantF = (int)(((float)currentStatus.coolant) * 1.8 + 32);
  renderGauge("COOLANT ", coolantF, 50, 250, coolantF > LIMIT_COOLANT_UPPER ? errorColors : okColors);
  renderGauge("OIL     ", currentStatus.oilPressure, 0, 60, currentStatus.oilPressure < LIMIT_OIL_LOWER ? errorColors : okColors);
  // renderGauge("VOLTS", )
  // tft.setTextColor(ILI9341_WHITE, BACKGROUND_COLOR);
  // float volts = currentStatus.battery10;
  // tft.println(volts);
  // showGauge((int)(volts * 100), 100, 150, BAR_COLOR);
  // renderGauge("BAT", volts, 10, 15, volts < 13 ? errorColors : okColors);

  int bottomPanelY = tft.getCursorY();
  tft.drawLine(WIDTH / 2, bottomPanelY, WIDTH / 2, HEIGHT, ILI9341_WHITE);
  tft.drawLine(0, bottomPanelY, WIDTH, bottomPanelY, ILI9341_WHITE);

  tft.setTextSize(2);

  bottomPanelY = bottomPanelY + 6;
  tft.setCursor(0, bottomPanelY);
  writeSecondaries(bottomPanelY);
  writeStatus(bottomPanelY);

  requestFrame = false;
}

void setup()
{
  delay(500);
  okColors.bar = ILI9341_CYAN;
  okColors.background = BACKGROUND_COLOR;
  okColors.text = ILI9341_WHITE;

  errorColors.bar = ILI9341_RED;
  errorColors.background = BACKGROUND_COLOR;
  errorColors.text = ILI9341_ORANGE;

  tft.begin();
  tft.setRotation(3);
  requestFrame = false;
  renderNoConnection();

  while (!Serial1)
    ;
  Serial1.begin(115200); // ftdi serial
  delay(500);

  Serial2.setRx(PA3);
  Serial2.setTx(PA2);
  Serial2.setTimeout(500);
  Serial2.begin(115200); // speeduino runs at 115200

  delay(250);
}

void loop(void)
{
  requestData();

  if (screenState != lastScreenState || requestFrame)
  {
    if (screenState != lastScreenState)
    {
      clearScreen();
    }

    // if we're going in between states, then
    if (screenState == SCREEN_STATE_NO_DATA)
    {
      renderNoData();
    }
    else if (screenState == SCREEN_STATE_NO_CONNECTION)
    {
      renderNoConnection();
    }
    else if (screenState == SCREEN_STATE_NORMAL)
    {
      render();
    }
  }

  lastScreenState = screenState;
  requestFrame = false;
}
