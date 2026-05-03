/**
 * ESP32 MQTT Client Example
 * Charging Station Controller
 *
 * Библиотеки:
 * - PubSubClient: https://github.com/knolleary/pubsubclient
 * - ArduinoJson: https://arduinojson.org/
 *
 * Установка через Arduino IDE:
 * Sketch -> Include Library -> Manage Libraries
 * Поиск: PubSubClient, ArduinoJson
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

// ===== Configuration =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* MQTT_SERVER = "YOUR_SERVER_IP";  // IP или домен сервера
const int MQTT_PORT = 1883;

const char* API_URL = "http://YOUR_SERVER_IP:8000";
const char* STATION_API_KEY = "YOUR_STATION_API_KEY";  // API ключ станции

const int STATION_ID = 1;  // ID вашей станции

// ===== MQTT Topics =====
char TOPIC_COMMANDS[50];   // stations/{station_id}/commands
char TOPIC_STATUS[50];     // stations/{station_id}/status
char TOPIC_RESPONSE[50];   // stations/{station_id}/response

// ===== WiFi & MQTT Clients =====
WiFiClient espClient;
PubSubClient mqttClient(espClient);
HTTPClient http;

// ===== Station State =====
struct StationState {
    bool charging_active = false;
    float current_power = 0.0;
    float current_kwh = 0.0;
    String pistol = "";
    unsigned long start_time = 0;
} state;

// ===== Function Declarations =====
void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleCommand(JsonDocument& doc);
void sendCommandResponse(const char* command_id, const char* status, JsonDocument& response);
void publishStatus();
void sendStatusToAPI(const char* command_id, JsonDocument& response);

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 Charging Station Controller");

    // Формируем топики
    sprintf(TOPIC_COMMANDS, "stations/%d/commands", STATION_ID);
    sprintf(TOPIC_STATUS, "stations/%d/status", STATION_ID);
    sprintf(TOPIC_RESPONSE, "stations/%d/response", STATION_ID);

    setupWiFi();
    setupMQTT();
}

// ===== Main Loop =====
void loop() {
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    // Периодически отправляем статус (каждые 30 секунд)
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime > 30000) {
        publishStatus();
        lastStatusTime = millis();
    }

    // Ваша логика зарядки здесь
    // ...
}

// ===== WiFi Setup =====
void setupWiFi() {
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWiFi connected!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
}

// ===== MQTT Setup =====
void setupMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);  // Увеличиваем буфер для JSON
}

// ===== MQTT Reconnect =====
void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.println("Connecting to MQTT...");

        String clientId = "ESP32-Station-" + String(STATION_ID);

        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("MQTT connected!");

            // Подписываемся на команды
            mqttClient.subscribe(TOPIC_COMMANDS);
            Serial.printf("Subscribed to: %s\n", TOPIC_COMMANDS);

            // Публикуем начальный статус
            publishStatus();
        } else {
            Serial.printf("MQTT connection failed, rc=%d\n", mqttClient.state());
            Serial.println("Retrying in 5 seconds...");
            delay(5000);
        }
    }
}

// ===== MQTT Callback =====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("Message received on topic: %s\n", topic);

    // Парсим JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return;
    }

    // Обрабатываем команду
    handleCommand(doc);
}

// ===== Handle Command =====
void handleCommand(JsonDocument& doc) {
    const char* command = doc["command"];
    const char* command_id = doc["command_id"];
    JsonObject payload_obj = doc["payload"];

    Serial.printf("Command: %s, ID: %s\n", command, command_id);

    JsonDocument response;

    if (strcmp(command, "get_status") == 0) {
        // Команда: получить статус
        response["charging_active"] = state.charging_active;
        response["current_power"] = state.current_power;
        response["current_kwh"] = state.current_kwh;
        response["pistol"] = state.pistol;
        response["uptime_seconds"] = millis() / 1000;
        response["free_heap"] = ESP.getFreeHeap();

        sendCommandResponse(command_id, "executed", response);
    }
    else if (strcmp(command, "stop_charging") == 0) {
        // Команда: остановить зарядку
        if (state.charging_active) {
            state.charging_active = false;
            state.current_power = 0;
            // Здесь ваш код остановки зарядки
            response["message"] = "Charging stopped";
            sendCommandResponse(command_id, "executed", response);
        } else {
            response["message"] = "Charging was not active";
            sendCommandResponse(command_id, "executed", response);
        }
    }
    else if (strcmp(command, "start_charging") == 0) {
        // Команда: начать зарядку
        const char* pistol = payload_obj["pistol"] | "default";
        state.charging_active = true;
        state.pistol = pistol;
        state.start_time = millis();
        // Здесь ваш код начала зарядки
        response["message"] = "Charging started";
        response["pistol"] = pistol;
        sendCommandResponse(command_id, "executed", response);
    }
    else if (strcmp(command, "restart") == 0) {
        // Команда: перезагрузка
        response["message"] = "Restarting...";
        sendCommandResponse(command_id, "executed", response);
        delay(1000);
        ESP.restart();
    }
    else if (strcmp(command, "ping") == 0) {
        // Команда: пинг
        response["message"] = "pong";
        response["timestamp"] = millis();
        sendCommandResponse(command_id, "executed", response);
    }
    else if (strcmp(command, "set_max_current") == 0) {
        // Команда: установить максимальный ток
        float max_current = payload_obj["max_current"] | 32.0;
        // Здесь ваш код установки тока
        response["message"] = "Max current set";
        response["max_current"] = max_current;
        sendCommandResponse(command_id, "executed", response);
    }
    else {
        // Неизвестная команда
        response["error"] = "Unknown command";
        sendCommandResponse(command_id, "failed", response);
    }
}

// ===== Send Command Response =====
void sendCommandResponse(const char* command_id, const char* status, JsonDocument& response) {
    // 1. Публикуем ответ в MQTT
    JsonDocument responseDoc;
    responseDoc["command_id"] = command_id;
    responseDoc["status"] = status;
    responseDoc["response"] = response;
    responseDoc["timestamp"] = millis();

    char buffer[512];
    serializeJson(responseDoc, buffer);
    mqttClient.publish(TOPIC_RESPONSE, buffer);

    Serial.printf("Response sent: %s = %s\n", command_id, status);

    // 2. Отправляем подтверждение в API
    sendStatusToAPI(command_id, response);
}

// ===== Send Status to API =====
void sendStatusToAPI(const char* command_id, JsonDocument& response) {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    String url = String(API_URL) + "/ingest/command-response";

    JsonDocument doc;
    doc["command_id"] = command_id;
    doc["status"] = "executed";
    doc["response"] = response;

    String jsonString;
    serializeJson(doc, jsonString);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-Key", STATION_API_KEY);

    int httpCode = http.POST(jsonString);

    if (httpCode > 0) {
        Serial.printf("API response: %d\n", httpCode);
    } else {
        Serial.printf("API error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}

// ===== Publish Status =====
void publishStatus() {
    JsonDocument doc;
    doc["station_id"] = STATION_ID;
    doc["charging_active"] = state.charging_active;
    doc["current_power"] = state.current_power;
    doc["current_kwh"] = state.current_kwh;
    doc["pistol"] = state.pistol;
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();

    char buffer[256];
    serializeJson(doc, buffer);

    mqttClient.publish(TOPIC_STATUS, buffer);
    Serial.println("Status published");
}


/* ===== MQTT Topics Structure =====

SUBSCRIBE (ESP32 слушает):
  stations/{station_id}/commands
    Формат входящего сообщения:
    {
      "command": "get_status|stop_charging|start_charging|restart|ping|set_max_current",
      "command_id": "uuid-string",
      "timestamp": "2025-12-09T10:00:00Z",
      "payload": { ... }  // опционально
    }

PUBLISH (ESP32 отправляет):
  stations/{station_id}/status
    Периодический статус станции

  stations/{station_id}/response
    Ответ на команду

*/

