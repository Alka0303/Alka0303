#include <Wire.h>
#include <PCF8575.h>
#include <PCAL9535A.h>
#include <TFT_eSPI.h>
#include <DHT11.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Configurare ---
#define PCF8575_ADDR 0x24
#define XL9535_ADDR  0x21

#define DHT1_PIN D0
#define DHT2_PIN D1

#define ONE_WIRE_BUS D4 // Data wire for DS18B20

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

// DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20_sensors(&oneWire);
DeviceAddress ds18b20_addresses[8]; // max 8 sensors
uint8_t ds18b20_count = 0;
float ds18b20_temps[8] = {0};

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
  for (uint8_t i = 0; i < 16; i++) {
    relayBoardXL.pinMode(i, OUTPUT);
    relayBoardXL.digitalWrite(i, LOW); // OFF la pornire
    stateXL[i] = false;
  }

  // PCF8575: toți INPUT (scriem HIGH ca să fie intrare) și setăm valorile la 0 la pornire
  for (uint8_t i = 0; i < 16; i++) {
    relayBoardPCF.write(i, HIGH); // rămâne INPUT (HIGH = intrare pentru PCF8575)
    statePCF[i] = false;
    prevPCF[i] = false;
  }

  // DS18B20
  ds18b20_sensors.begin();
  ds18b20_count = ds18b20_sensors.getDeviceCount();
  // Store addresses for quick access
  for (uint8_t i = 0; i < ds18b20_count && i < 8; i++) {
    ds18b20_sensors.getAddress(ds18b20_addresses[i], i);
  }
  ds18b20_sensors.setResolution(9); // 9bit for all

  drawLabels();
  updateDisplay();
}

// --- Loop ---
void loop() {
  readPCFInputs();

  // NU mai există corelare între PCF și XL! (eliminat orice legătură)
  // Comanda releelor XL trebuie făcută manual sau cu altă logică, nu după starea PCF!

  // Citire DHT11 la fiecare 2 secunde (pentru D0 și D1)
  if (millis() - lastDHTRead > DHT_INTERVAL) {
    dhtResult[0] = dht11_1.readTemperatureHumidity(temperature[0], humidity[0]);
    dhtResult[1] = dht11_2.readTemperatureHumidity(temperature[1], humidity[1]);
    lastDHTRead = millis();

    // Citire DS18B20 (se citește odată cu DHT11)
    ds18b20_sensors.requestTemperatures();
    for (uint8_t i = 0; i < ds18b20_count && i < 8; i++) {
      float temp = ds18b20_sensors.getTempC(ds18b20_addresses[i]);
      ds18b20_temps[i] = temp;
    }
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
  tft.setCursor(5, 0);      tft.printf("XL");
  tft.setCursor(65, 0);     tft.printf("PCF");
  tft.setCursor(115, 0);    tft.printf("DHT");
  tft.setCursor(175, 0);    tft.printf("DS18B20");
  for (uint8_t i = 0; i < 16; i++) {
    tft.setCursor(5, 12 + i * 10);
    tft.printf("%02d:", i + 1);
    tft.setCursor(55, 12 + i * 10);
    tft.printf("%02d:", i + 1);
    // DHT/DS18B20 label e pus la updateDisplay
  }
}

void updateDisplay() {
  for (uint8_t i = 0; i < 16; i++) {
    // XL (relee)
    tft.setCursor(35, 12 + i * 10);
    tft.setTextColor(stateXL[i] ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.printf("%s ", stateXL[i] ? "ON " : "OFF");

    // PCF (intrari) - mutat mai la dreapta
    tft.setCursor(85, 12 + i * 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.printf("%d ", statePCF[i] ? 1 : 0);

    // DHT: doar pentru primele 2 canale (corelate cu D0, D1)
    tft.setCursor(115, 12 + i * 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (i == 0 && dhtResult[0] == 0)
      tft.printf("%2dC/%2d%%", temperature[0], humidity[0]);
    else if (i == 1 && dhtResult[1] == 0)
      tft.printf("%2dC/%2d%%", temperature[1], humidity[1]);
    else
      tft.printf("     ");

    // DS18B20: afișare pe coloana nouă, max 8 senzori (primele 8 linii)
    tft.setCursor(175, 12 + i * 10);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    if (i < ds18b20_count && i < 8 && ds18b20_temps[i] > -100.0 && ds18b20_temps[i] < 125.0) {
      tft.printf("%5.2fC", ds18b20_temps[i]);
    } else {
      tft.printf("      ");
    }
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}