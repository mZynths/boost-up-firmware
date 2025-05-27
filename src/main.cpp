#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <functional>
#include <secrets.h>
#include <Pump.h>
#include <StepperPowderDispenser.h>
#include <ESPmDNS.h>
#include <Adafruit_Sensor.h>

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
#define STEPPER_B_STEP    15  // STEP pin   12
#define STEPPER_B_SLEEP    7  // SLEEP pin  11
#define STEPPER_B_DIR      6  // DIR pin     6

#define TUMERIC_A 35
#define TUMERIC_B 36

// ——— Global variables & constants ———
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
#define SERVICE_PORT 80

// Create a WebServer object
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Stepper objects
static std::map<String, StepperPowderDispenser*> proteinToDispenserMap;
StepperPowderDispenser proteina(
    "Pure Health",
    STEPPER_B_STEP,
    STEPPER_B_SLEEP,
    STEPPER_B_DIR,
    false, // dispense clockwise
    87.13371302,   // steps per gram
    3000,  // step interval in microseconds
    3000,  // pulse duration in microseconds
    200,   // steps per revolution
    1000,  // vibration step interval in microseconds
    100,   // vibration pulse duration in microseconds
    500    // steps per vibration
);
StepperPowderDispenser nido(
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

// Debuging commands
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

// Pump commands
void onCommandPumpFluid(Pump* pump, float milliliters) {
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

void onCommandFluidSpin(Pump* pump, float milliseconds) {
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

void onCommandFluidSetMlPerSecond(Pump* pump, float millilitersPerSecond) {
    if (millilitersPerSecond <= 0.0f) {
        Serial.println("Error: millilitersPerSecond must be > 0");
        return;
    }

    pump->set_calibration(millilitersPerSecond);
    Serial.printf(
        "Set %s calibration to %.2f mL/s\n",
        pump->getFluidName().c_str(),
        millilitersPerSecond
    );
}

// Powder dispenser commands
void onCommandDispenserSpin(StepperPowderDispenser* dispenser, int steps) {
    if (steps <= 0) {
        Serial.println("Error: steps must be > 0");
        return;
    }

    Serial.printf("Spinning %s for %d steps\n", dispenser->getPowderName().c_str(), steps);
    
    dispenser->enable();
    dispenser->spin(steps);
    
    while (dispenser->isDispensing()) {
        dispenser->update();
    }
    
    dispenser->disable();
}

void onCommandDispensePowder(StepperPowderDispenser* dispenser, float grams) {
    if (grams <= 0.0f) {
        Serial.println("Error: grams must be > 0");
        return;
    }

    Serial.printf("Dispensing %.2f grams of %s\n", grams, dispenser->getPowderName().c_str());
    
    dispenser->enable();
    dispenser->dispense(grams);
    
    while (dispenser->isDispensing()) {
        dispenser->update();
    }
    
    dispenser->disable();
}

void onCommandCalibrateDispenser(StepperPowderDispenser* dispenser, int steps, float grams) {
    if (steps <= 0) {
        Serial.println("Error: steps must be > 0");
        return;
    }

    if (grams <= 0.0f) {
        Serial.println("Error: grams must be > 0");
        return;
    }

    dispenser->calibrate(steps, grams);
    
    Serial.printf(
        "Calibrated %s: %d steps for %.2f grams\n",
        dispenser->getPowderName().c_str(),
        steps,
        grams
    );
}

void onCommandReadHumidity() {

}

// Initialize commands and their handlers
void initCommands() {
    fluidToPumpMap["1"] = &chocolate;
    fluidToPumpMap["2"]  = &caramelo;
    fluidToPumpMap["3"]  = &vainilla;
    fluidToPumpMap["a"]      = &agua;

    proteinToDispenserMap["1"] = &proteina;
    proteinToDispenserMap["2"] = &nido;

    // Debuging commands
    commandMap["blink"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 1) {
            Serial.println("Usage: blink(times)");
            return;
        }

        int times = parts[0].toInt();
        onCommandBlink(times);
    };

    // Pump commands
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

        onCommandPumpFluid(it->second, milliliters);
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

        onCommandFluidSpin(it->second, milliseconds);
    };

    commandMap["fluidSetmlPerSecond"] = [](const String& args){
        auto parts = splitArgs(args);

        if (parts.size() < 2) {
            Serial.println("Usage: fluidSetmlPerSecond(fluidAlias,millilitersPerSecond)");
            return;
        }

        String fluid = parts[0];
        float millilitersPerSecond = parts[1].toFloat();

        auto it = fluidToPumpMap.find(fluid);
        
        if (it == fluidToPumpMap.end()) {
            Serial.println("Error: unknown fluid " + fluid);
            return;
        }

        onCommandFluidSetMlPerSecond(it->second, millilitersPerSecond);
    };

    // Powder dispenser commands
    commandMap["powderSpin"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 2) {
            Serial.println("Usage: powderSpin(powderAlias,steps)");
            return;
        }

        String powderAlias = parts[0];
        int steps = parts[1].toInt();

        auto it = proteinToDispenserMap.find(powderAlias);
        
        if (it == proteinToDispenserMap.end()) {
            Serial.println("Error: unknown powder " + powderAlias);
            return;
        }

        onCommandDispenserSpin(it->second, steps);
    };

    commandMap["powderDispense"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 2) {
            Serial.println("Usage: powderDispense(powderAlias,grams)");
            return;
        }

        String powderAlias = parts[0];
        float grams = parts[1].toFloat();

        auto it = proteinToDispenserMap.find(powderAlias);
        
        if (it == proteinToDispenserMap.end()) {
            Serial.println("Error: unknown powder " + powderAlias);
            return;
        }

        onCommandDispensePowder(it->second, grams);
    };

    commandMap["dispenserSetStepsPerGram"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 2) {
            Serial.println("Usage: dispenserSetStepsPerGram(powderAlias,stepsPerGram)");
            return;
        }

        String powderAlias = parts[0];
        int stepsPerGram = parts[1].toInt();

        auto it = proteinToDispenserMap.find(powderAlias);
        
        if (it == proteinToDispenserMap.end()) {
            Serial.println("Error: unknown powder " + powderAlias);
            return;
        }

        it->second->setStepsPerGram(stepsPerGram);
        Serial.printf("Set %s steps per gram to %d\n", powderAlias.c_str(), stepsPerGram);
    };

    commandMap["enableDispenser"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 1) {
            Serial.println("Usage: enableDispenser(powderAlias)");
            return;
        }

        String powderAlias = parts[0];
        auto it = proteinToDispenserMap.find(powderAlias);
        
        if (it == proteinToDispenserMap.end()) {
            Serial.println("Error: unknown powder " + powderAlias);
            return;
        }

        it->second->enable();
        Serial.printf("Enabled %s dispenser\n", powderAlias.c_str());
    };

    commandMap["disableDispenser"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 1) {
            Serial.println("Usage: disableDispenser(powderAlias)");
            return;
        }

        String powderAlias = parts[0];
        auto it = proteinToDispenserMap.find(powderAlias);
        
        if (it == proteinToDispenserMap.end()) {
            Serial.println("Error: unknown powder " + powderAlias);
            return;
        }

        it->second->disable();
        Serial.printf("Disabled %s dispenser\n", powderAlias.c_str());
    };

    // more commands can be added here
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

// Initialize MDNS
void initMDNS() {
    if (!MDNS.begin("booster")) {
        Serial.println("Error setting up MDNS responder!");
        return;
    }

    Serial.println("MDNS responder started; domain is booster.local");

    MDNS.addService("ws", "tcp", SERVICE_PORT);
    Serial.printf("Registered service “_ws._tcp” on port %u\n", SERVICE_PORT);
}

// Initialize pins
void initPins() {
    // Built-in LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Set the LED to LOW initially

    // Stepper A
    // pinMode(STEPPER_A_DIR, OUTPUT);
    // digitalWrite(STEPPER_A_DIR, LOW);   // Set the stepper to LOW initially

    // pinMode(STEPPER_A_STEP, OUTPUT);
    // digitalWrite(STEPPER_A_STEP, LOW);   // Set the stepper to LOW initially
    
    // pinMode(STEPPER_A_SLEEP, OUTPUT);
    // digitalWrite(STEPPER_A_SLEEP, LOW);  // Set the stepper to LOW initially
    
    // Stepper B
    // pinMode(STEPPER_B_DIR, OUTPUT);
    // digitalWrite(STEPPER_B_DIR, LOW);   // Set the stepper to LOW initially

    // pinMode(STEPPER_B_STEP, OUTPUT);
    // digitalWrite(STEPPER_B_STEP, LOW);   // Set the stepper to LOW initially
    
    // pinMode(STEPPER_B_SLEEP, OUTPUT);
    // digitalWrite(STEPPER_B_SLEEP, LOW);  // Set the stepper to LOW initially

    // Tumeric
    pinMode(TUMERIC_A, OUTPUT);
    digitalWrite(TUMERIC_A, LOW);   // Set the stepper to LOW initially
    pinMode(TUMERIC_B, OUTPUT);
    digitalWrite(TUMERIC_B, LOW);   // Set the stepper to LOW initially

    Serial.println("Pins initialized");
}

void setup() {
    Serial.begin(115200);

    initPins();
    initWiFi();
    initWebSocket();
    initMDNS();
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