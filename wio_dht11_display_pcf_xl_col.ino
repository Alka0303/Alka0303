#include <Wire.h>
#include <PCF8575.h>
#include <PCAL9535A.h>
#include <TFT_eSPI.h>
#include <DHT11.h>

// --- Configurare ---
#define PCF8575_ADDR 0x24
#define XL9535_ADDR  0x21

#define DHT1_PIN D0
#define DHT2_PIN D1

PCF8575 relayBoardPCF(PCF8575_ADDR);
PCAL9535A::PCAL9535A<TwoWire> relayBoardXL(Wire);
TFT_eSPI tft;

bool stateXL[16];
bool statePCF[16];
bool prevPCF[16];

// DHT11
DHT11 dht11_1(DHT1_PIN);
DHT11 dht11_2(DHT2_PIN);
int temperature[2] = {0, 0};
int humidity[2] = {0, 0};
int dhtResult[2] = {0, 0};
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

// --- Setup ---
void setup() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  Serial.begin(9600);
  Wire.begin();
  relayBoardPCF.begin();
  relayBoardXL.begin((PCAL9535A::HardwareAddress)(XL9535_ADDR - 0x20));

  // XL9535: TOATE OPRITE la pornire (LOW)
  // Dacă vrei PORNITE, schimbă LOW cu HIGH mai jos!
  for (uint8_t i = 0; i < 16; i++) {
    relayBoardXL.pinMode(i, OUTPUT);
    relayBoardXL.digitalWrite(i, LOW); // <-- TOATE OPRITE la repornire!
    stateXL[i] = false;
  }

  // PCF8575: toți INPUT (scriem HIGH ca să fie intrare)
  for (uint8_t i = 0; i < 16; i++) {
    relayBoardPCF.write(i, HIGH);
    statePCF[i] = false;
    prevPCF[i] = false;
  }

  drawLabels();
  updateDisplay();
}

// --- Loop ---
void loop() {
  readPCFInputs();

  // Toggle ieșiri XL după intrări PCF (1-1, 2-2, etc) pe front pozitiv
  for (uint8_t i = 0; i < 16; i++) {
    if (statePCF[i] && !prevPCF[i]) {
      stateXL[i] = !stateXL[i];
      relayBoardXL.digitalWrite(i, stateXL[i] ? HIGH : LOW);
    }
  }

  // Citire DHT11 la fiecare 2 secunde (pentru D0 și D1)
  if (millis() - lastDHTRead > DHT_INTERVAL) {
    dhtResult[0] = dht11_1.readTemperatureHumidity(temperature[0], humidity[0]);
    dhtResult[1] = dht11_2.readTemperatureHumidity(temperature[1], humidity[1]);
    lastDHTRead = millis();
  }

  updateDisplay();
  for (uint8_t i = 0; i < 16; i++) prevPCF[i] = statePCF[i];
  delay(50);
}

void readPCFInputs() {
  uint16_t inVal = relayBoardPCF.read16();
  for (uint8_t i = 0; i < 16; i++) {
    statePCF[i] = (inVal & (1 << i)) ? 1 : 0;
  }
}

void drawLabels() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(5, 0);     tft.printf("XL");
  tft.setCursor(45, 0);    tft.printf("PCF");
  tft.setCursor(105, 0);   tft.printf("DHT");
  for (uint8_t i = 0; i < 16; i++) {
    tft.setCursor(5, 12 + i * 10);
    tft.printf("%02d:", i + 1);
    tft.setCursor(45, 12 + i * 10);
    tft.printf("%02d:", i + 1);
    // DHT label e pus la updateDisplay
  }
}

void updateDisplay() {
  for (uint8_t i = 0; i < 16; i++) {
    // XL (relee)
    tft.setCursor(25, 12 + i * 10);
    tft.setTextColor(stateXL[i] ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.printf("%s ", stateXL[i] ? "ON " : "OFF");

    // PCF (intrari)
    tft.setCursor(65, 12 + i * 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.printf("%d ", statePCF[i] ? 1 : 0);

    // DHT: doar pentru primele 2 canale (corelate cu D0, D1)
    tft.setCursor(105, 12 + i * 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (i == 0 && dhtResult[0] == 0)
      tft.printf("%2dC/%2d%%", temperature[0], humidity[0]);
    else if (i == 1 && dhtResult[1] == 0)
      tft.printf("%2dC/%2d%%", temperature[1], humidity[1]);
    else
      tft.printf("     ");
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}