#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Replace with your network credentials
const char* ssid = "YOUR-SSID";
const char* password = "YOUR-PASSWORD";

// Data wire is connected to GPIO2 (D2)
#define ONE_WIRE_BUS D0
#define POWER_PIN D7  // Define the pin for power control (D7)
#define RESET_PIN D8  // Define the pin for reset control (D8)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);

float currentTemperature = 0.0;
unsigned long lastTempRequest = 0;
const unsigned long tempRequestInterval = 5000;  // 5 seconds

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  sensors.begin();

  pinMode(POWER_PIN, OUTPUT);  // Initialize the power control pin
  pinMode(RESET_PIN, OUTPUT);  // Initialize the power control pin
  digitalWrite(POWER_PIN, LOW);  // Ensure the pin is low initially
  digitalWrite(RESET_PIN, LOW);  // Ensure the pin is low initially

  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<200> doc;
    doc["currentTemperature"] = currentTemperature;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });

  server.on("/power/on", HTTP_GET, [](AsyncWebServerRequest* request) {
    digitalWrite(POWER_PIN, HIGH);  // Set the pin high
    delay(300);  // Wait
    digitalWrite(POWER_PIN, LOW);  // Set the pin low again

    request->send(200, "text/plain", "Power pin set updated");
  });

  server.on("/reset/on", HTTP_GET, [](AsyncWebServerRequest* request) {
    digitalWrite(RESET_PIN, HIGH);  // Set the pin high
    delay(300);  // Wait
    digitalWrite(RESET_PIN, LOW);  // Set the pin low again

    request->send(200, "text/plain", "Reset pin set updated");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  unsigned long currentMillis = millis();

  // Read temperature at intervals
  if (currentMillis - lastTempRequest >= tempRequestInterval) {
    sensors.requestTemperatures();
    currentTemperature = sensors.getTempCByIndex(0);
    lastTempRequest = currentMillis;
  }
}
