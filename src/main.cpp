#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "DHT.h"
#include <ESP32Servo.h>
#include <ArduinoJson.h>

#define DHTPIN 2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHT22);

// Variables to store last readings of DHT22
unsigned long lastRead = 0;
float lastTemp = 0;
float lastHum = 0;

// Variables for servo
Servo doorServo;
const int servoPin = 18;

// Variables for sound sensor

const int soundPin = 34;   // AO pin
const int led1 = 14;
const int led2 = 27;
const int led3 = 13;

int threshold = 1000;
unsigned long clapTimeout = 800;
unsigned long lastClapTime = 0;
int clapCount = 0;
bool lightsOn = false;

int currentSoundValue = 0; // <- will be sent to dashboard


// --- WiFi credentials ---
const char* ssid = "Baldonasa Fam 2.4G";
const char* password = "Borgoydabest_7";

// Create server on port 80
AsyncWebServer server(80);



// Handle 404
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void setup() {
    Serial.begin(115200);
    delay(500);

    // -- initialize sensors and actuators --

    // Sound + LEDs
    pinMode(led1, OUTPUT);
    pinMode(led2, OUTPUT);
    pinMode(led3, OUTPUT);
    digitalWrite(led1, LOW); 
    digitalWrite(led2, LOW);
    digitalWrite(led3, LOW);

Serial.println(">>> Sound Sensor + Clap Control Loaded <<<");

    // DHT22
    dht.begin(); //start DHT sensor
    doorServo.setPeriodHertz(50);      
    doorServo.attach(servoPin, 500, 2400);  
    doorServo.write(0);                 // Initialize to closed
    delay(300);

   

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
    if (millis() - lastRead > 2000) { // Read DHT22 output every 2 seconds
        lastTemp = dht.readTemperature();
        lastHum = dht.readHumidity();
        lastRead = millis();
    }
   

    // Handle sensor read errors
    if (isnan(lastTemp)) lastTemp = 0.00;
    if (isnan(lastHum)) lastHum = 0.00;

    String json = "{\"temperature\":";
    json += lastTemp;
    json += ",\"humidity\":";
    json += lastHum;
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/sound-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<100> data;
    data["sound"] = currentSoundValue;
    String json;
    serializeJson(data, json);
    request->send(200, "application/json", json);
});

    // ===== Servo control endpoint =====


  server.on("/door", HTTP_GET, [](AsyncWebServerRequest *request){
        if(request->hasParam("state")){
            String state = request->getParam("state")->value();
            if(state == "open"){
                doorServo.write(180); // Open door
                Serial.println("Door opened (180°)");
            } else {
                doorServo.write(0);   // Close door
                Serial.println("Door closed (0°)");
            }
            request->send(200, "text/plain", "Door moved: " + state);
        } else {
            request->send(400, "text/plain", "Missing state parameter");
        }
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

    // ===== SOUND SENSOR READ + CLAP DETECTION =====
    currentSoundValue = analogRead(soundPin);

    // Optional serial print every 200ms (not spam)
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 200) {
  Serial.print("sound=");
  Serial.println(currentSoundValue);
  lastPrint = millis();
}

    // Detect clap
    if (currentSoundValue > threshold) {
        unsigned long now = millis();
        if (now - lastClapTime > 80) {  // debounce
            clapCount++;
            Serial.print("Clap detected. Count= ");
            Serial.println(clapCount);
            lastClapTime = now;
        }
    }

    // If 2 claps -> toggle LEDs
    if (clapCount >= 2) {
        lightsOn = !lightsOn;
        digitalWrite(led1, lightsOn);
        digitalWrite(led2, lightsOn);
        digitalWrite(led3, lightsOn);
        Serial.println(lightsOn ? "LEDs ON" : "LEDs OFF");
        clapCount = 0;
        delay(400);
    }

    // Reset clap count if no second clap
    if (millis() - lastClapTime > clapTimeout) {
        clapCount = 0;
    }

    
}
