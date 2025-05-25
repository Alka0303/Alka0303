#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <TFT_eSPI.h>

// ---- BMP180 ----
Adafruit_BMP085 bmp;
bool bmpAvailable = false;

// ---- HC-SR04 ----
#define TRIG_PIN  D2
#define ECHO_PIN  D3

// ---- 5-way Switch ----
#define SW_RIGHT  WIO_5S_RIGHT
#define SW_LEFT   WIO_5S_LEFT

TFT_eSPI tft;

int page = 0;
const int totalPages = 2;

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // BMP180
  if (!bmp.begin()) {
    bmpAvailable = false;
  } else {
    bmpAvailable = true;
  }

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // 5-way Switch
  pinMode(SW_RIGHT, INPUT_PULLUP);
  pinMode(SW_LEFT, INPUT_PULLUP);
}

void loop() {
  static bool prevLeft = HIGH, prevRight = HIGH;
  bool nowLeft = digitalRead(SW_LEFT);
  bool nowRight = digitalRead(SW_RIGHT);

  // Navigare pagini
  if (prevRight == HIGH && nowRight == LOW) {
    page = (page + 1) % totalPages;
    tft.fillScreen(TFT_BLACK);
    delay(200); // debounce
  }
  if (prevLeft == HIGH && nowLeft == LOW) {
    page = (page - 1 + totalPages) % totalPages;
    tft.fillScreen(TFT_BLACK);
    delay(200); // debounce
  }
  prevLeft = nowLeft;
  prevRight = nowRight;

  switch (page) {
    case 0:
      showBMP180();
      break;
    case 1:
      showHCSR04();
      break;
  }
  delay(100);
}

void showBMP180() {
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.println("BMP180");
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 60);

  if (!bmpAvailable) {
    tft.println("Sensor not found!");
    return;
  }

  float temp = bmp.readTemperature();
  float pres = bmp.readPressure() / 100.0; // hPa
  tft.printf("Temp: %.2f C\nPres: %.2f hPa", temp, pres);
}

void showHCSR04() {
  long duration;
  float distance;

  // Trigger pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo
  duration = pulseIn(ECHO_PIN, HIGH, 30000);

  distance = duration > 0 ? duration * 0.0343 / 2 : 0;

  tft.setCursor(0, 0);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(3);
  tft.println("HC-SR04");
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 60);

  if (duration == 0) {
    tft.println("No Echo!");
  } else {
    tft.printf("Dist: %.1f cm", distance);
  }
}