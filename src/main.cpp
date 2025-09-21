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

#define W_SPRAY_PIN 33 // GPIO pin connected to the onboard LED
#define P_SPRAY_PIN 25 // GPIO pin connected to the onboard LED

static unsigned long when_activated; // Declare as static to retain value across loop calls
static unsigned long MQTT_activated; // Declare as static to retain value across loop calls

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

  when_activated = millis() / 1000;
  MQTT_activated = millis() / 1000;

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
}

void loop()
{
  delay(1000); // Read Every 1 Sec
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

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
  if (soilPercent < 50 && W_active == false)
  {                                 // If soil is dry
    digitalWrite(W_SPRAY_PIN, LOW); // Turn on water spray (LED ON)
    Serial.println("# Water Spray: ON");
    W_active = true;
  }
  else if (soilPercent >= 50 && W_active == true)
  {
    digitalWrite(W_SPRAY_PIN, LOW);  // Turn off water spray (LED OFF)
    delay(100);                      // Small delay to ensure the LED state is set before printing
    digitalWrite(W_SPRAY_PIN, HIGH); // Turn off water spray (LED OFF)
    delay(100);                      // Small delay to ensure the LED state is set before printing
    digitalWrite(W_SPRAY_PIN, LOW);  // Turn off water spray (LED OFF)
    Serial.println("# Water Spray: OFF");
    W_active = false;
  }
  delay(100);                      // Small delay to ensure the LED state is set before printing
  digitalWrite(W_SPRAY_PIN, HIGH); // Ensure water spray is off after check
  Serial.println("Status: Default");
  Serial.println("----------------------------------------");

  static bool P_active = false; // Water pump status
  if ((millis() / 1000) - when_activated >= 1 && P_active == true)
  {                                 // If 1 second has passed
    digitalWrite(P_SPRAY_PIN, HIGH); // Turn off water pump (LED OFF)
    Serial.println("# Water Pump OFF");
    P_active = false;
  }
  if (soilPercent < 50 && P_active == false && (millis() / 1000) - when_activated >= 10)
  {                                 // If soil is dry and at least 10 seconds have passed since last activation
    digitalWrite(P_SPRAY_PIN, LOW); // Turn on water pump (LED ON)
    Serial.println("# Water Pump ON");
    P_active = true;
    when_activated = (millis() / 1000); // get current time
  }
  Serial.println("##########################################");

  //
  if ((millis() / 1000) - MQTT_activated >= 5)
  { // Publish every 5 seconds
    sprintf(msg, "{\"temp\": %.2f, \"humid\": %.2f, \"soil\": %d, \"soil_percent\": %d, \"soil_digital\": %d, \"ldr\": %d}",
            temperature,
            humidity,
            soilAnalog,
            soilPercent,
            soilDigital,
            ldrValue);

    Serial.print("Publishing JSON to topic '");
    Serial.print(mqtt_publish_topic);
    Serial.print("': ");
    Serial.println(msg);

    client.publish(mqtt_publish_topic, msg);
    MQTT_activated = (millis() / 1000); // get current time
  }
}
