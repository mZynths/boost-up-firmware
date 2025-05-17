#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <map>
#include <functional>
#include <secrets.h>

// helper: split a String by ‘,’ and trim whitespace
static std::vector<String> splitArgs(const String& s) {
    std::vector<String> parts;
    int start = 0;

    while (start < s.length()) {
        int comma = s.indexOf(',', start);
        if (comma < 0) comma = s.length();
        String part = s.substring(start, comma);
        part.trim();
        parts.push_back(part);
        start = comma + 1;
    }

    return parts;
}

using CmdHandler = std::function<void(const String& args)>;
static std::map<String, CmdHandler> commandMap;

// ——— Pin definitions ———
#define STEPPER_A_STEP   14  // STEP pin
#define STEPPER_A_SLEEP  13  // SLEEP pin

#define STEPPER_B_STEP   12  // STEP pin
#define STEPPER_B_SLEEP  11  // SLEEP pin

#define PERISTALTIC_A_A  15  // A pin
#define PERISTALTIC_A_B   7  // B pin

#define PUMP_ENABLE 42

// ——— Global variables & constants ———
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

bool pumpState = false;
constexpr float STEPS_PER_REV = 200.0;

// Create a WebServer object
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

void onCommandOn() {
    Serial.println("Pump ON");
    digitalWrite(PUMP_ENABLE, HIGH);
}

void onCommandOff() {
    Serial.println("Pump OFF");
    digitalWrite(PUMP_ENABLE, LOW);
}

void onCommandTgl() {
    Serial.println("Pump TOGGLE");
    pumpState = !pumpState;
    digitalWrite(PUMP_ENABLE, pumpState ? HIGH : LOW);
}

// Fixed 200-step move
void onCommandStp() {
    const int steps = 200;
    const unsigned long pulseDelay = 1000;  // µs, adjust to taste
  
    Serial.println("Turning stepper 200 steps");
  
    // wake driver
    digitalWrite(STEPPER_A_SLEEP, HIGH);
    delayMicroseconds(50);
  
    for (int i = 0; i < steps; i++) {
        digitalWrite(STEPPER_A_STEP, HIGH);
        delayMicroseconds(pulseDelay);
        digitalWrite(STEPPER_A_STEP, LOW);
        delayMicroseconds(pulseDelay);
    }
  
    // sleep driver
    digitalWrite(STEPPER_A_SLEEP, LOW);
    Serial.println("Finished");
  }

void onCommandStep(int steps) {
    if (steps == 0) {
        Serial.println("Usage: step(n) where n != 0");
        return;
    }
  
    // total time for entire move—choose a default or pass as second arg if you like
    const float totalSeconds = 0.5;  // e.g. half a second for the whole move
    unsigned long interval = (unsigned long)((totalSeconds * 1e6) / (abs(steps) * 2));
  
    Serial.println("Turning stepper " + String(steps) + " steps");
  
    digitalWrite(STEPPER_A_SLEEP, HIGH);
    delayMicroseconds(50);
  
    for (int i = 0; i < abs(steps); i++) {
        digitalWrite(STEPPER_A_STEP, HIGH);
        delayMicroseconds(interval);
        digitalWrite(STEPPER_A_STEP, LOW);
        delayMicroseconds(interval);
    }
  
    digitalWrite(STEPPER_A_SLEEP, LOW);
    Serial.println("Finished");
}

// ——— onCommandSpin ———
// degrees: how far to turn (e.g. 90.0 for a quarter turn)
// seconds: how long the move should take
void onCommandSpin(float degrees, float seconds) {
    if (seconds <= 0.0f) {
        Serial.println("Error: duration must be > 0");
        return;
    }
  
    // compute how many full steps
    long totalSteps = lround((degrees / 360.0f) * STEPS_PER_REV);
    if (totalSteps == 0) {
        Serial.println("Error: degrees too small to move any steps");
        return;
    }
  
    // half-pulse interval in µs so that:
    unsigned long interval = (unsigned long)((seconds * 1e6) / (abs(totalSteps) * 2));
  
    Serial.printf(
        "Spinning %.2f° → %ld steps over %.2f s (interval %lums)\n",
        degrees, totalSteps, seconds, interval
    );
  
    digitalWrite(STEPPER_A_SLEEP, HIGH);
    delayMicroseconds(50);
  
    for (int i = 0; i < abs(totalSteps); i++) {
        digitalWrite(STEPPER_A_STEP, HIGH);
        delayMicroseconds(interval);
        digitalWrite(STEPPER_A_STEP, LOW);
        delayMicroseconds(interval);
    }
  
    digitalWrite(STEPPER_A_SLEEP, LOW);
    Serial.println("Spin complete.");
  }

void onCommandDispense(float milliseconds) {
    if (milliseconds <= 0.0f) {
        Serial.println("Error: duration must be > 0");
        return;
    }

    Serial.printf(
        "Pumping A over %.2f s (interval %lums)\n",
        milliseconds
    );

    Serial.println("Dispensing...");
    digitalWrite(PERISTALTIC_A_A, HIGH);
    delay(milliseconds);
    digitalWrite(PERISTALTIC_A_A, LOW);
    Serial.println("Dispense complete.");
}

void initCommands() {
    commandMap["on"]   = [](const String&){ onCommandOn(); };
    commandMap["off"]  = [](const String&){ onCommandOff(); };
    commandMap["tgl"]  = [](const String&){ onCommandTgl(); };
    commandMap["stp"]  = [](const String&){ onCommandStp(); };

    commandMap["step"] = [](const String& args){
      int steps = args.toInt();
      onCommandStep(steps);
    };

    commandMap["spin"] = [](const String& args){
        auto parts = splitArgs(args);

        if (parts.size() < 2) {
            Serial.println("Usage: spin(degrees,seconds)");
            return;
        }

        float degrees = parts[0].toFloat();
        float seconds = parts[1].toFloat();
        onCommandSpin(degrees, seconds);
    };

    // add new commands here!

    Serial.println("Commands initialized");
  }

// Function to handle WebSocket messages
void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (!(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT))
        return;

    data[len] = 0;
    String msg = (char*)data;
    Serial.println("Command received: " + msg);

    // parse name and args
    String name = msg;
    String args = "";
    int p = msg.indexOf('(');
    if (p >= 0) {
        name = msg.substring(0, p);
        int q = msg.indexOf(')', p+1);
        if (q > p) args = msg.substring(p+1, q);
    }

    // dispatch
    auto it = commandMap.find(name);
    if (it != commandMap.end()) {
        it->second(args);
    } else {
        Serial.println("Unknown command: " + name);
    }
}

// Function to handle WebSocket events
void onEvent(
    AsyncWebSocket *server, 
    AsyncWebSocketClient *client, 
    AwsEventType type, 
    void *arg, 
    uint8_t *data, 
    size_t len
) {
    switch (type) {
    case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        break;

    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;

    case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
        
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
        break;
    }
}

// Initialize WiFi
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi ..");

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print('.');
        delay(1000);
    }

    Serial.println("");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

// Initialize WebSocket
void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    Serial.println("WebSocket initialized");
}

// Initialize pins
void initPins() {
    // Pump
    pinMode(PUMP_ENABLE, OUTPUT);
    digitalWrite(PUMP_ENABLE, LOW);      // Set the pump to LOW initially

    // Stepper A
    pinMode(STEPPER_A_STEP, OUTPUT);
    digitalWrite(STEPPER_A_STEP, LOW);   // Set the stepper to LOW initially
    
    pinMode(STEPPER_A_SLEEP, OUTPUT);
    digitalWrite(STEPPER_A_SLEEP, LOW);  // Set the stepper to LOW initially

    // Stepper B
    pinMode(STEPPER_B_STEP, OUTPUT);
    digitalWrite(STEPPER_B_STEP, LOW);   // Set the stepper to LOW initially
    
    pinMode(STEPPER_B_SLEEP, OUTPUT);
    digitalWrite(STEPPER_B_SLEEP, LOW);  // Set the stepper to LOW initially

    // Peristaltic A
    pinMode(PERISTALTIC_A_A, OUTPUT);
    pinMode(PERISTALTIC_A_B, OUTPUT);
    digitalWrite(PERISTALTIC_A_A, LOW);  // Set the peristaltic to LOW initially
    digitalWrite(PERISTALTIC_A_B, LOW);  // Set the peristaltic to LOW initially
}

void setup() {
    Serial.begin(115200);

    initPins();
    initWiFi();
    initWebSocket();
    initCommands();

    // Start server
    server.begin(); // Not entirely sure if the WS Server needs this to run
}

void loop() {
    // Do something:
    delay(500);


    // At the end:
    ws.cleanupClients();
}