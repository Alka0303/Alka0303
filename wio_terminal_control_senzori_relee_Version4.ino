/*
  Wio Terminal - Control senzori și relee (XL + PCF)
  Integrează:
    - DS18B20 cu debounce și control timer ciclic pe 2 relee (XL)
    - DHT11, BMP180, HC-SR04, TEMT6000, UV analog, PCF8575
    - Releu 4 controlat de Dallas 1 >= 18°C ON, < 18°C OFF
    - Releu 5: ON 2 min, OFF 10 min ciclic
    - Releu 6: ON 20 sec, OFF 5 min ciclic
    - Releu 7: ON dacă HC-SR04 variază > 20 și TEMT6000 < 50, ON 20 sec după eveniment
    - Releu 8: ON dacă TEMT6000 < 100
    - Fără RTC, fără condiție orar
    - Carusel pagini mutat mai jos cu 10 pixeli
*/

#include <Wire.h>
#include <PCF8575.h>
#include <PCAL9535A.h>
#include <TFT_eSPI.h>
#include <DHT11.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_BMP085.h>

// === PINOUT ===
#define SW_RIGHT  WIO_5S_RIGHT
#define SW_LEFT   WIO_5S_LEFT
#define PCF8575_ADDR 0x24
#define XL9535_ADDR  0x21
#define DHT1_PIN D0
#define DHT2_PIN D1
#define ONE_WIRE_BUS D4 // DS18B20
#define TRIG_PIN  D2    // HC-SR04
#define ECHO_PIN  D3    // HC-SR04
#define TEMT6000_PIN   A5  // LUX
#define UV_PIN   A8         // UV analog

// === Senzori și module ===
PCF8575 relayBoardPCF(PCF8575_ADDR);
PCAL9535A::PCAL9535A<TwoWire> relayBoardXL(Wire);
TFT_eSPI tft;
DHT11 dht11_1(DHT1_PIN);
DHT11 dht11_2(DHT2_PIN);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20_sensors(&oneWire);
Adafruit_BMP085 bmp;

// === Variabile globale ===
bool stateXL[16];
bool statePCF[16];
bool prevPCF[16];

// DHT11
int temperature[2] = {0, 0};
int humidity[2] = {0, 0};
int dhtResult[2] = {0, 0};
unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

// DS18B20
DeviceAddress ds18b20_addresses[8];
uint8_t ds18b20_count = 0;
float ds18b20_temps[8] = {0};

// BMP180
float bmp180_temp = 0, bmp180_pressure = 0;

// HC-SR04
float hcsr04_distance = 0;
float hcsr04_distance_last = 0;
unsigned long lastHCSR04Read = 0;
const unsigned long HCSR04_INTERVAL = 500;

// TEMT6000
int temt6000_raw = 0;
float temt6000_voltage = 0, temt6000_lux = 0;
unsigned long lastTEMT6000Read = 0;
const unsigned long TEMT6000_INTERVAL = 500;

// UV
int uv_raw = 0;
float uv_voltage = 0, uv_index = 0;
unsigned long lastUVRead = 0;
const unsigned long UV_INTERVAL = 1000;

// === Timer ciclic DS18B20 pentru relee XL ===
#define DEBOUNCE_BUF 3
int temp_buf[DEBOUNCE_BUF] = {0};
uint8_t temp_idx = 0;
int ultim_temp_stabila = 0;
#define TEMP_MIN      28
#define TEMP_MAX      38
#define SEC_PER_GRAD  60
#define MAX_TIMER_RELEU2 600

int timer_releu2 = 0;   // Timerul ce trebuie parcurs de releul 2 (secunde, max 600)
int timp_releu2 = 0;    // Cât a stat efectiv ON releul 2
int timp_releu3 = 0;    // Cât a stat efectiv ON releul 3
int timer_releu3_target = 0; // Timpul pe care trebuie să îl consume releul 3 la scădere
bool scadere_detectata = false;
bool temp_sub_28 = false;

// === Timere pentru ciclice suplimentare ===
unsigned long releu5_last_change = 0;
const unsigned long releu5_on_time = 2 * 60 * 1000UL;    // 2 min
const unsigned long releu5_off_time = 10 * 60 * 1000UL;  // 10 min

unsigned long releu6_last_change = 0;
const unsigned long releu6_on_time = 20 * 1000UL;     // 20 sec
const unsigned long releu6_off_time = 5 * 60 * 1000UL; // 5 min

// === Pentru releu 7 ===
bool r7_should_be_on = false;
unsigned long r7_on_until = 0;
float r7_last_hc = 0;

// === Pagini afișaj ===
int page = 0;
const int totalPages = 5;

// === Prototipuri funcții ===
void drawLabels();
void updateDisplay();
void handleSwitch();
void readPCFInputs();
float readHCSR04();
void drawCarousel();
void displayPage0();

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

  for (uint8_t i = 0; i < 16; i++) {
    relayBoardXL.pinMode(i, OUTPUT);
    relayBoardXL.digitalWrite(i, LOW);
    stateXL[i] = false;
    relayBoardPCF.write(i, HIGH);
    statePCF[i] = false;
    prevPCF[i] = false;
  }

  ds18b20_sensors.begin();
  ds18b20_count = ds18b20_sensors.getDeviceCount();
  for (uint8_t i = 0; i < ds18b20_count && i < 8; i++)
    ds18b20_sensors.getAddress(ds18b20_addresses[i], i);
  ds18b20_sensors.setResolution(9);

  if (!bmp.begin()) {
    bmp180_temp = 0;
    bmp180_pressure = 0;
  }

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(TEMT6000_PIN, INPUT);
  pinMode(UV_PIN, INPUT);
  pinMode(SW_LEFT, INPUT_PULLUP);
  pinMode(SW_RIGHT, INPUT_PULLUP);

  drawLabels();
  updateDisplay();

  // Inițializare timere ciclice
  releu5_last_change = millis();
  releu6_last_change = millis();
  r7_last_hc = 0;
}

void loop() {
  handleSwitch();
  readPCFInputs();

  // ---- CITIRE SENZORI LA 1s ----
  static unsigned long lastSensorRead = 0;
  if (millis() - lastSensorRead > 1000) {
    // DS18B20
    ds18b20_sensors.requestTemperatures();
    for (uint8_t i = 0; i < ds18b20_count && i < 8; i++)
      ds18b20_temps[i] = ds18b20_sensors.getTempC(ds18b20_addresses[i]);

    // DHT
    dhtResult[0] = dht11_1.readTemperatureHumidity(temperature[0], humidity[0]);
    dhtResult[1] = dht11_2.readTemperatureHumidity(temperature[1], humidity[1]);

    // BMP
    bmp180_temp = bmp.readTemperature();
    bmp180_pressure = bmp.readPressure() / 100.0;

    // TEMT6000
    temt6000_raw = analogRead(TEMT6000_PIN);
    temt6000_voltage = temt6000_raw * (3.3 / 1023.0);
    temt6000_lux = temt6000_voltage * 100.0;

    // UV
    uv_raw = analogRead(UV_PIN);
    uv_voltage = uv_raw * (3.3 / 1023.0);
    uv_index = uv_voltage / 1.0 * 15.0;

    // HC-SR04
    hcsr04_distance_last = hcsr04_distance;
    hcsr04_distance = readHCSR04();

    lastSensorRead = millis();
  }

  // --- Debounce DS18B20.1 ciclic pentru R2/R3 ---
  static unsigned long debounce_last_read = 0;
  static bool debounce_first = true;
  if (millis() - debounce_last_read > 2000) {
    if (ds18b20_count > 0) {
      float t = ds18b20_temps[0];
      if (t > -100.0 && t < 125.0) {
        int temp_int = (int)t;
        temp_buf[temp_idx] = temp_int;
        temp_idx = (temp_idx + 1) % DEBOUNCE_BUF;

        bool stable = true;
        for (uint8_t i = 1; i < DEBOUNCE_BUF; i++)
          if (temp_buf[i] != temp_buf[0]) { stable = false; break; }

        if (stable) {
          int new_temp = temp_buf[0];
          if (debounce_first) { ultim_temp_stabila = new_temp; debounce_first = false; }
          if (new_temp > ultim_temp_stabila && new_temp >= TEMP_MIN && new_temp <= TEMP_MAX) {
            int delta = (new_temp - ultim_temp_stabila) * SEC_PER_GRAD;
            timer_releu2 += delta;
            if (timer_releu2 > MAX_TIMER_RELEU2) timer_releu2 = MAX_TIMER_RELEU2;
            scadere_detectata = false;
          } else if (new_temp < ultim_temp_stabila) {
            scadere_detectata = true;
            timer_releu3_target = timp_releu2;
          }
          ultim_temp_stabila = new_temp;
        }
        temp_sub_28 = (temp_int < TEMP_MIN);
      }
    }
    debounce_last_read = millis();
  }

  // --- LOGICA RELEE TIMERE DS18B20 R2/R3 ---
  static bool releu2_on = false, releu3_on = false;
  if (!scadere_detectata && timp_releu2 < timer_releu2) {
    releu2_on = true;
    releu3_on = false;
  } else if (scadere_detectata && temp_sub_28 && timp_releu3 < timer_releu3_target && timer_releu3_target > 0) {
    releu2_on = false;
    releu3_on = true;
  } else {
    releu2_on = false;
    releu3_on = false;
  }
  relayBoardXL.digitalWrite(1, releu2_on ? HIGH : LOW); stateXL[1] = releu2_on;
  relayBoardXL.digitalWrite(2, releu3_on ? HIGH : LOW); stateXL[2] = releu3_on;

  static unsigned long last_sec = 0;
  unsigned long now = millis();
  if (now - last_sec >= 1000) {
    if (releu2_on) timp_releu2++;
    if (releu3_on) timp_releu3++;
    last_sec += 1000;
  }
  if (!releu2_on && !releu3_on) last_sec = now;

  if (temp_sub_28 && !releu2_on && !releu3_on) {
    timer_releu2 = 0;
    timp_releu2 = 0; timp_releu3 = 0; timer_releu3_target = 0;
    scadere_detectata = false;
  }

  // --- Releu 4: DS18B20.1 >= 18°C ON, < 18°C OFF ---
  if (ds18b20_count > 0) {
    if (ds18b20_temps[0] >= 18.0) {
      relayBoardXL.digitalWrite(3, HIGH);
      stateXL[3] = true;
    } else {
      relayBoardXL.digitalWrite(3, LOW);
      stateXL[3] = false;
    }
  }

  // --- Releu 5: ON 2 min, OFF 10 min ciclic ---
  {
    unsigned long tnow = millis();
    unsigned long period = releu5_on_time + releu5_off_time;
    unsigned long cycle_pos = (tnow - releu5_last_change) % period;
    bool desired_state = (cycle_pos < releu5_on_time);
    relayBoardXL.digitalWrite(4, desired_state ? HIGH : LOW);
    stateXL[4] = desired_state;
  }

  // --- Releu 6: ON 20 sec, OFF 5 min ciclic ---
  {
    unsigned long tnow = millis();
    unsigned long period = releu6_on_time + releu6_off_time;
    unsigned long cycle_pos = (tnow - releu6_last_change) % period;
    bool desired_state = (cycle_pos < releu6_on_time);
    relayBoardXL.digitalWrite(5, desired_state ? HIGH : LOW);
    stateXL[5] = desired_state;
  }

  // --- Releu 7: HC-SR04 variază cu >20 și TEMT6000 < 50, ON 20s după eveniment ---
  float delta = fabs(hcsr04_distance - r7_last_hc);
  if (delta > 20.0 && temt6000_lux < 50.0) {
    r7_should_be_on = true;
    r7_on_until = millis() + 20000UL;
  }
  r7_last_hc = hcsr04_distance;

  if (r7_should_be_on && millis() < r7_on_until) {
    relayBoardXL.digitalWrite(6, HIGH);
    stateXL[6] = true;
  } else {
    relayBoardXL.digitalWrite(6, LOW);
    stateXL[6] = false;
    r7_should_be_on = false;
  }

  // --- Releu 8: ON dacă TEMT6000 < 100 ---
  if (temt6000_lux < 100.0) {
    relayBoardXL.digitalWrite(7, HIGH);
    stateXL[7] = true;
  } else {
    relayBoardXL.digitalWrite(7, LOW);
    stateXL[7] = false;
  }

  updateDisplay();
  for (uint8_t i = 0; i < 16; i++) prevPCF[i] = statePCF[i];
  delay(50);
}

// === Funcții ===
void readPCFInputs() {
  uint16_t inVal = relayBoardPCF.read16();
  for (uint8_t i = 0; i < 16; i++)
    statePCF[i] = (inVal & (1 << i)) ? 1 : 0;
}

void handleSwitch() {
  static bool prevLeft = HIGH, prevRight = HIGH;
  bool nowLeft = digitalRead(SW_LEFT);
  bool nowRight = digitalRead(SW_RIGHT);
  if (prevRight == HIGH && nowRight == LOW) {
    page = (page + 1) % totalPages;
    tft.fillScreen(TFT_BLACK); drawLabels(); updateDisplay(); delay(200);
  }
  if (prevLeft == HIGH && nowLeft == LOW) {
    page = (page - 1 + totalPages) % totalPages;
    tft.fillScreen(TFT_BLACK); drawLabels(); updateDisplay(); delay(200);
  }
  prevLeft = nowLeft;
  prevRight = nowRight;
}

void drawLabels() {
  if (page == 0) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(5, 0);      tft.printf("XL");
    tft.setCursor(65, 0);     tft.printf("PCF");
    tft.setCursor(110, 0);    tft.printf("DHT");
    tft.setCursor(155, 0);    tft.printf("DS18B20");
    for (uint8_t i = 0; i < 16; i++) {
      tft.setCursor(5, 12 + i * 10); tft.printf("%02d:", i + 1);
      tft.setCursor(55, 12 + i * 10); tft.printf("%02d:", i + 1);
    }
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(5, 172);    tft.printf("BMP180:");
    tft.setCursor(80, 172);   tft.printf("HC-SR04:");
    tft.setCursor(140, 172);  tft.printf("TEMT6000:");
    tft.setCursor(200, 172);  tft.printf("UV:");
  } else {
    tft.fillScreen(TFT_BLACK);
  }
}

void updateDisplay() {
  switch (page) {
    case 0: displayPage0(); break;
    default: break;
  }
  drawCarousel();
}

void displayPage0() {
  for (uint8_t i = 0; i < 16; i++) {
    tft.setCursor(25, 12 + i * 10);
    tft.setTextColor(stateXL[i] ? TFT_GREEN : TFT_RED, TFT_BLACK);
    tft.printf("%s ", stateXL[i] ? "ON " : "OFF");
    tft.setCursor(85, 12 + i * 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.printf("%d ", statePCF[i] ? 1 : 0);
    tft.setCursor(100, 12 + i * 10);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if (i == 0 && dhtResult[0] == 0)
      tft.printf("%2dC/%2d%%", temperature[0], humidity[0]);
    else if (i == 1 && dhtResult[1] == 0)
      tft.printf("%2dC/%2d%%", temperature[1], humidity[1]);
    else tft.printf("     ");
    tft.setCursor(155, 12 + i * 10);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    if (i < ds18b20_count && i < 8 && ds18b20_temps[i] > -100.0 && ds18b20_temps[i] < 125.0)
      tft.printf("%5.2fC", ds18b20_temps[i]);
    else tft.printf("      ");
  }
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(5, 185); tft.printf("%5.1fC", bmp180_temp);
  tft.setCursor(5, 197); tft.printf("%6.1fhPa", bmp180_pressure);
  tft.setCursor(65, 185); tft.printf("%6.1fcm", hcsr04_distance);
  tft.setCursor(120, 185); tft.printf("%6.0flux", temt6000_lux);
  tft.setCursor(200, 185); tft.printf("%4.1f", uv_index);

  // Timer ciclic, page 0, afișare stare relee ciclice
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(5, 205);
  tft.printf("R2:%s %d/%d  R3:%s %d/%d",
    stateXL[1] ? "ON" : "OF", timp_releu2, timer_releu2,
    stateXL[2] ? "ON" : "OF", timp_releu3, timer_releu3_target);

  // Afișare stare relee speciale
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(150, 205);
  tft.printf("R4:%s R5:%s R6:%s", stateXL[3] ? "ON" : "OF", stateXL[4] ? "ON" : "OF", stateXL[5] ? "ON" : "OF");
  tft.setCursor(5, 215);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.printf("R7:%s R8:%s", stateXL[6] ? "ON" : "OF", stateXL[7] ? "ON" : "OF");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

float readHCSR04() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  float distance = duration * 0.0343f / 2.0f;
  if (duration == 0) return 0;
  return distance;
}

// Carusel pagini, mutat cu 10 pixeli mai jos
void drawCarousel() {
  int startY = 220; // era 210
  int startX = 60;
  int deltaX = 40;
  tft.setTextSize(1);
  for (int i = 0; i < totalPages; i++) {
    if (i == page) tft.setTextColor(TFT_RED, TFT_BLACK);
    else tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(startX + i * deltaX, startY);
    tft.printf("%d", i + 1);
  }
  tft.setTextSize(1);
}