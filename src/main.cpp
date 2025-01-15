#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <secrets.h>

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

// Create a WebSocket object
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && 
        info->index == 0 && 
        info->len == len && 
        info->opcode == WS_TEXT) {
        
        data[len] = 0;
        String message = (char *)data;

        Serial.println("Command recieved: " + message);

        // Check if the message is "Command"
        if (strcmp((char *)data, "Command") == 0) {
            // Do something
        }

        // Add more commands here...
    }
}

void onEvent(
    AsyncWebSocket *server, 
    AsyncWebSocketClient *client, 
    AwsEventType type, 
    void *arg, 
    uint8_t *data, 
    size_t len) {

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

    Serial.println(WiFi.localIP());
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    Serial.println("WebSocket initialized");
}

void setup() {
    Serial.begin(115200);

    initWiFi();
    initWebSocket();

    // Pin declarations go here:

    // End of pin declarations

    // Start server
    server.begin(); // Not entirely sure if the WS Server needs this to run
}

void loop() {
    // Do something:
    delay(500);


    // At the end:
    ws.cleanupClients();
}