#include <Wire.h>
#include <TFT_eSPI.h>

// --- DS18B20 ---
#include <OneWire.h>
#include <DallasTemperature.h>
#define DS18B20_PIN      22
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
DeviceAddress dsAddr;
bool ds18b20Available = false;

// --- DHT11 ---
#include <DHT.h>
#define DHTTYPE DHT11
#define DHT11_PIN1 13
#define DHT11_PIN2 15
DHT dht1(DHT11_PIN1, DHTTYPE);
DHT dht2(DHT11_PIN2, DHTTYPE);
bool dht1Available = false, dht2Available = false;

// --- TEMT32 (analog) ---
#define TEMT32_PIN 32

// --- HC-SR04 ---
#define HCSR04_TRIG 16
#define HCSR04_ECHO 18

// --- Extensii I2C PCF8575/XL9535 ---
#include <PCF8575.h>
#include <Adafruit_MCP23X17.h>
#define PCF8575_ADDR 0x24
#define XL9535_ADDR  0x21
PCF8575 pcf8575(PCF8575_ADDR);
Adafruit_MCP23X17 xl9535;
bool pcf8575Available = false, xl9535Available = false;

// --- BMP180 ---
#include <Adafruit_BMP085.h>
Adafruit_BMP085 bmp180;
bool bmp180Available = false;

// --- 5-way Switch ---
#define SW_RIGHT  WIO_5S_RIGHT
#define SW_LEFT   WIO_5S_LEFT

TFT_eSPI tft;

int page = 0;
const int totalPages = 9; // Actualizat cu BMP180

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // --- DS18B20 ---
  ds18b20.begin();
  if (ds18b20.getDS18Count() > 0) {
    ds18b20.getAddress(dsAddr, 0);
    ds18b20Available = true;
  }

  // --- DHT11 ---
  dht1.begin();
  dht2.begin();
  dht1Available = true;
  dht2Available = true;

  // --- PCF8575 / XL9535 ---
  Wire.begin();
  if (pcf8575.begin()) pcf8575Available = true;
  if (xl9535.begin_I2C(XL9535_ADDR)) xl9535Available = true;

  // --- HC-SR04 ---
  pinMode(HCSR04_TRIG, OUTPUT);
  pinMode(HCSR04_ECHO, INPUT);
  digitalWrite(HCSR04_TRIG, LOW);

  // --- BMP180 ---
  if (bmp180.begin()) bmp180Available = true;

  // --- 5-way Switch ---
  pinMode(SW_RIGHT, INPUT_PULLUP);
  pinMode(SW_LEFT, INPUT_PULLUP);
}

void loop() {
  static bool prevLeft = HIGH, prevRight = HIGH;
  bool nowLeft = digitalRead(SW_LEFT);
  bool nowRight = digitalRead(SW_RIGHT);

  // Navigare pagini circular
  if (prevRight == HIGH && nowRight == LOW) {
    page = (page + 1) % totalPages;
    tft.fillScreen(TFT_BLACK);
    delay(200);
  }
  if (prevLeft == HIGH && nowLeft == LOW) {
    page = (page - 1 + totalPages) % totalPages;
    tft.fillScreen(TFT_BLACK);
    delay(200);
  }
  prevLeft = nowLeft;
  prevRight = nowRight;

  switch (page) {
    case 0: showDS18B20(); break;
    case 1: showDHT11_1(); break;
    case 2: showDHT11_2(); break;
    case 3: showTEMT32(); break;
    case 4: showHCSR04(); break;
    case 5: showPCF8575(); break;
    case 6: showXL9535(); break;
    case 7: showBMP180(); break;
    case 8: showInfoPage(); break; // pagină de status general (opțional)
  }
  delay(100);
}

// --- Pagini senzori ---
void showDS18B20() {
  tft.setCursor(0,0); tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextSize(3);
  tft.println("DS18B20");
  tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  if (!ds18b20Available) { tft.println("Sensor not found!"); return; }
  ds18b20.requestTemperatures();
  float temp = ds18b20.getTempC(dsAddr);
  tft.printf("Temp: %.2f C", temp);
}

void showDHT11_1() {
  tft.setCursor(0,0); tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextSize(3);
  tft.println("DHT11 #1"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  if (!dht1Available) { tft.println("Sensor not found!"); return; }
  float temp = dht1.readTemperature(); float hum = dht1.readHumidity();
  if (isnan(temp) || isnan(hum)) { tft.println("Read error!"); return; }
  tft.printf("Temp: %.1f C\nHum: %.1f%%", temp, hum);
}

void showDHT11_2() {
  tft.setCursor(0,0); tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextSize(3);
  tft.println("DHT11 #2"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  if (!dht2Available) { tft.println("Sensor not found!"); return; }
  float temp = dht2.readTemperature(); float hum = dht2.readHumidity();
  if (isnan(temp) || isnan(hum)) { tft.println("Read error!"); return; }
  tft.printf("Temp: %.1f C\nHum: %.1f%%", temp, hum);
}

void showTEMT32() {
  tft.setCursor(0,0); tft.setTextColor(TFT_MAGENTA, TFT_BLACK); tft.setTextSize(3);
  tft.println("TEMT32"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  int raw = analogRead(TEMT32_PIN);
  float voltage = raw * (3.3 / 1023.0);
  tft.printf("Raw: %d\nVolt: %.2f V", raw, voltage);
}

void showHCSR04() {
  tft.setCursor(0,0); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setTextSize(3);
  tft.println("HC-SR04"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  // Trigger pulse
  digitalWrite(HCSR04_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(HCSR04_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(HCSR04_TRIG, LOW);
  long duration = pulseIn(HCSR04_ECHO, HIGH, 30000);
  float distance = duration > 0 ? duration * 0.0343 / 2 : 0;
  if (duration == 0) tft.println("No Echo!");
  else tft.printf("Dist: %.1f cm", distance);
}

void showPCF8575() {
  tft.setCursor(0,0); tft.setTextColor(TFT_BLUE, TFT_BLACK); tft.setTextSize(3);
  tft.println("PCF8575"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  if (!pcf8575Available) { tft.println("Not found!"); return; }

  tft.println("Pins 0-15:");
  for (uint8_t i = 0; i < 16; i++) {
    bool state = pcf8575.read(i);
    tft.printf("P%u:%s ", i, state ? "H" : "L");
    if (i == 7) tft.println();
  }
}

void showXL9535() {
  tft.setCursor(0,0); tft.setTextColor(TFT_ORANGE, TFT_BLACK); tft.setTextSize(3);
  tft.println("XL9535"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  if (!xl9535Available) { tft.println("Not found!"); return; }
  bool state = xl9535.digitalRead(0);
  tft.printf("P0: %s", state ? "HIGH" : "LOW");
}

void showBMP180() {
  tft.setCursor(0,0); tft.setTextColor(TFT_RED, TFT_BLACK); tft.setTextSize(3);
  tft.println("BMP180"); tft.setTextSize(2); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(0,60);
  if (!bmp180Available) { tft.println("Sensor not found!"); return; }
  float temp = bmp180.readTemperature();
  float pres = bmp180.readPressure() / 100.0;
  tft.printf("Temp: %.2f C\nPres: %.2f hPa", temp, pres);
}

// Pagină informativă - opțional, poți personaliza
void showInfoPage() {
  tft.setCursor(0,0); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2);
  tft.println("Status Senzori:");
  tft.printf("DS18B20    : %s\n", ds18b20Available ? "OK" : "Missing");
  tft.printf("DHT11 #1   : %s\n", dht1Available ? "OK" : "Missing");
  tft.printf("DHT11 #2   : %s\n", dht2Available ? "OK" : "Missing");
  tft.printf("TEMT32     : Analog\n");
  tft.printf("HC-SR04    : Digital\n");
  tft.printf("PCF8575    : %s\n", pcf8575Available ? "OK" : "Missing");
  tft.printf("XL9535     : %s\n", xl9535Available ? "OK" : "Missing");
  tft.printf("BMP180     : %s\n", bmp180Available ? "OK" : "Missing");
}