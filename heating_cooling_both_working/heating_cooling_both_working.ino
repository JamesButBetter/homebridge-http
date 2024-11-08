#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>

// Network credentials
const char* ssid = "PLUSNET-2JC93C";
const char* password = "KNHgRaVJE3KmTK";

// GPIO pins
#define ONE_WIRE_BUS D2
#define CONTROL_PIN_HEATING_DOWN D4
#define CONTROL_PIN_COOLING_DOWN D6
#define CONTROL_PIN_HEATING_UP D5
#define CONTROL_PIN_COOLING_UP D7

// Upstairs Arduino IP address and port
const char* upstairsIPAddress = "192.168.1.179";
const int upstairsPort = 80;

// Server port
const int serverPort = 2000;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(serverPort);

float upstairsTemperature = 0.0;

// Variables to hold the target and current states
struct TempControl {
  int targetHeatingCoolingState = 0;
  float targetTemperature = 22.0;
  int currentHeatingCoolingState = 0;
  float currentTemperature = 0.0;
};

TempControl down, up;
SemaphoreHandle_t xMutex;
TaskHandle_t fetchTemperatureTaskHandle = NULL;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  sensors.begin();

  pinMode(CONTROL_PIN_HEATING_DOWN, OUTPUT);
  pinMode(CONTROL_PIN_HEATING_UP, OUTPUT);
  pinMode(CONTROL_PIN_COOLING_DOWN, OUTPUT);
  pinMode(CONTROL_PIN_COOLING_UP, OUTPUT);

  server.on("/downstairs/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleStatus(request, down);
  });
  server.on("/downstairs/targetHeatingCoolingState", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleSetTargetHeatingCoolingState(request, down);
  });
  server.on("/downstairs/targetTemperature", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleSetTargetTemperature(request, down);
  });

  server.on("/upstairs/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleStatus(request, up);
  });
  server.on("/upstairs/targetHeatingCoolingState", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleSetTargetHeatingCoolingState(request, up);
  });
  server.on("/upstairs/targetTemperature", HTTP_GET, [](AsyncWebServerRequest* request) {
    handleSetTargetTemperature(request, up);
  });

  server.begin();
  Serial.printf("HTTP server started on port %d\n", serverPort);

  xMutex = xSemaphoreCreateMutex();

  // Create a task for fetching upstairs temperature
  xTaskCreate(fetchUpstairsTemperatureTask, "FetchTempTask", 4096, NULL, 1, &fetchTemperatureTaskHandle);
}

void loop() {
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    sensors.requestTemperatures();
    down.currentTemperature = sensors.getTempCByIndex(0);
    updateHeatingCoolingState(down, CONTROL_PIN_HEATING_DOWN, CONTROL_PIN_COOLING_DOWN);
    updateHeatingCoolingState(up, CONTROL_PIN_HEATING_UP, CONTROL_PIN_COOLING_UP);
    xSemaphoreGive(xMutex);
  }
}

void fetchUpstairsTemperatureTask(void* parameter) {
  for (;;) {
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      fetchUpstairsTemperature();
      xSemaphoreGive(xMutex);
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);  // Wait for 5 seconds before next fetch
  }
}

void fetchUpstairsTemperature() {
  HTTPClient http;
  String url = "http://" + String(upstairsIPAddress) + ":" + String(upstairsPort) + "/temperature";
  http.begin(url);
  http.setTimeout(2000);  // Set timeout to 2 seconds

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("Failed to parse JSON, error: ");
      Serial.println(error.c_str());
    } else {
      upstairsTemperature = doc["currentTemperature"].as<float>();
      up.currentTemperature = upstairsTemperature;
    }
  } else {
    Serial.printf("Failed to fetch upstairs temperature, HTTP code: %d\n", httpCode);
  }

  http.end();
}

void handleStatus(AsyncWebServerRequest* request, TempControl& control) {
  if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
    StaticJsonDocument<200> doc;
    doc["targetHeatingCoolingState"] = control.targetHeatingCoolingState;
    doc["targetTemperature"] = control.targetTemperature;
    doc["currentHeatingCoolingState"] = control.currentHeatingCoolingState;
    doc["currentTemperature"] = control.currentTemperature;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
    xSemaphoreGive(xMutex);
  }
}

void handleSetTargetHeatingCoolingState(AsyncWebServerRequest* request, TempControl& control) {
  if (request->hasParam("value")) {
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      control.targetHeatingCoolingState = request->getParam("value")->value().toInt();
      request->send(200, "text/plain", "TargetHeatingCoolingState set");
      xSemaphoreGive(xMutex);
    }
  } else {
    request->send(400, "text/plain", "Missing value");
  }
}

void handleSetTargetTemperature(AsyncWebServerRequest* request, TempControl& control) {
  if (request->hasParam("value")) {
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      control.targetTemperature = request->getParam("value")->value().toFloat();
      request->send(200, "text/plain", "TargetTemperature set");
      xSemaphoreGive(xMutex);
    }
  } else {
    request->send(400, "text/plain", "Missing value");
  }
}

void updateHeatingCoolingState(TempControl& control, int heatingPin, int coolingPin) {
  switch (control.targetHeatingCoolingState) {
    case 0:  // Off
      digitalWrite(heatingPin, LOW);
      digitalWrite(coolingPin, LOW);
      control.currentHeatingCoolingState = 0;
      break;
    case 1:  // Heat
      digitalWrite(coolingPin, LOW);
      if (control.currentTemperature < control.targetTemperature) {
        digitalWrite(heatingPin, HIGH);
        control.currentHeatingCoolingState = 1;
      } else {
        digitalWrite(heatingPin, LOW);
        control.currentHeatingCoolingState = 0;
      }
      break;
    case 2:  // Cool
      digitalWrite(heatingPin, LOW);
      if (control.currentTemperature > control.targetTemperature) {
        digitalWrite(coolingPin, HIGH);
        control.currentHeatingCoolingState = 2;
      } else {
        digitalWrite(coolingPin, LOW);
        control.currentHeatingCoolingState = 0;
      }
      break;
    case 3:  // Auto
      if (control.currentTemperature < control.targetTemperature - 0.5) {
        digitalWrite(heatingPin, HIGH);
        digitalWrite(coolingPin, LOW);
        control.currentHeatingCoolingState = 1;
      } else if (control.currentTemperature > control.targetTemperature + 0.5) {
        digitalWrite(heatingPin, LOW);
        digitalWrite(coolingPin, HIGH);
        control.currentHeatingCoolingState = 2;
      } else {
        digitalWrite(heatingPin, LOW);
        digitalWrite(coolingPin, LOW);
        control.currentHeatingCoolingState = 0;
      }
      break;
  }
}
