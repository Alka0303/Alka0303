#include <TFT_eSPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS D2
#define TEMP_MIN      28
#define TEMP_MAX      38
#define SEC_PER_GRAD  60
#define MAX_TIMER_RELEU2 600

TFT_eSPI tft = TFT_eSPI();
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress addr;

// Debounce
#define DEBOUNCE_BUF 3
int temp_buf[DEBOUNCE_BUF] = {0};
uint8_t temp_idx = 0;

// Timere și stări
int timer_releu2 = 0;   // Timerul ce trebuie parcurs de releul 2 (secunde, max 600)
int timp_releu2 = 0;    // Cât a stat efectiv ON releul 2
int timp_releu3 = 0;    // Cât a stat efectiv ON releul 3
int ultim_temp_stabila = 0; // Ultima temp stabilă debounce
bool scadere_detectata = false; // S-a detectat scădere temperatură?
bool temp_sub_28 = false;       // E sub 28C temperatura curentă?

void setup() {
  Serial.begin(9600);
  sensors.begin();
  if (!sensors.getAddress(addr, 0)) {
    Serial.println("No DS18B20 found!");
    while (1);
  }
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("DS18B20 Dual Timer");
  delay(1000);
  tft.fillScreen(TFT_BLACK);
}

void loop() {
  static unsigned long last_read = 0, last_sec = 0;
  static int last_disp_temp = -100;
  static int timer_releu3_target = 0; // Timpul pe care trebuie să îl consume releul 3 la scădere
  unsigned long now = millis();

  // Citire temperatura cu debounce la 2s
  if (now - last_read > 2000) {
    sensors.requestTemperatures();
    float t = sensors.getTempC(addr);

    if (t > -100.0 && t < 125.0) {
      int temp_int = (int)t;
      temp_buf[temp_idx] = temp_int;
      temp_idx = (temp_idx + 1) % DEBOUNCE_BUF;

      bool stable = true;
      for (uint8_t i = 1; i < DEBOUNCE_BUF; i++)
        if (temp_buf[i] != temp_buf[0]) { stable = false; break; }

      static bool first = true;
      if (stable) {
        int new_temp = temp_buf[0];
        // Prima citire stabilă
        if (first) {
          ultim_temp_stabila = new_temp;
          first = false;
        }
        // CREȘTERE temperatură (doar între 28-38)
        if (new_temp > ultim_temp_stabila && new_temp >= TEMP_MIN && new_temp <= TEMP_MAX) {
          int delta = (new_temp - ultim_temp_stabila) * SEC_PER_GRAD;
          timer_releu2 += delta;
          if (timer_releu2 > MAX_TIMER_RELEU2) timer_releu2 = MAX_TIMER_RELEU2;
          Serial.print("Crestere: +"); Serial.print(delta); Serial.print(" sec, total: "); Serial.println(timer_releu2);
          scadere_detectata = false;
        }
        // SCADE temperatura
        else if (new_temp < ultim_temp_stabila) {
          // Timerul pentru releu2 se oprește, se setează ținta pentru releu3
          scadere_detectata = true;
          timer_releu3_target = timp_releu2; // Timpul ON efectiv al releului 2
          Serial.print("Scadere detectata, timer_releu3_target = "); Serial.println(timer_releu3_target);
        }
        ultim_temp_stabila = new_temp;
      }

      // Actualizare display temperatura
      if ((int)t != last_disp_temp) {
        tft.fillRect(0, 30, 320, 40, TFT_BLACK);
        tft.setCursor(10, 30);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.printf("Temp: %.2f C", t);
        last_disp_temp = (int)t;
      }
      temp_sub_28 = (temp_int < TEMP_MIN);
    }
    last_read = now;
  }

  // LOGICA RELEE
  static bool releu2_on = false, releu3_on = false;
  // RELEU 2: ON cât timp nu s-a detectat scădere și nu și-a terminat timerul
  if (!scadere_detectata && timp_releu2 < timer_releu2) {
    releu2_on = true;
    releu3_on = false;
  }
  // RELEU 3: ON doar când a venit scăderea, temp < 28, și trebuie să consume fix cât a stat ON releul 2
  else if (scadere_detectata && temp_sub_28 && timp_releu3 < timer_releu3_target && timer_releu3_target > 0) {
    releu2_on = false;
    releu3_on = true;
  } else {
    releu2_on = false;
    releu3_on = false;
  }

  // Incrementare secunde pentru relee active
  if (now - last_sec >= 1000) {
    if (releu2_on) timp_releu2++;
    if (releu3_on) timp_releu3++;
    last_sec += 1000;
  }
  if (!releu2_on && !releu3_on) last_sec = now;

  // RESET: când temp < 28 și ambele relee sunt oprite
  if (temp_sub_28 && !releu2_on && !releu3_on) {
    timer_releu2 = 0;
    timp_releu2 = 0;
    timp_releu3 = 0;
    timer_releu3_target = 0;
    scadere_detectata = false;
  }

  // Afisaj pe ecran
  static int last_disp_timp2 = -1, last_disp_timp3 = -1;
  static bool last_releu2_on = false, last_releu3_on = false;
  if (timp_releu2 != last_disp_timp2 || timp_releu3 != last_disp_timp3 ||
      releu2_on != last_releu2_on || releu3_on != last_releu3_on) {
    tft.fillRect(0, 80, 320, 160, TFT_BLACK);
    tft.setCursor(10, 80);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.printf("Releu2: %s  %d/%d", releu2_on ? "ON " : "OFF", timp_releu2, timer_releu2);
    tft.setCursor(10, 120);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.printf("Releu3: %s  %d/%d", releu3_on ? "ON " : "OFF", timp_releu3, timer_releu3_target);
    last_disp_timp2 = timp_releu2;
    last_disp_timp3 = timp_releu3;
    last_releu2_on = releu2_on;
    last_releu3_on = releu3_on;
  }

  delay(10);
}