#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "DHT.h"
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHT22);

#define TRIG_PIN 17 //for ultrasoninc
#define ECHO_PIN 16

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
bool clapDetected = false;  // flag for clap detector in dashboard

//LDR variables

const int ldrPin = 33;        
const int ldrLed1 = 19;
const int ldrLed2 = 5;
const int ldrLed3 = 4;
int ldrThreshold = 2000;       
int currentLdrValue = 0;       // <- will be sent to dashboard

/// states for manual led controls
int ldrMode = 0;  // 0 = auto mode, 1 = manual on, 2 = manua off
int  soundMode = 0; 


// LCD variables
bool showingGreeting = false;
unsigned long greetingStart = 0;
LiquidCrystal_PCF8574 lcd(0x27);

// Ultrasonic variables
unsigned long lastUltrasonicRead = 0;
float lastDistance = -1;





// --- WiFi credentials ---
const char* ssid = "Baldonasa Fam 2.4G";
const char* password = "Borgoydabest_7";

// Create server on port 80
AsyncWebServer server(80);



// Handle 404
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

float getDistanceCM() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000); //30 ms 5 meters max distance
    if (duration == 0) return -1; // No echo received
    
    float distance = duration * 0.034 / 2;
    return distance;
}

void showGreetingScreen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" [GREETINGS]");
    lcd.setCursor(0, 1);
    lcd.print(" Welcome Kuya!");
}

void showDefaultScreen() {
    
    // Line 1: Room LED status
    lcd.setCursor(0, 0);
    lcd.print("R1:");
    lcd.print(digitalRead(ldrLed1) ? "ON " : "OFF");
    lcd.print(" R2:");
    lcd.print(digitalRead(led1) ? "ON" : "OFF");
    
    // Line 2: Temperature & Humidity
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(lastTemp, 1);
    lcd.print("C H:");
    lcd.print(lastHum, 1);
    lcd.print("%");
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

    //LDR
    pinMode(ldrLed1, OUTPUT);
    pinMode(ldrLed2, OUTPUT);
    pinMode(ldrLed3, OUTPUT);
    pinMode(ldrPin, INPUT);

    Serial.println(">>> LDR Loaded <<<");

    // Ultrasonic sensor
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    Serial.println(">>> Ultrasonic Loaded <<<");

    // DHT22
    dht.begin(); //start DHT sensor
    doorServo.setPeriodHertz(50);      
    doorServo.attach(servoPin, 500, 2400);  
    doorServo.write(0);                 // Initialize to closed
    delay(300);

    // Initialize LCD
    Wire.begin();
    lcd.begin(16, 2);
    lcd.setBacklight(255);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Ready");
    delay(1000);
    lcd.clear();



   

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

  server.on("/ldr-data", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<100> data;
    data["light"] = currentLdrValue;
    String json;
    serializeJson(data, json);
    request->send(200, "application/json", json);
});

  server.on("/sound-data", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<100> data;
    data["sound"] = currentSoundValue;
    data["clapDetected"] = clapDetected ? "Clap Detected!" : "Detecting...";
    String json;
    serializeJson(data, json);
    request->send(200, "application/json", json);
});

// ===== LED status endpoint =====

server.on("/led-status", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<100> data;

    // LDR LEDs: check one of them since they are all synced
    data["ldr"] = digitalRead(ldrLed1) == HIGH ? "ON" : "OFF";

    // Sound LEDs: check one of them
    data["sound"] = digitalRead(led1) == HIGH ? "ON" : "OFF";

    String json;
    serializeJson(data, json);
    request->send(200, "application/json", json);
});

server.on("/room1", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("state")){
        String state = request->getParam("state")->value();
        if (state == "on") ldrMode = 1;
        else if (state == "off") ldrMode = 2;
        else ldrMode = 0;
         request->send(200, "text/plain", "Room1 updated");
    } 
    else {
        request->send(400, "text/plain", "Missing state parameter");
    }
});

server.on("/room2", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("state")){
        String state = request->getParam("state")->value();
        if (state == "on") soundMode = 1;
        else if (state == "off") soundMode = 2;
        else soundMode = 0;

        request->send(200, "text/plain", "Room2 updated");
    } else {
        request->send(400, "text/plain", "Missing state parameter");
    }
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
    unsigned long now = millis();

    //DHT
    if (millis() - lastRead > 2000) { // Read DHT22 output every 2 seconds
        lastTemp = dht.readTemperature();
        lastHum = dht.readHumidity();
        lastRead = millis();
    }       

    // --- SOUND SENSOR + CLAP ---
    currentSoundValue = analogRead(soundPin);

    static unsigned long lastSoundPrint = 0;
    if (now - lastSoundPrint > 200) {
        //Serial.print("sound=");
        //Serial.println(currentSoundValue);
        lastSoundPrint = now;
    }

    if (currentSoundValue > threshold && now - lastClapTime > 80) {
        clapCount++;
        Serial.print("Clap detected. Count= ");
        Serial.println(clapCount);
        lastClapTime = now;
        clapDetected = true;
    }

    if (clapDetected && now - lastClapTime > 500) {
        clapDetected = false;
    }

    static bool ledState = digitalRead(led1); // track LED state from claps

    if (clapCount >= 2) {
        ledState = !ledState; // toggle due to claps
        clapCount = 0;
    }

    bool sensorSoundState = ledState;
    bool finalSoundState;
    if (soundMode == 1) finalSoundState = true;        // FORCE ON
    else if (soundMode == 2) finalSoundState = false;  // FORCE OFF
    else finalSoundState = sensorSoundState;           // AUTO
    digitalWrite(led1, finalSoundState);
    digitalWrite(led2, finalSoundState);
    digitalWrite(led3, finalSoundState);
    ledState = finalSoundState;


    if (now - lastClapTime > clapTimeout) {
        clapCount = 0;
    }

    // --- LDR ---
    currentLdrValue = analogRead(ldrPin);

    static unsigned long lastLdrPrint = 0;
    if (now - lastLdrPrint > 200) {
        //Serial.print("LDR Value=");
        //Serial.println(currentLdrValue);
        lastLdrPrint = now;
    }

    bool sensorLdrState = currentLdrValue > ldrThreshold;
    bool finalLdrState;
    if (ldrMode == 1) finalLdrState = true;       // FORCE ON
    else if (ldrMode == 2) finalLdrState = false; // FORCE OFF
    else finalLdrState = sensorLdrState;          // AUTO

    digitalWrite(ldrLed1, finalLdrState);
    digitalWrite(ldrLed2, finalLdrState);
    digitalWrite(ldrLed3, finalLdrState);


    // --- ULTRASONIC + LCD ---
    // Read ultrasonic sensor every 150ms to avoid blocking
    if (now - lastUltrasonicRead >= 500) {
        lastUltrasonicRead = now;
        lastDistance = getDistanceCM();
        
        if (lastDistance > 0) {
            Serial.print("Distance: ");
            Serial.print(lastDistance);
            Serial.println(" cm");
        }
    }
    
    // Greeting trigger
    if (lastDistance > 0 && lastDistance < 10 && !showingGreeting) {
        showingGreeting = true;
        greetingStart = millis();
        showGreetingScreen();
    }
    
    // Handle greeting display
    if (showingGreeting) {
        if (millis() - greetingStart >= 5000) {
            showingGreeting = false;
            lcd.clear();
            showDefaultScreen(); 
        }
    } else {
        // Update default screen every 3 seconds
        static unsigned long lastLCDUpdate = 0;
        if (millis() - lastLCDUpdate >= 5000) {
            lastLCDUpdate = millis();
            
            // Update DHT readings for LCD
            if (millis() - lastRead > 2000) {
                lastTemp = dht.readTemperature();
                lastHum = dht.readHumidity();
                lastRead = millis();
                
                if (isnan(lastTemp)) lastTemp = 0.00;
                if (isnan(lastHum)) lastHum = 0.00;
            }
            
            showDefaultScreen();
        }
    }
    delay(1);
}
