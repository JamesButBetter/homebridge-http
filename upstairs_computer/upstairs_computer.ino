#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ESPping.h>
#include <ArduinoJson.h>

// Replace with your network credentials
const char* ssid = "YOUR-SSID";
const char* password = "YOUR-PASSWORD";

// Pins
#define ONE_WIRE_BUS D0
#define POWER_PIN D7
#define RESET_PIN D8

// Temperature sensor setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
float currentTemperature = 0.0;
unsigned long lastTempRequest = 0;
const unsigned long tempRequestInterval = 5000;  // 5 seconds

// Computer monitoring setup
IPAddress computerIP(192, 168, 1, 248);  // IP of the computer to ping
bool computerOn = false;  // Store the computer status
unsigned long lastPingCheck = 0;
const unsigned long pingInterval = 10000;  // 10 seconds

// Web server
AsyncWebServer server(80);

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

  // Initialize pins
  pinMode(POWER_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  digitalWrite(RESET_PIN, LOW);

  // Endpoint: Get temperature
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest* request) {
    StaticJsonDocument<200> doc;
    doc["currentTemperature"] = currentTemperature;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });

  // Endpoint: Power on the computer
  server.on("/power/on", HTTP_GET, [](AsyncWebServerRequest* request) {
    digitalWrite(POWER_PIN, HIGH);
    delay(300);
    digitalWrite(POWER_PIN, LOW);
    request->send(200, "text/plain", "Power pin set updated");
  });

  // Endpoint: Reset the computer
  server.on("/reset/on", HTTP_GET, [](AsyncWebServerRequest* request) {
    digitalWrite(RESET_PIN, HIGH);
    delay(300);
    digitalWrite(RESET_PIN, LOW);
    request->send(200, "text/plain", "Reset pin set updated");
  });

  // Endpoint: Shut down the computer
  server.on("/power/off", HTTP_GET, [](AsyncWebServerRequest* request) {
    HTTPClient http;
    http.begin("http://192.168.1.248:8081/shutdown");
    int httpResponseCode = http.GET();
    http.end();

    if (httpResponseCode == 200) {
      request->send(200, "text/plain", "Computer is shutting down");
    } else {
      request->send(500, "text/plain", "Failed to shut down computer");
    }
  });

  // Endpoint: Put the computer to sleep
  server.on("/power/sleep", HTTP_GET, [](AsyncWebServerRequest* request) {
    HTTPClient http;
    http.begin("http://192.168.1.248:8081/sleep");
    int httpResponseCode = http.GET();
    http.end();

    if (httpResponseCode == 200) {
      request->send(200, "text/plain", "Computer is going to sleep");
    } else {
      request->send(500, "text/plain", "Failed to put computer to sleep");
    }
  });

  // Endpoint: Get computer status
  server.on("/computer/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", computerOn ? "1" : "0");
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
    Serial.print("Current temperature: ");
    Serial.println(currentTemperature);
  }

  // Ping the computer at intervals
  if (currentMillis - lastPingCheck >= pingInterval) {
    computerOn = Ping.ping(computerIP);
    lastPingCheck = currentMillis;
    Serial.print("Computer status: ");
    Serial.println(computerOn ? "On" : "Off");
  }
}
