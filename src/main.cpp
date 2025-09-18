#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

#define DHTPIN 26   
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_AO 34   
#define SOIL_DO 27

const char* ssid = "POCOX4"; // Replace with your WiFi SSID
const char* password = "11111111"; // Replace with your WiFi password
const char* mqtt_server = "10.12.49.85"; // Replace with
const int mqtt_port = 1883; // Replace with your MQTT broker port if different

const char* mqtt_publish_topic = "esp32/output"; // Topic to publish sensor data
const char* mqtt_subscribe_topic = "esp32/input"; // Topic to subscribe for commands

WiFiClient espClient;
PubSubClient client(espClient); 

long lastMsg = 0;
char msg[100]; // Increased buffer size to prevent overflow
int value = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_publish_topic, "!!!SAKI!!! ESP32 connected");
      // ... and resubscribe
      client.subscribe(mqtt_subscribe_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting...");

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  dht.begin();
  pinMode(SOIL_DO, INPUT);
}

void loop() {
  delay(3000);  // Read Every 10 Sec
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
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

  //
  sprintf(msg, "{\"temp\": %f, \"humid\": %f, \"soil\": %d, \"soil_percent\": %d, \"soil_digital\": %d}", 
          (float)temperature, 
          (float)humidity, 
          soilAnalog, 
          soilPercent, 
          soilDigital);
  Serial.print("Publishing JSON to topic '");
    Serial.print(mqtt_publish_topic);
    Serial.print("': ");
    Serial.println(msg);

    client.publish(mqtt_publish_topic, msg);
}