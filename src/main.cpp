#include <Arduino.h>
#include "secrets.h"
#include "customColors.h"
#include "AnimatedStrip.h"
#include "StepperPowderDispenser.h"
#include "SymmetricFillAnim.h"
#include "BlinkingSymetricFillAnim.h"
#include "Pump.h"
#include <ESPAsyncWebServer.h>
#include <map>
#include <functional>
#include <ESPmDNS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <FastLED.h>

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
#define TUMERIC 35

#define STEPPER_A_STEP   14   // STEP pin   14
#define STEPPER_A_SLEEP  13   // SLEEP pin  13
#define STEPPER_A_DIR     5   // DIR pin     5 

#define STEPPER_B_STEP    15  // STEP pin   12
#define STEPPER_B_SLEEP    7  // SLEEP pin  11
#define STEPPER_B_DIR      6  // DIR pin     6

#define DHTPIN 17

#define RGB_DATA 48

// ——— State machine ——— 
#define NOT_PREPARING -1
#define START_ORDER 0
#define WATER_PUMPING 1
#define PROTEIN_DISPENSING 2
#define FLAVOR_PUMPING 3
#define TUMERIC_DISPENSING 4
#define FINISH_ORDER 5

int state = NOT_PREPARING;

// ——— Global variables & constants ———
#define NUM_LEDS 84
#define LED_TYPE    WS2812
#define COLOR_ORDER BGR

CRGB leds[NUM_LEDS];
AnimatedStrip strip(leds, NUM_LEDS);

#define DHTTYPE DHT11
DHT_Unified dht(DHTPIN, DHTTYPE);

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
#define SERVICE_PORT 80

// Create a WebServer object
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

// Stepper objects
static std::map<String, StepperPowderDispenser*> proteinToDispenserMap;

StepperPowderDispenser birdman(
    "Birdman",
    STEPPER_B_STEP,
    STEPPER_B_SLEEP,
    STEPPER_B_DIR,
    false, // dispense clockwise
    32.5415, //346.18,   // steps per gram
    3000,  // step interval in microseconds
    3000,  // pulse duration in microseconds
    200,   // steps per revolution
    1000,  // vibration step interval in microseconds
    100,   // vibration pulse duration in microseconds
    84    // steps per vibration
);
StepperPowderDispenser pureHealth(
    "Pure Health",
    STEPPER_A_STEP,
    STEPPER_A_SLEEP,
    STEPPER_A_DIR,
    false, // dispense clockwise
    71.8602,   // steps per gram
    3000,  // step interval in microseconds
    3000,  // pulse duration in microseconds
    200,   // steps per revolution
    1000,  // vibration step interval in microseconds
    100,   // vibration pulse duration in microseconds
    84    // steps per vibration
);

// Pump objects
static std::map<String, Pump*> fluidToPumpMap;
Pump chocolate("Saborizante de Chocolate", PERISTALTIC_A, 1.8f, false);
Pump vainilla("Saborizante de Vainilla", PERISTALTIC_B, 1.53f, false);
Pump fresa("Saborizante de Fresa", PERISTALTIC_C, 1.56f, false);
Pump agua("Agua", WATER_PUMP, 32.83f, true); // Negated logic, LOW = on, HIGH = off
Pump tumeric("Tumeric", TUMERIC, 8.7575f, false); // Non calibrated

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
}

void onCommandDispensePowder(StepperPowderDispenser* dispenser, float grams) {
    if (grams <= 0.0f) {
        Serial.println("Error: grams must be > 0");
        return;
    }

    Serial.printf("Dispensing %.2f grams of %s\n", grams, dispenser->getPowderName().c_str());
    
    dispenser->enable();
    dispenser->dispense(grams);
}

void onCommandReadHumidity() {
    float averageHumidity = 0.0f;

    // Make 10 readings to get an average
    for (int i = 0; i < 10; i++) {
        sensors_event_t event;
        dht.humidity().getEvent(&event);
        if (isnan(event.relative_humidity)) {
            Serial.println(F("Error reading humidity!"));
            return;
        }
        averageHumidity += event.relative_humidity;
        delay(100); // wait a bit between readings
    }
    
    averageHumidity /= 10.0f;
    Serial.print(F("Average Humidity: "));
    Serial.print(averageHumidity);
    Serial.println(F("%"));
    // Send average humidity data over WebSocket
    String humidityData = String(averageHumidity);
    ws.textAll(humidityData);
}

// Order to prepare variables
static StepperPowderDispenser* orderDispenser = nullptr; 
static float orderGrams = 0.0f;
static Pump* orderPump = nullptr;
static float orderMilliliters = 0.0f;
static float orderTumericMl = 0.0f;

// Receives the dispenser, the amount in grams, the pump, the amount in mL, and ammount of tumeric in grams
void onCommandPrepareDrink(StepperPowderDispenser* dispenser, float grams, Pump* pump, float milliliters, float tumericMl) {
    if (grams <= 0.0f || milliliters <= 0.0f) {
        Serial.println("Error: grams and ml amounts must be > 0");
        return;
    }

    Serial.printf("Preparing drink with %.2f grams of %s, %.2f mL of %s, and %.2f grams of Tumeric\n",
        grams, dispenser->getPowderName().c_str(),
        milliliters, pump->getFluidName().c_str(),
        tumericMl
    );

    // Start the preparation process
    state = START_ORDER;
    // Set lastOrderVariables
    orderDispenser = dispenser;
    orderGrams = grams;
    orderPump = pump;
    orderMilliliters = milliliters;
    orderTumericMl = tumericMl;
    Serial.println("Drink preparation started");
}

void onCommandSetRGB(const String& args) {
    auto parts = splitArgs(args);
    if (parts.size() < 3) {
        Serial.println("Usage: setRGB(red,green,blue)");
        return;
    }

    int red = parts[0].toInt();
    int green = parts[1].toInt();
    int blue = parts[2].toInt();

    // Clamp values to 0-255
    red = constrain(red, 0, 255);
    green = constrain(green, 0, 255);
    blue = constrain(blue, 0, 255);

    fill_solid(leds, NUM_LEDS, CRGB(red, green, blue));
    FastLED.setBrightness(255); // Set brightness to maximum
    
    leds[0] = CRGB::Black;

    FastLED.show();
    Serial.printf("Set RGB to (%d,%d,%d)\n", red, green, blue);
}

void onCommandSymetric(const String& args) {
    auto parts = splitArgs(args);
    if (parts.size() < 6) {
        Serial.println("Usage: symetric(startIndex,endIndex,r,g,b,animationDurationMs)");
        return;
    }

    int startIndex = parts[0].toInt();
    int endIndex = parts[1].toInt();
    CRGB color = CRGB(
        parts[2].toInt(), // Red
        parts[3].toInt(), // Green
        parts[4].toInt()  // Blue
    );
    float durationMs = parts[5].toFloat();

    if (startIndex < 0 || endIndex >= NUM_LEDS || startIndex > endIndex) {
        Serial.println("Error: Invalid indices for symetric animation");
        return;
    }

    // SymmetricFillAnim* cmdSymAnim = new SymmetricFillAnim(
    //     startIndex,
    //     endIndex,
    //     color,
    //     durationMs,
    //     60 // FPS
    // );

    RadiatingSymmetricPulseAnim* cmdSymAnim = new RadiatingSymmetricPulseAnim(
        startIndex,
        endIndex,
        true,
        3,
        color,
        durationMs,
        60 // FPS
    );

    strip.addAnimation(cmdSymAnim);
}

RadiatingSymmetricPulseAnim* tabletRadInPointer = nullptr;
RadiatingSymmetricPulseAnim* bottleRadInPointer = nullptr;
RadiatingSymmetricPulseAnim* animationWhilePreparing = nullptr;

void onCommandOrderDetails() {
    if (tabletRadInPointer != nullptr) {
        tabletRadInPointer->finish();
    }

    tabletRadInPointer = new RadiatingSymmetricPulseAnim(
        49,
        56,
        true,
        0,
        TABLET_INTERACT_YELLOW,
        300,
        60 // FPS
    );

    strip.addAnimation(tabletRadInPointer);

    Serial.println("Waiting for user to check their order");
}

void onCommandOrderCanceled() {
    // Stop the tablet animation
    if (tabletRadInPointer != nullptr) {
        tabletRadInPointer->finish();
        tabletRadInPointer = nullptr;
    }

    SymmetricFillAnim *fixTablet = new SymmetricFillAnim(
        49 - 5, // Start index
        56 + 5, // End index
        DIM_BOOSTUP_PURPLE, // Color
        500, // Duration in milliseconds
        60 // FPS
    );

    SymmetricFillAnim *fixBottle = new SymmetricFillAnim(
        33 - 5, // Start index
        43 + 5, // End index
        DIM_BOOSTUP_PURPLE, // Color
        500, // Duration in milliseconds
        60 // FPS
    );

    if (bottleRadInPointer != nullptr) {
        bottleRadInPointer->finish();
        bottleRadInPointer = nullptr;
    }
    
    strip.addAnimation(fixTablet);
    strip.addAnimation(fixBottle);

    Serial.println("Order cancelled, returning to idle state");
}

void onCommandOrderAskForBottle() {
    // Stop the tablet animation
    if (tabletRadInPointer != nullptr) {
        tabletRadInPointer->finish();
        tabletRadInPointer = nullptr;
    }

    if (bottleRadInPointer != nullptr) {
        bottleRadInPointer->finish();
    }

    bottleRadInPointer = new RadiatingSymmetricPulseAnim(
        33,
        43,
        true,
        0,
        INSERT_BOTTLE_YELLOW,
        300,
        60 // FPS
    );


    SymmetricFillAnim *fixTablet = new SymmetricFillAnim(
        49 - 5, // Start index
        56 + 5, // End index
        DIM_BOOSTUP_PURPLE, // Color
        500, // Duration in milliseconds
        60 // FPS
    );

    strip.addAnimation(fixTablet);

    strip.addAnimation(bottleRadInPointer);
    Serial.println("Asking user to insert bottle");
}

void onCommandProgressBar() {
    if (bottleRadInPointer != nullptr) {
        bottleRadInPointer->finish();
        bottleRadInPointer = nullptr;
    }

    if (animationWhilePreparing != nullptr) {
        animationWhilePreparing->finish();
        animationWhilePreparing = nullptr;
    }

    animationWhilePreparing = new RadiatingSymmetricPulseAnim(
        33,
        43,
        true,
        0,
        PROGRESS_BLUE,
        1000,
        60, // FPS,
        60
    );

    SymmetricFillAnim *fixBottle = new SymmetricFillAnim(
        33 - 5, // Start index
        43 + 5, // End index
        DIM_BOOSTUP_PURPLE, // Color
        500, // Duration in milliseconds
        60 // FPS
    );
    strip.addAnimation(fixBottle);

    strip.addAnimation(animationWhilePreparing);

    Serial.println("Order preparation animation started");
}

void onCommandOrderFinish() {
    if (animationWhilePreparing != nullptr) {
        animationWhilePreparing->finish();
        animationWhilePreparing = nullptr;
    }

    // Finish the order preparation animation

    RadiatingSymmetricPulseAnim *takeBottle = new RadiatingSymmetricPulseAnim(
        33,
        43,
        false,
        5,
        REMOVE_BOTTLE_GREEN,
        300,
        60 // FPS
    );

    SymmetricFillAnim *fixBottle = new SymmetricFillAnim(
        33 - 5, // Start index
        43 + 5, // End index
        DIM_BOOSTUP_PURPLE, // Color
        500, // Duration in milliseconds
        60 // FPS
    );

    fixBottle->start_delay_ms = 6000; // Wait 3 seconds before starting the fix animation

    strip.addAnimation(takeBottle);
    
    strip.addAnimation(fixBottle);
    
    Serial.println("Order preparation finished");
}

// Initialize commands and their handlers
void initCommands() {
    fluidToPumpMap["1"] = &chocolate;
    fluidToPumpMap["2"] = &vainilla;
    fluidToPumpMap["3"] = &fresa;
    fluidToPumpMap["a"] = &agua;
    fluidToPumpMap["c"] = &tumeric;

    proteinToDispenserMap["1"] = &birdman;
    proteinToDispenserMap["2"] = &pureHealth;

    commandMap["rgb"] = [](const String& args){
        onCommandSetRGB(args);
    };

    commandMap["symetric"] = [](const String& args){
        onCommandSymetric(args);
    };

    // Animation commands
    commandMap["orderDetails"] = [](const String& args){
        onCommandOrderDetails();
    };

    commandMap["orderCanceled"] = [](const String& args){
        onCommandOrderCanceled();
    };

    commandMap["orderAskForBottle"] = [](const String& args){
        onCommandOrderAskForBottle();
    };

    commandMap["orderProgressBar"] = [](const String& args){
        onCommandProgressBar();
    };

    commandMap["orderFinish"] = [](const String& args){
        onCommandOrderFinish();
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

    // DHT commands
    commandMap["readHumidity"] = [](const String& args){
        onCommandReadHumidity();
    };

    commandMap["prepare"] = [](const String& args){
        auto parts = splitArgs(args);
        if (parts.size() < 4) {
            Serial.println("Usage: prepare(powderAlias,grams,fluidAlias,milliliters,tumericGrams)");
            return;
        }

        String powderAlias = parts[0];
        float grams = parts[1].toFloat();
        String fluidAlias = parts[2];
        float milliliters = parts[3].toFloat();
        float tumericGrams = parts.size() > 4 ? parts[4].toFloat() : 0.0f;

        auto powderIt = proteinToDispenserMap.find(powderAlias);
        if (powderIt == proteinToDispenserMap.end()) {
            Serial.println("Error: unknown powder " + powderAlias);
            return;
        }

        auto fluidIt = fluidToPumpMap.find(fluidAlias);
        if (fluidIt == fluidToPumpMap.end()) {
            Serial.println("Error: unknown fluid " + fluidAlias);
            return;
        }

        onCommandPrepareDrink(powderIt->second, grams, fluidIt->second, milliliters, tumericGrams);
    };

    // more commands can be added here
    Serial.println("Commands initialized");
}

void updateStateMachine(){
    if (state == NOT_PREPARING) {
        // Not preparing anything
        return;
    } else if (state == START_ORDER) {
        // Start the order
        
        onCommandProgressBar();

        Serial.println("Starting order preparation");
        state = PROTEIN_DISPENSING;

        orderDispenser->enable();
        orderDispenser->dispense(orderGrams);
        
     } else if (state == PROTEIN_DISPENSING) {
        if (!orderDispenser->isDispensing()) {
            Serial.println("Protein dispensing done, pumping flavor");
            state = WATER_PUMPING;
            orderDispenser->disable();
            agua.enable();
            agua.dispense(350.0f); // Dispense 350 mL of water
        }
    } else if (state == WATER_PUMPING) {
        if (!agua.isDispensing()) {
            Serial.println("Water pumping done, dispensing protein");
            delay(500); // Half a second delay before dispensing protein
            state = FLAVOR_PUMPING;
            orderPump->enable();
            orderPump->dispense(orderMilliliters);
        }
    } else if (state == FLAVOR_PUMPING) {
        if (!orderPump->isDispensing()) {
            Serial.println("Flavor pumping done, dispensing Tumeric");
            state = TUMERIC_DISPENSING;
            // Start dispensing tumeric
            tumeric.enable();
            tumeric.dispense(orderTumericMl);
        }
    } else if (state == TUMERIC_DISPENSING) {
        if (!tumeric.isDispensing()) {
            orderPump->disable(); // Disable flavor pump
            // Tumeric dispensing is done
            Serial.println("Tumeric dispensing done");
            state = FINISH_ORDER;
            tumeric.disable(); // Disable tumeric pump
        }
    } else if (state == FINISH_ORDER) {
        // Order finished, reset state
        Serial.println("Order finished");

        onCommandOrderFinish();
        
        // Let the websocket clients know
        ws.textAll("Order finished");
        // Reset order variables
        orderDispenser = nullptr;
        orderGrams = 0.0f;
        orderPump = nullptr;
        orderMilliliters = 0.0f;
        orderTumericMl = 0.0f;

        state = NOT_PREPARING;

        // Disable all dispensers and pumps
        birdman.disable();
        pureHealth.disable();
        chocolate.disable();
        vainilla.disable();
        fresa.disable();
        agua.disable();
        tumeric.disable();
    }
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

// Initialize RGB Strip
void initRGBStrip() {
    FastLED.addLeds<LED_TYPE, RGB_DATA, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();



    Serial.println("RGB Strip initialized");
}

// Initialize pins
void initPins() {
    // Built-in LED
    pinMode(RGB_DATA, OUTPUT);

    Serial.println("Pins initialized");
}

void setup() {
    Serial.begin(115200);

    initPins();

    initRGBStrip();
    fill_solid(leds, NUM_LEDS, CRGB::Red);

    FastLED.show();

    initWiFi();
    fill_solid(leds, NUM_LEDS, CRGB::Orange);
    FastLED.show();

    initWebSocket();
    fill_solid(leds, NUM_LEDS, CRGB::Yellow);
    FastLED.show();

    initMDNS();
    initCommands();
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();

    // Start server
    server.begin();
    
    fill_solid(leds, NUM_LEDS, BOOSTUP_PURPLE);
    leds[0] = CRGB::Black;
    FastLED.show();

    SymmetricFillAnim* frontAnim = new SymmetricFillAnim(
        26,
        59,
        DIM_BOOSTUP_PURPLE,
        1000.0f,
        60 // FPS
    );

    strip.addAnimation(frontAnim);
}

void loop() {
    ws.cleanupClients();

    updateStateMachine();

    strip.update();

    chocolate.update();
    vainilla.update();
    fresa.update();
    agua.update();
    tumeric.update();

    birdman.update();
    pureHealth.update();
}