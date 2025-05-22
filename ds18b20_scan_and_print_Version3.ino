#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port D2
#define ONE_WIRE_BUS D2

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup(void) {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial Monitor to open

  sensors.begin();

  // Set all sensors to 9 bit resolution
  sensors.setResolution(9);

  Serial.println("Scanning for DS18B20 sensors...");
  int numberOfDevices = sensors.getDeviceCount();
  Serial.print("Found ");
  Serial.print(numberOfDevices);
  Serial.println(" sensor(s).");
}

void loop(void) {
  int numberOfDevices = sensors.getDeviceCount();

  sensors.requestTemperatures();

  for (int i = 0; i < numberOfDevices; i++) {
    float tempC = sensors.getTempCByIndex(i);
    Serial.print("Sensor ");
    Serial.print(i + 1);
    Serial.print(": ");
    if (tempC == DEVICE_DISCONNECTED_C) {
      Serial.println("Not found or disconnected");
    } else {
      Serial.print(tempC, 2);
      Serial.println(" °C");
    }
  }
  Serial.println("------------------------");
  delay(1000); // Update every second
}