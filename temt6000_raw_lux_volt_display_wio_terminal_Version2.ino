#include <TFT_eSPI.h>

#define TEMT6000_PIN   A0  // Analog pin

TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);

  tft.println("TEMT6000 Light Sensor");
  Serial.println("TEMT6000 Light Sensor");
  pinMode(TEMT6000_PIN, INPUT);
}

void loop() {
  int rawValue = analogRead(TEMT6000_PIN);
  float voltage = rawValue * (3.3 / 1023.0); // Wio Terminal ADC: 3.3V, 10-bit
  float lux = voltage * 100.0; // Approximation for TEMT6000

  tft.fillRect(0, 60, 320, 80, TFT_BLACK);
  tft.setCursor(0, 60);
  tft.printf("Raw: %d\nVolt: %.2f V\nLux: %.0f", rawValue, voltage, lux);

  Serial.print("Raw: "); Serial.print(rawValue);
  Serial.print("\tVolt: "); Serial.print(voltage, 2);
  Serial.print("\tLux: "); Serial.println(lux, 0);

  delay(500);
}