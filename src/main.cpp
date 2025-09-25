#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <time.h>

#define DHTPIN 27
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_AO 34
#define SOIL_DO 27
#define LDR_PIN 32 // GPIO pin connected to the LDR

#define BEEP_PIN 13 // GPIO pin connected to the onboard LED

#define W_SPRAY_PIN 33 // GPIO pin connected to the onboard LED
#define P_SPRAY_PIN 25 // GPIO pin connected to the onboard LED


static unsigned long last_pump_activation_time = 0;
static unsigned long last_mqtt_publish_time = 0;

const char *ssid = "ROG-KOTORISAKI";         // Replace with your WiFi SSID
const char *password = "0951034031";         // Replace with your WiFi password
const char *mqtt_server = "192.168.137.191"; // Replace with your MQTT broker IP
const int mqtt_port = 1883;                  // Replace with your MQTT broker port if different

const char *mqtt_publish_topic = "esp32/output";  // Topic to publish sensor data
const char *mqtt_subscribe_topic = "esp32/input"; // Topic to subscribe for commands

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[150]; // Increased buffer size to prevent overflow
int value = 0;

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_publish_topic, "!!!SAKI!!! ESP32 connected");
      // ... and resubscribe
      client.subscribe(mqtt_subscribe_topic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println("Starting...");

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  dht.begin();
  pinMode(SOIL_DO, INPUT);
  pinMode(W_SPRAY_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  digitalWrite(W_SPRAY_PIN, HIGH);
  pinMode(P_SPRAY_PIN, OUTPUT);
  digitalWrite(P_SPRAY_PIN, HIGH);
  pinMode(BEEP_PIN, OUTPUT);
  digitalWrite(BEEP_PIN, OUTPUT);
  digitalWrite(BEEP_PIN, LOW); // Set initial state to OFF
}

void loop()
{
  unsigned long current_time = millis();

  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // It's better to use non-blocking delays for sensor reading
  delay(2000); // Read Every 2 Sec
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature))
  {
    Serial.println("Failed to read from DHT22");
  }
  else
  {
    Serial.println("##########################################");
    Serial.println("#");
    Serial.print("# Humidity : ");
    Serial.print(humidity);
    Serial.print(" %\n");
    Serial.println("# ----------------------------------------");
    Serial.print("# Temperature : ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.println("# ----------------------------------------");
  }

  int soilAnalog = analogRead(SOIL_AO);               // 0–4095
  int soilPercent = map(soilAnalog, 0, 4095, 100, 0); // % ความชื้น

  int soilDigital = digitalRead(SOIL_DO); // 0 = เปียก, 1 = แห้ง

  Serial.print("# Soil moisture (analog) : ");
  Serial.print(soilAnalog);
  Serial.print(" -> ");
  Serial.print(soilPercent);
  Serial.println(" %");
  Serial.println("# ----------------------------------------");

  Serial.print("Soil moisture (digital) : ");
  Serial.println(soilDigital == 0 ? "Wet" : "Dry");
  Serial.println("---------------------");

  int ldrValue = analogRead(LDR_PIN); // Read LDR value (0-4095)
  Serial.print("# LDR Value : ");
  Serial.println(ldrValue);
  Serial.println("# ----------------------------------------");

  static bool W_active = false; // Water spray status
  if (humidity< 90 && W_active == false)
  {                                 // If soil is dry
    digitalWrite(W_SPRAY_PIN, LOW); // Turn on water spray (LED ON)
    Serial.println("# Water Spray: ON");
    W_active = true;
  }
  else if (humidity >= 90 && W_active == true)
  {
    digitalWrite(W_SPRAY_PIN, LOW);  // Turn off water spray (LED OFF)
    delay(100);                      // Small delay to ensure the LED state is set before printing
    digitalWrite(W_SPRAY_PIN, HIGH); // Turn off water spray (LED OFF)
    delay(100);                      // Small delay to ensure the LED state is set before printing
    digitalWrite(W_SPRAY_PIN, LOW);  // Turn off water spray (LED OFF)
    Serial.println("# Water Spray: OFF");
    W_active = false;
  }
  delay(100);                // Small delay to ensure the LED state is set before printing
  digitalWrite(W_SPRAY_PIN, HIGH); // Ensure water spray is off after check
  Serial.println("Status: Default");
  Serial.println("----------------------------------------");

  // Corrected Water Pump logic using millis()
  static bool P_active = false; // Water pump status
  static unsigned long last_pump_activation_time = 0;

  // Check if the pump has been active for 10 seconds and turn it off
  if (P_active && (millis() - last_pump_activation_time >= 3000))
  {
    digitalWrite(P_SPRAY_PIN, HIGH); // Turn off water pump (LED OFF)
    Serial.println("# Water Pump OFF");
    P_active = false;
  }
  // Turn on the pump if soil is dry and it hasn't been activated in the last 10 seconds
  if (!P_active && soilPercent < 50 && (millis() - last_pump_activation_time >= 10000))
  {
    digitalWrite(P_SPRAY_PIN, LOW); // Turn on water pump (LED ON)
    Serial.println("# Water Pump ON");
    P_active = true;
    last_pump_activation_time = millis(); // record the time it was turned on
  }
  Serial.println("# ----------------------------------------");

  if (temperature > 32)
  {
    Serial.println("# It's too hot!");
    digitalWrite(BEEP_PIN, HIGH); // Turn on buzzer (LED ON)
  }
  else if (temperature < 20) // Corrected logic for "too cold"
  {
    Serial.println("# It's too cold!");
    digitalWrite(BEEP_PIN, HIGH); // Turn off buzzer (LED OFF)
  }
  else
  {
    Serial.println("# Temperature is just right.");
    digitalWrite(BEEP_PIN, LOW); // Turn off buzzer (LED OFF)
  }
  Serial.println("##########################################");

  // Publish data to MQTT every 5 seconds (non-blocking)
  if (millis() - last_mqtt_publish_time >= 6000)
  { // Publish every 5 seconds (5000 milliseconds)
    sprintf(msg, "{\"temp\": %.2f, \"humid\": %.2f, \"soil\": %d, \"soil_percent\": %d, \"ldr\": %d}",
            temperature,
            humidity,
            soilAnalog,
            soilPercent,
            ldrValue);

    Serial.print("Publishing JSON to topic '");
    Serial.print(mqtt_publish_topic);
    Serial.print("': ");
    Serial.println(msg);

    client.publish(mqtt_publish_topic, msg);
    last_mqtt_publish_time = current_time; // Update the last publish time
  }
}

