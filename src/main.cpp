#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <time.h>

#define DHTPIN 26
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_AO 34
#define SOIL_DO 27
#define LDR_PIN 32 // GPIO pin connected to the LDR

#define BEEP_PIN 13 // GPIO pin connected to the onboard LED

#define W_SPRAY_PIN 33 // GPIO pin connected to the onboard LED
#define P_SPRAY_PIN 25 // GPIO pin connected to the onboard LED


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

unsigned long last_reconnect_attempt = 0;

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
      last_reconnect_attempt = millis();
      // Wait 5 seconds before retrying
      // delay(5000); // Replaced with non-blocking logic in loop()
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
  digitalWrite(BEEP_PIN, HIGH); // Set initial state to OFF
}

// Timers for non-blocking tasks
unsigned long last_sensor_read_time = 0;
const long sensor_read_interval = 5000; // 5 วินาที

unsigned long last_mqtt_publish_time = 0;
const long mqtt_publish_interval = 10000; // 10 วินาที

unsigned long last_pump_activation_time = 0;
const long pump_on_duration = 3000; // 3 วินาที
const long pump_cooldown = 10000; // 10 วินาที

unsigned long beep_start_time = 0;
const long beep_duration = 500; // 0.5 วินาที

float humidity;
float temperature;

void loop()
{
  unsigned long currentMillis = millis();
  if (!client.connected())
  {
    if (currentMillis - last_reconnect_attempt > 5000) {
      last_reconnect_attempt = currentMillis;
      reconnect();
    }
  }
  client.loop();
  
  // It's better to use non-blocking delays for sensor reading
  if (currentMillis - last_sensor_read_time >= sensor_read_interval)
  {
    last_sensor_read_time = currentMillis;

  float hum = dht.readHumidity();
  float tem = dht.readTemperature();


  if (isnan(hum) || isnan(tem))
  {
    Serial.println("Failed to read from DHT22");
    if (isnan(humidity) && isnan(temperature))
    {
      humidity = 50.0; // Default humidity value
      temperature = 25.0; // Default temperature value
    }
  }
  else if (hum > 2 && tem > 2 && !isnan(hum) && !isnan(tem))
  {
    humidity = hum;
    temperature = tem;
  }

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
  Serial.println(soilPercent > 30 ? "Wet" : "Dry");
  Serial.println("---------------------");

  int ldrValue = analogRead(LDR_PIN); // Read LDR value (0-4095)
  Serial.print("# LDR Value : ");
  Serial.println(ldrValue);
  Serial.println("# ----------------------------------------");

  static bool W_active = false; // Water spray status
  if (humidity < 70 && !W_active)
  {                                 // If soil is dry
    digitalWrite(W_SPRAY_PIN, LOW); // Turn on water spray (LED ON)
    Serial.println("# Water Spray: ON");
    W_active = true;
  }
  else if (humidity >= 70 && W_active)
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

  // Check if the pump has been active for 10 seconds and turn it off
  if (P_active && (currentMillis - last_pump_activation_time >= pump_on_duration))
  {
    digitalWrite(P_SPRAY_PIN, HIGH); // Turn off water pump (LED OFF)
    Serial.println("# Water Pump OFF");
    P_active = false;
  }
  // Turn on the pump if soil is dry and it hasn't been activated in the last 10 seconds
  if (!P_active && soilPercent < 30 && (currentMillis - last_pump_activation_time >= pump_cooldown))
  {
    digitalWrite(P_SPRAY_PIN, LOW); // Turn on water pump (LED ON)
    Serial.println("# Water Pump ON");
    P_active = true;
    last_pump_activation_time = currentMillis; // record the time it was turned on
  }
  Serial.println("# ----------------------------------------");

  static bool beeping = false;
  if (temperature > 28)
  {
    Serial.println("# It's too hot!");
    if (!beeping) {
      digitalWrite(BEEP_PIN, HIGH); // Turn on buzzer
      beep_start_time = currentMillis;
      beeping = true;
    }
  }
  else if (temperature < 10)
  {
    Serial.println("# It's too cold!");
    if (!beeping) {
      digitalWrite(BEEP_PIN, HIGH); // Turn on buzzer
      beep_start_time = currentMillis;
      beeping = true;
    }
  }
  else
  {
    Serial.println("# Temperature is just right.");
    digitalWrite(BEEP_PIN, LOW); // Turn off buzzer (LED OFF)
  }
  Serial.println("##########################################");
  
  // Turn off beeper after a duration
  if (beeping && (currentMillis - beep_start_time >= beep_duration)) {
    digitalWrite(BEEP_PIN, LOW);
    beeping = false;
  }

  // Publish data to MQTT every 10 seconds (non-blocking)
  if (currentMillis - last_mqtt_publish_time >= mqtt_publish_interval)
  { // Publish every 10 seconds (10000 milliseconds)
    last_mqtt_publish_time = currentMillis; // Update the last publish time
    sprintf(msg, "{\"temp\": %.2f, \"humid\": %.2f, \"soil\": %d, \"soil_percent\": %d, \"ldr\": %d}",
            temperature,
            humidity,
            soilAnalog,
            soilPercent,
            ldrValue);

    Serial.print("\nPublishing JSON to topic '");
    Serial.print(mqtt_publish_topic);
    Serial.print("': ");
    Serial.println(msg);

    client.publish(mqtt_publish_topic, msg);
  }
  }
}
