#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(115200);
  delay(1000);
  Serial.println("I2C Scanner pentru Wio Terminal");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Caut dispozitive I2C...");

  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Gasit dispozitiv I2C la adresa 0x");
      if (address<16)
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println(" !");
      nDevices++;
    }
  }
  if (nDevices == 0)
    Serial.println("Niciun dispozitiv I2C gasit\n");
  else
    Serial.println("Scanare terminata\n");

  delay(3000); // asteapta 3 secunde inainte de urmatoarea scanare
}