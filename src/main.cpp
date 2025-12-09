#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "DHT.h"

#define DHTPIN 2
#define DHTTPE DHT22
DHT dht(DHTPIN, DHT22);


// --- WiFi credentials ---
const char* ssid = "VivoV19Neo";
const char* password = "password";

// Create server on port 80
AsyncWebServer server(80);

// Handle 404
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 STARTUP ===");

    // --- Mount LittleFS 
    Serial.println("Mounting LittleFS...");
    if (!LittleFS.begin(false)) { // false = means do not format if mount fails
    Serial.println("LittleFS mount failed!");
} else {
    Serial.println("LittleFS mounted successfully.");
}


    // --- Connect to WiFi ---
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.printf("Connecting to WiFi '%s' ...\n", ssid);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if (millis() - start > 20000) { // 20s timeout
            Serial.println("\nWiFi connection timed out.");
            break;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nConnected! IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("No WiFi — server will still run (AP fallback could be added).");
    }

    // ===== Sensor endpoint =====
  server.on("/sensor-data", HTTP_GET, [](AsyncWebServerRequest *request){
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    // Handle sensor read errors
    if (isnan(temp)) temp = 0.00;
    if (isnan(hum)) hum = 0.00;

    String json = "{\"temperature\":";
    json += temp;
    json += ",\"humidity\":";
    json += hum;
    json += "}";
    request->send(200, "application/json", json);
  });

    // --- Setup HTTP server ---
    Serial.println("Setting up HTTP server...");

    // Serve all files from LittleFS root
    server.serveStatic("/", LittleFS, "/").setDefaultFile("/login/login.html");

    // Serve login pages + images
    server.serveStatic("/login/", LittleFS, "/login/");
    server.serveStatic("/login/images/", LittleFS, "/login/images/");

    // Route for dashboard
    server.on("/dashboard/dashboard.html", HTTP_GET, [](AsyncWebServerRequest *request){
        if (LittleFS.exists("/dashboard/dashboard.html")) {
            request->send(LittleFS, "/dashboard/dashboard.html", "text/html");
        } else {
            request->send(404, "text/plain", "Dashboard not found");
        }
    });

    server.onNotFound(notFound);
    server.begin();
    Serial.println("HTTP server started.");
    Serial.println("=== SETUP COMPLETE ===");


  
}

void loop() {
    
}
