#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <functional>
#include <secrets.h>
#include <Pump.h>
#include <StepperPowderDispenser.h>

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
#define PERISTALTIC_A  46
#define PERISTALTIC_B   9 
#define PERISTALTIC_C  10
#define WATER_PUMP      2

#define STEPPER_A_STEP   14  // STEP pin   14
#define STEPPER_A_SLEEP  13  // SLEEP pin  13
#define STEPPER_A_DIR     5  // DIR pin     5 

// ——— Some other pins (migrating to objects) ———
#define STEPPER_B_STEP   12  // STEP pin   12
#define STEPPER_B_SLEEP  11  // SLEEP pin  11
#define STEPPER_B_DIR     6  // DIR pin     6

#define TUMERIC_A 35
#define TUMERIC_B 36

// ——— Global variables & constants ———
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Create a WebServer object
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Stepper objects
StepperPowderDispenser proteina(
    "Leche nido XD",
    STEPPER_A_STEP,
    STEPPER_A_SLEEP,
    STEPPER_A_DIR,
    false, // dispense clockwise
    87.13371302,   // steps per gram
    3000,  // step interval in microseconds
    3000,  // pulse duration in microseconds
    200,   // steps per revolution
    1000,  // vibration step interval in microseconds
    100,   // vibration pulse duration in microseconds
    500    // steps per vibration
);

// Pump objects
static std::map<String, Pump*> fluidToPumpMap;
Pump chocolate("Jarabe de Chocolate", PERISTALTIC_A, 1.0f);
Pump caramelo("Jarabe de Caramelo", PERISTALTIC_B, 1.0f);
Pump vainilla("Jarabe de Vainilla", PERISTALTIC_C, 1.0f);
Pump agua("Agua", WATER_PUMP, 1.0f);

void onCommandDispenseFluid(Pump* pump, float milliliters) {
    if (milliliters <= 0.0f) {
        Serial.println("Error: duration must be > 0");
        return;
    }

    Serial.printf(
        "Pumping %.2f mL of %s\n",
        milliliters,
        pump->getFluidName().c_str()
    );

    pump->enable();
    pump->dispense(milliliters);
    
    while (pump->isDispensing()) {
        pump->update();
    }
    
    pump->disable();
}

void onCommandRunPump(Pump* pump, float milliseconds) {
    if (milliseconds <= 0.0f) {
        Serial.println("Error: duration must be > 0");
        return;
    }

    Serial.printf(
        "Dispensing %s over %.2f ms\n",
        pump->getFluidName().c_str(),
        milliseconds
    );

    pump->enable();
    pump->spin(milliseconds);

    while (pump->isDispensing()) {
        pump->update();
    }

    pump->disable();
}

void onCommandPumpSetCalibration(Pump* pump, float milliliters, float milliseconds) {
    if (milliliters <= 0.0f) {
        Serial.println("Error: duration must be > 0");
        return;
    }
    
    if (milliseconds <= 0.0f) {
        Serial.println("Error: duration must be > 0");
        return;
    }

    pump->set_calibration(milliseconds, milliliters);

    Serial.printf(
        "Set calibration for %s: %.2f mL over %.2f ms\n",
        pump->getFluidName().c_str(),
        milliliters,
        milliseconds
    );
}

void onCommandBlink(int times) {
    if (times <= 0) {
        Serial.println("Error: times must be > 0");
        return;
    }

    Serial.printf("Blinking LED %d times\n", times);
    
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }

    Serial.println("Done blinking");
}

void initCommands() {
    fluidToPumpMap["chocolate"] = &chocolate;
    fluidToPumpMap["caramelo"]  = &caramelo;
    fluidToPumpMap["vainilla"]  = &vainilla;
    fluidToPumpMap["agua"]      = &agua;

    commandMap["blink"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 1) {
            Serial.println("Usage: blink(times)");
            return;
        }

        int times = parts[0].toInt();
        onCommandBlink(times);
    };

    commandMap["powderDisp"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 1) {
            Serial.println("Usage: powderDisp(grams)");
            return;
        }

        float grams = parts[0].toFloat();

        proteina.enable();
        proteina.dispense(grams);
    };

    commandMap["spin"] = [](const String&){
        proteina.enable();
        proteina.spin(100);
    };
    
    commandMap["vib"] = [](const String&){
        proteina.enable();
        proteina.vibrate();
    };

    commandMap["dis"] = [](const String&){
        proteina.disable();
    };

    commandMap["en"] = [](const String&){
        proteina.enable();
    };

    commandMap["fluidPump"] = [](const String& args){
        auto parts = splitArgs(args);

        if (parts.size() < 2) {
            Serial.println("Usage: fluidPump(fluidAlias,milliliters)");
            return;
        }

        String fluid = parts[0];
        float milliliters = parts[1].toFloat();

        auto it = fluidToPumpMap.find(fluid);
        
        if (it == fluidToPumpMap.end()) {
            Serial.println("Error: unknown fluid " + fluid);
            return;
        }

        onCommandDispenseFluid(it->second, milliliters);
    };

    commandMap["fluidSpin"] = [](const String& args){
        auto parts = splitArgs(args);

        if (parts.size() < 2) {
            Serial.println("Usage: fluidSpin(fluidAlias,milliseconds)");
            return;
        }

        String fluid = parts[0];
        float milliseconds = parts[1].toFloat();

        auto it = fluidToPumpMap.find(fluid);
        
        if (it == fluidToPumpMap.end()) {
            Serial.println("Error: unknown fluid " + fluid);
            return;
        }

        onCommandRunPump(it->second, milliseconds);
    };

    commandMap["fluidSetCalibration"] = [](const String& args){
        auto parts = splitArgs(args);

        if (parts.size() < 3) {
            Serial.println("Usage: fluidSetCalibration(fluidAlias,milliliters,milliseconds)");
            return;
        }

        String fluid = parts[0];
        float milliliters = parts[1].toFloat();
        float milliseconds = parts[2].toFloat();

        auto it = fluidToPumpMap.find(fluid);
        
        if (it == fluidToPumpMap.end()) {
            Serial.println("Error: unknown fluid " + fluid);
            return;
        }

        onCommandPumpSetCalibration(it->second, milliliters, milliseconds);
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
    // Stepper A
    // pinMode(STEPPER_A_DIR, OUTPUT);
    // digitalWrite(STEPPER_A_DIR, LOW);   // Set the stepper to LOW initially

    // pinMode(STEPPER_A_STEP, OUTPUT);
    // digitalWrite(STEPPER_A_STEP, LOW);   // Set the stepper to LOW initially
    
    // pinMode(STEPPER_A_SLEEP, OUTPUT);
    // digitalWrite(STEPPER_A_SLEEP, LOW);  // Set the stepper to LOW initially
    
    // Stepper B
    pinMode(STEPPER_B_DIR, OUTPUT);
    digitalWrite(STEPPER_B_DIR, LOW);   // Set the stepper to LOW initially

    pinMode(STEPPER_B_STEP, OUTPUT);
    digitalWrite(STEPPER_B_STEP, LOW);   // Set the stepper to LOW initially
    
    pinMode(STEPPER_B_SLEEP, OUTPUT);
    digitalWrite(STEPPER_B_SLEEP, LOW);  // Set the stepper to LOW initially

    // Tumeric
    pinMode(TUMERIC_A, OUTPUT);
    digitalWrite(TUMERIC_A, LOW);   // Set the stepper to LOW initially
    pinMode(TUMERIC_B, OUTPUT);
    digitalWrite(TUMERIC_B, LOW);   // Set the stepper to LOW initially
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
    // delay(500);

    // At the end:
    ws.cleanupClients();

    chocolate.update();
    caramelo.update();
    vainilla.update();
    agua.update();

    proteina.update();
}