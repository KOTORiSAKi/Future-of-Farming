#include "DHT.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define DHTPIN 13       
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_AO 34      
#define SOIL_DO 25     

void setup() {
  Serial.begin(115200);
  Serial.println("Test");

  dht.begin();
  pinMode(SOIL_DO, INPUT);
}

void loop() {
  delay(10000);  // Read Every 10 Sec

  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT22");
  } else {
    Serial.print("Humidity : ");
    Serial.print(humidity);
    Serial.print(" %\n");
    Serial.println("---------------------");
    Serial.print("Temperature : ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.println("---------------------");
  }

  int soilAnalog = analogRead(SOIL_AO);   // 0–4095
  int soilPercent = map(soilAnalog, 0, 4095, 100, 0); // % ความชื้น

  int soilDigital = digitalRead(SOIL_DO); // 0 = เปียก, 1 = แห้ง

  Serial.print("Soil moisture (analog) : ");
  Serial.print(soilAnalog);
  Serial.print(" -> ");
  Serial.print(soilPercent);
  Serial.println(" %");
  Serial.println("---------------------");

  Serial.print("Soil moisture (digital) : ");
  Serial.println(soilDigital == 0 ? "Wet" : "Dry");
  Serial.println("---------------------");
}