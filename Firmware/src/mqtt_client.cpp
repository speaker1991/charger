#include "main.h"
#include "mqtt_client.h"
#include "relay.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


// Флаги управления терминалами (из main.cpp)
extern bool stayIDLE_A;
extern bool stayIDLE_B;

// Флаги бесплатной зарядки (из main.cpp)
extern bool freeChargingMode_A;
extern bool freeChargingMode_B;

// Порты зарядки (из payments.cpp)
extern transactions portA;
extern transactions portB;

// ===== MQTT Client =====
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ===== Topics =====
char topicCommands[50];   // stations/{id}/commands - входящие команды
char topicStatus[50];     // stations/{id}/status - исходящий статус
char topicResponse[50];   // stations/{id}/response - ответы на команды
char topicDebug[50];      // stations/{id}/debug - отладочные сообщения (вместо HTTP)
char topicPayments[50];   // stations/{id}/payments - информация о платежах (вместо HTTP)

// ===== Timers =====
static unsigned long lastReconnectAttempt = 0;
static unsigned long lastStatusPublish = 0;

// ===== Forward declarations =====
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void mqtt_reconnect();
void mqtt_handle_command(const char* payload, unsigned int length);

// ===== Initialization =====
void init_mqtt() {
    // Формируем топики
    snprintf(topicCommands, sizeof(topicCommands), "stations/%d/commands", STATION_ID);
    snprintf(topicStatus, sizeof(topicStatus), "stations/%d/status", STATION_ID);
    snprintf(topicResponse, sizeof(topicResponse), "stations/%d/response", STATION_ID);
    snprintf(topicDebug, sizeof(topicDebug), "stations/%d/debug", STATION_ID);
    snprintf(topicPayments, sizeof(topicPayments), "stations/%d/payments", STATION_ID);

    // Настройка клиента
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE);
    mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT);  // Быстрый таймаут
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);        // Увеличенный буфер для JSON

    Serial.println();
    Serial.println("╔═════════════════════════════════════════════════════");
    Serial.println("║ 📡 MQTT ИНИЦИАЛИЗАЦИЯ");
    Serial.println("╠─────────────────────────────────────────────────────");
    Serial.printf("║ Сервер: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    Serial.printf("║ Station ID: %d\n", STATION_ID);
    Serial.printf("║ Keep-alive: %d сек\n", MQTT_KEEPALIVE);
    Serial.printf("║ Socket timeout: %d сек\n", MQTT_SOCKET_TIMEOUT);
    Serial.printf("║ Buffer size: %d байт\n", MQTT_BUFFER_SIZE);
    Serial.println("╠─────────────────────────────────────────────────────");
    Serial.printf("║ → commands: %s\n", topicCommands);
    Serial.printf("║ ← status: %s\n", topicStatus);
    Serial.printf("║ ← response: %s\n", topicResponse);
    Serial.printf("║ ← debug: %s\n", topicDebug);
    Serial.printf("║ ← payments: %s\n", topicPayments);
    Serial.println("╚═════════════════════════════════════════════════════");

    // Первая попытка подключения
    if (WiFi.status() == WL_CONNECTED) {
        mqtt_reconnect();
    }
}

// ===== Main Loop =====
void mqtt_loop() {
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
            lastReconnectAttempt = now;
            mqtt_reconnect();
        }
    } else {
        mqttClient.loop();

        // Периодическая публикация статуса
        unsigned long now = millis();
        if (now - lastStatusPublish > MQTT_STATUS_INTERVAL) {
            lastStatusPublish = now;
            mqtt_publish_status();
        }
    }
}

// ===== Reconnect =====
void mqtt_reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        return;  // Нет WiFi - не пытаемся
    }

    Serial.println();
    Serial.println("┌─────────────────────────────────────────────────────");
    Serial.println("│ 📡 MQTT: Подключение...");

    // Формируем уникальный Client ID
    char clientId[30];
    snprintf(clientId, sizeof(clientId), "ESP32-Station-%d", STATION_ID);

    if (mqttClient.connect(clientId)) {
        Serial.println("│ ✓ MQTT подключен!");
        
        // Подписываемся на команды
        if (mqttClient.subscribe(topicCommands)) {
            Serial.printf("│ ✓ Подписка на: %s\n", topicCommands);
        } else {
            Serial.printf("│ ✗ Ошибка подписки на: %s\n", topicCommands);
        }

        Serial.println("└─────────────────────────────────────────────────────");

        // Публикуем начальный статус
        mqtt_publish_status();
    } else {
        Serial.printf("│ ✗ Ошибка подключения, rc=%d\n", mqttClient.state());
        Serial.println("│ Коды ошибок:");
        Serial.println("│   -4 = MQTT_CONNECTION_TIMEOUT");
        Serial.println("│   -3 = MQTT_CONNECTION_LOST");
        Serial.println("│   -2 = MQTT_CONNECT_FAILED");
        Serial.println("│   -1 = MQTT_DISCONNECTED");
        Serial.println("│    1 = MQTT_CONNECT_BAD_PROTOCOL");
        Serial.println("│    2 = MQTT_CONNECT_BAD_CLIENT_ID");
        Serial.println("│    3 = MQTT_CONNECT_UNAVAILABLE");
        Serial.println("│    4 = MQTT_CONNECT_BAD_CREDENTIALS");
        Serial.println("│    5 = MQTT_CONNECT_UNAUTHORIZED");
        Serial.println("└─────────────────────────────────────────────────────");
    }
}

// ===== Callback - получение сообщений =====
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.println();
    Serial.println("╔═════════════════════════════════════════════════════");
    Serial.println("║ 📩 MQTT: ПОЛУЧЕНА КОМАНДА");
    Serial.println("╠─────────────────────────────────────────────────────");
    Serial.printf("║ Топик: %s\n", topic);
    Serial.printf("║ Размер: %u байт\n", length);
    Serial.println("╠─────────────────────────────────────────────────────");
    Serial.println("║ Payload (raw):");
    Serial.print("║ ");
    
    // Выводим payload как строку
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    
    Serial.println("╠─────────────────────────────────────────────────────");

    // Обрабатываем команду
    mqtt_handle_command((const char*)payload, length);

    Serial.println("╚═════════════════════════════════════════════════════");
}

// ===== Обработка команды =====
void mqtt_handle_command(const char* payload, unsigned int length) {
    // Парсим JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.printf("║ ✗ Ошибка парсинга JSON: %s\n", error.c_str());
        return;
    }

    // Извлекаем поля команды
    const char* command = doc["command"] | "unknown";
    const char* command_id = doc["command_id"] | "no-id";
    const char* timestamp = doc["timestamp"] | "";
    JsonObject cmd_payload = doc["payload"];

    Serial.println("║ Распарсенная команда:");
    Serial.printf("║   command: %s\n", command);
    Serial.printf("║   command_id: %s\n", command_id);
    Serial.printf("║   timestamp: %s\n", timestamp);
    
    // Выводим payload команды если есть
    if (!cmd_payload.isNull() && cmd_payload.size() > 0) {
        Serial.println("║   payload:");
        for (JsonPair kv : cmd_payload) {
            Serial.printf("║     %s: ", kv.key().c_str());
            if (kv.value().is<const char*>()) {
                Serial.println(kv.value().as<const char*>());
            } else if (kv.value().is<int>()) {
                Serial.println(kv.value().as<int>());
            } else if (kv.value().is<float>()) {
                Serial.println(kv.value().as<float>());
            } else {
                Serial.println("(complex)");
            }
        }
    }

    Serial.println("╠─────────────────────────────────────────────────────");

    // ===== ОБРАБОТКА КОМАНД =====
    
    if (strcmp(command, "get_status") == 0) {
        Serial.println("║ 📊 Выполняю: GET_STATUS");
        mqtt_publish_status();
        
        // Формируем информативное сообщение о статусе
        mqtt_send_detailed_status(command_id);
    }
    else if (strcmp(command, "ping") == 0) {
        Serial.println("║ 🏓 Выполняю: PING");
        char msg[64];
        snprintf(msg, sizeof(msg), "pong, uptime: %lu sec", millis() / 1000);
        mqtt_send_response(command_id, "executed", msg);
    }
    else if (strcmp(command, "restart") == 0) {
        Serial.println("║ 🔄 Выполняю: RESTART");
        Serial.println("║ Отключаю терминалы перед перезагрузкой...");
        
        // Сначала отправляем ответ что команда принята
        mqtt_send_response(command_id, "executed", "Команда принята. Отключаю терминалы и перезагружаюсь...");
        
        // Даём время MQTT клиенту отправить сообщение
        mqttClient.loop();
        delay(300);
        
        // Отключаем терминалы перед перезагрузкой
        send_DIS(TERMINAL_A);
        send_DIS(TERMINAL_B);
        delay(200);  // Даём время отправить DIS
        
        ESP.restart();
    }
    else if (strcmp(command, "start_charging") == 0) {
        Serial.println("║ 🟢 Получена: START_CHARGING");
        const char* pistol = cmd_payload["pistol"] | "A";
        Serial.printf("║    Пистолет: %s\n", pistol);
        Serial.println("║ ⚠️ TODO: Реализовать запуск зарядки");
        mqtt_send_response(command_id, "executed", "Start charging command received (TODO: implement)");
    }
    else if (strcmp(command, "set_max_current") == 0) {
        Serial.println("║ ⚡ Получена: SET_MAX_CURRENT");
        float max_current = cmd_payload["max_current"] | 32.0f;
        Serial.printf("║    Макс. ток: %.1f A\n", max_current);
        Serial.println("║ ⚠️ TODO: Реализовать установку тока");
        mqtt_send_response(command_id, "executed", "Set max current command received (TODO: implement)");
    }
    // ===== УПРАВЛЕНИЕ ТЕРМИНАЛАМИ =====
    else if (strcmp(command, "enable_terminal_a") == 0) {
        Serial.println("║ 🟢 Выполняю: ENABLE_TERMINAL_A");
        // Нельзя включить терминал если активен бесплатный режим
        if (freeChargingMode_A) {
            Serial.println("║ ✗ Отказано: активен бесплатный режим");
            mqtt_send_response(command_id, "failed", "⚠️ Нельзя включить терминал A.\nСначала отключите бесплатный режим.");
        } else {
            stayIDLE_A = true;
            send_IDL(TERMINAL_A);
            Serial.println("║ ✓ Терминал A включён");
            mqtt_send_response(command_id, "executed", "✅ Терминал A включён");
        }
    }
    else if (strcmp(command, "disable_terminal_a") == 0) {
        Serial.println("║ 🔴 Выполняю: DISABLE_TERMINAL_A");
        stayIDLE_A = false;
        send_DIS(TERMINAL_A);
        Serial.println("║ ✓ Терминал A отключён");
        mqtt_send_response(command_id, "executed", "🔴 Терминал A отключён");
    }
    else if (strcmp(command, "enable_terminal_b") == 0) {
        Serial.println("║ 🟢 Выполняю: ENABLE_TERMINAL_B");
        // Нельзя включить терминал если активен бесплатный режим
        if (freeChargingMode_B) {
            Serial.println("║ ✗ Отказано: активен бесплатный режим");
            mqtt_send_response(command_id, "failed", "⚠️ Нельзя включить терминал B.\nСначала отключите бесплатный режим.");
        } else {
            stayIDLE_B = true;
            send_IDL(TERMINAL_B);
            Serial.println("║ ✓ Терминал B включён");
            mqtt_send_response(command_id, "executed", "✅ Терминал B включён");
        }
    }
    else if (strcmp(command, "disable_terminal_b") == 0) {
        Serial.println("║ 🔴 Выполняю: DISABLE_TERMINAL_B");
        stayIDLE_B = false;
        send_DIS(TERMINAL_B);
        Serial.println("║ ✓ Терминал B отключён");
        mqtt_send_response(command_id, "executed", "🔴 Терминал B отключён");
    }
    // ===== БЕСПЛАТНАЯ ЗАРЯДКА =====
    else if (strcmp(command, "free_charging_a") == 0) {
        Serial.println("║ 🆓 Выполняю: FREE_CHARGING_A");
        freeChargingMode_A = true;
        stayIDLE_A = false;  // Отключаем периодическую отправку IDL
        send_DIS(TERMINAL_A);  // Отключаем терминал
        ralay_portA_on();  // Включаем реле - подключаем линию данных
        Serial.println("║ ✓ Бесплатная зарядка A включена");
        Serial.println("║   Терминал A отключён (DIS)");
        Serial.println("║   Реле A включено - зарядка разрешена");
        mqtt_send_response(command_id, "executed", "🆓 Бесплатная зарядка A включена.\nТерминал отключён, реле ВКЛ.");
    }
    else if (strcmp(command, "free_charging_b") == 0) {
        Serial.println("║ 🆓 Выполняю: FREE_CHARGING_B");
        freeChargingMode_B = true;
        stayIDLE_B = false;  // Отключаем периодическую отправку IDL
        send_DIS(TERMINAL_B);  // Отключаем терминал
        ralay_portB_on();  // Включаем реле - подключаем линию данных
        Serial.println("║ ✓ Бесплатная зарядка B включена");
        Serial.println("║   Терминал B отключён (DIS)");
        Serial.println("║   Реле B включено - зарядка разрешена");
        mqtt_send_response(command_id, "executed", "🆓 Бесплатная зарядка B включена.\nТерминал отключён, реле ВКЛ.");
    }
    else if (strcmp(command, "disable_free_charging_a") == 0) {
        Serial.println("║ 💰 Выполняю: DISABLE_FREE_CHARGING_A");
        freeChargingMode_A = false;
        ralay_portA_off();  // Выключаем реле - отключаем линию данных
        stayIDLE_A = true;  // Включаем периодическую отправку IDL
        send_IDL(TERMINAL_A);  // Включаем терминал
        Serial.println("║ ✓ Платный режим A включён");
        Serial.println("║   Реле A выключено - ждём оплаты");
        Serial.println("║   Терминал A включён (IDL)");
        mqtt_send_response(command_id, "executed", "💰 Платный режим A включён.\nРеле ВЫКЛ, терминал активирован.");
    }
    else if (strcmp(command, "disable_free_charging_b") == 0) {
        Serial.println("║ 💰 Выполняю: DISABLE_FREE_CHARGING_B");
        freeChargingMode_B = false;
        ralay_portB_off();  // Выключаем реле - отключаем линию данных
        stayIDLE_B = true;  // Включаем периодическую отправку IDL
        send_IDL(TERMINAL_B);  // Включаем терминал
        Serial.println("║ ✓ Платный режим B включён");
        Serial.println("║   Реле B выключено - ждём оплаты");
        Serial.println("║   Терминал B включён (IDL)");
        mqtt_send_response(command_id, "executed", "💰 Платный режим B включён.\nРеле ВЫКЛ, терминал активирован.");
    }
    // ===== СТАТУС ТЕРМИНАЛОВ (для динамических кнопок в боте) =====
    else if (strcmp(command, "get_terminals_status") == 0) {
        Serial.println("║ 📊 Выполняю: GET_TERMINALS_STATUS");
        
        // Определяем реальный статус терминалов (с учётом ошибок счётчика)
        bool termA_disabled_meter = is_terminal_A_disabled_due_to_meter();
        bool termB_disabled_meter = is_terminal_B_disabled_due_to_meter();
        
        // Терминал считается включённым только если:
        // 1. stayIDLE = true (нет программного отключения)
        // 2. Нет ошибки счётчика
        bool termA_actually_enabled = stayIDLE_A && !termA_disabled_meter;
        bool termB_actually_enabled = stayIDLE_B && !termB_disabled_meter;
        
        // Формируем JSON с состоянием терминалов
        JsonDocument respDoc;
        respDoc["command_id"] = command_id;
        respDoc["status"] = "executed";
        respDoc["terminal_a_enabled"] = termA_actually_enabled;
        respDoc["terminal_b_enabled"] = termB_actually_enabled;
        respDoc["free_charging_a"] = freeChargingMode_A;
        respDoc["free_charging_b"] = freeChargingMode_B;
        
        // Добавляем причины отключения
        respDoc["meter_error_a"] = portA.meterError;
        respDoc["meter_error_b"] = portB.meterError;
        respDoc["disabled_by_meter_a"] = termA_disabled_meter;
        respDoc["disabled_by_meter_b"] = termB_disabled_meter;
        
        respDoc["timestamp"] = millis();
        
        char buffer[384];
        serializeJson(respDoc, buffer);
        
        if (mqttClient.publish(topicResponse, buffer)) {
            Serial.println("║ ✓ Статус терминалов отправлен");
            Serial.printf("║   A: %s (meter_err=%d)\n", 
                termA_actually_enabled ? "ВКЛ" : "ВЫКЛ", termA_disabled_meter);
            Serial.printf("║   B: %s (meter_err=%d)\n", 
                termB_actually_enabled ? "ВКЛ" : "ВЫКЛ", termB_disabled_meter);
        } else {
            Serial.println("║ ✗ Ошибка MQTT публикации");
        }
    }
    else {
        Serial.printf("║ ❓ Неизвестная команда: %s\n", command);
        mqtt_send_response(command_id, "failed", "Unknown command");
    }
}

// ===== Публикация статуса =====
void mqtt_publish_status() {
    if (!mqttClient.connected()) {
        return;
    }

    JsonDocument doc;
    
    doc["station_id"] = STATION_ID;
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);

    // Статус портов зарядки
#if !defined(TERMINAL_DEBUG_ONLY)
    JsonObject port_a = doc["port_a"].to<JsonObject>();
    port_a["charging_status"] = portA.chargingStatus;
    port_a["payment_status"] = portA.paymentStatus;
    port_a["kwh_available"] = portA.kWattPerHourAvailable;
    port_a["meter_error"] = portA.meterError;

    JsonObject port_b = doc["port_b"].to<JsonObject>();
    port_b["charging_status"] = portB.chargingStatus;
    port_b["payment_status"] = portB.paymentStatus;
    port_b["kwh_available"] = portB.kWattPerHourAvailable;
    port_b["meter_error"] = portB.meterError;
#endif

    char buffer[512];
    size_t len = serializeJson(doc, buffer);

    mqttClient.publish(topicStatus, buffer);
}

// ===== Отправка ответа на команду =====
void mqtt_send_response(const char* command_id, const char* status, const char* message) {
    if (!mqttClient.connected()) {
        Serial.println("║ ✗ MQTT не подключен, ответ не отправлен");
        return;
    }

    JsonDocument doc;
    doc["command_id"] = command_id;
    doc["status"] = status;
    doc["message"] = message;
    doc["timestamp"] = millis();

    char buffer[256];
    serializeJson(doc, buffer);

    if (mqttClient.publish(topicResponse, buffer)) {
        Serial.printf("║ ✓ MQTT ответ: %s = %s\n", command_id, status);
    } else {
        Serial.println("║ ✗ Ошибка MQTT публикации");
    }
}

// ===== Отправка детального статуса (для TG бота) =====
void mqtt_send_detailed_status(const char* command_id) {
    if (!mqttClient.connected()) {
        Serial.println("║ ✗ MQTT не подключен, ответ не отправлен");
        return;
    }

    // Формируем читаемое сообщение о статусе
    String msg = "";
    
    // Режим зарядки
    msg += "🔌 Режим зарядки:\n";
    msg += "  A: ";
    msg += freeChargingMode_A ? "🆓 БЕСПЛАТНАЯ" : "💰 Платная";
    msg += "\n  B: ";
    msg += freeChargingMode_B ? "🆓 БЕСПЛАТНАЯ" : "💰 Платная";
    
    // Терминалы
    msg += "\n\n💳 Терминалы:\n";
    msg += "  A: ";
    msg += stayIDLE_A ? "✅ ВКЛ" : "🔴 ВЫКЛ";
    msg += "\n  B: ";
    msg += stayIDLE_B ? "✅ ВКЛ" : "🔴 ВЫКЛ";
    
    // Счётчики
    msg += "\n\n📊 Счётчики:\n";
    msg += "  A: ";
    msg += portA.meterError ? "❌ ОШИБКА" : "✅ OK";
    msg += "\n  B: ";
    msg += portB.meterError ? "❌ ОШИБКА" : "✅ OK";
    
    // Статус зарядки
    msg += "\n\n⚡ Зарядка:\n";
    
    // Порт A
    msg += "  A: ";
    switch (portA.chargingStatus) {
        case WAITING_TO_CHARGE: msg += "⏳ Ожидание"; break;
        case START_TO_CHARGE:   msg += "🔄 Запуск"; break;
        case RUNNING:           msg += "⚡ Идёт зарядка"; break;
        case STOPPING:          msg += "⏹ Остановка"; break;
        default:                msg += "❓ Неизвестно"; break;
    }
    
    // Порт B  
    msg += "\n  B: ";
    switch (portB.chargingStatus) {
        case WAITING_TO_CHARGE: msg += "⏳ Ожидание"; break;
        case START_TO_CHARGE:   msg += "🔄 Запуск"; break;
        case RUNNING:           msg += "⚡ Идёт зарядка"; break;
        case STOPPING:          msg += "⏹ Остановка"; break;
        default:                msg += "❓ Неизвестно"; break;
    }
    
    // WiFi и uptime
    msg += "\n\n📶 WiFi RSSI: ";
    msg += String(WiFi.RSSI());
    msg += " dBm\n⏱ Uptime: ";
    msg += String(millis() / 1000 / 60);
    msg += " мин";

    // Отправляем ответ
    JsonDocument doc;
    doc["command_id"] = command_id;
    doc["status"] = "executed";
    doc["message"] = msg;
    doc["timestamp"] = millis();

    char buffer[512];
    serializeJson(doc, buffer);

    if (mqttClient.publish(topicResponse, buffer)) {
        Serial.println("║ ✓ MQTT детальный статус отправлен");
    } else {
        Serial.println("║ ✗ Ошибка MQTT публикации");
    }
}

// ===== Проверка подключения =====
bool is_mqtt_connected() {
    return mqttClient.connected();
}

// ===== Отправка debug через MQTT (вместо HTTP) =====
void mqtt_send_debug(
    const char* text1,
    const char* text2,
    const char* text3,
    const char* text4,
    const char* text5,
    const char* text6
) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Debug не отправлен: нет подключения");
        return;
    }

    JsonDocument doc;
    doc["station_id"] = STATION_ID;
    doc["text1"] = text1;
    doc["text2"] = text2;
    doc["text3"] = text3;
    doc["text4"] = text4;
    doc["text5"] = text5;
    doc["text6"] = text6;
    doc["timestamp"] = millis();

    char buffer[512];
    serializeJson(doc, buffer);

    if (mqttClient.publish(topicDebug, buffer, false)) {
        Serial.printf("[MQTT] Debug отправлен: %s\n", text1);
    } else {
        Serial.println("[MQTT] Ошибка отправки debug");
    }
}

// ===== Отправка платежа через MQTT (вместо HTTP) =====
void mqtt_send_payment(
    const char* occurred_at,
    int amount_paid,
    int refund_amount,
    float kwh_spent,
    const char* pistol
) {
    if (!mqttClient.connected()) {
        Serial.println("[MQTT] Платёж не отправлен: нет подключения");
        return;
    }

    JsonDocument doc;
    doc["station_id"] = STATION_ID;
    doc["occurred_at"] = occurred_at;
    doc["amount_paid"] = (float)amount_paid / 100.0;      // копейки → рубли
    doc["refund_amount"] = (float)refund_amount / 100.0;  // копейки → рубли
    doc["kwh_spent"] = kwh_spent;
    doc["pistol"] = pistol;
    doc["timestamp"] = millis();

    char buffer[256];
    serializeJson(doc, buffer);

    if (mqttClient.publish(topicPayments, buffer, false)) {
        Serial.println();
        Serial.println("╔═════════════════════════════════════════════════════");
        Serial.println("║ 📤 ПЛАТЁЖ ОТПРАВЛЕН (MQTT)");
        Serial.println("╠─────────────────────────────────────────────────────");
        Serial.printf("║ Время: %s\n", occurred_at);
        Serial.printf("║ Оплачено: %.2f руб.\n", (float)amount_paid / 100.0);
        Serial.printf("║ Возврат: %.2f руб.\n", (float)refund_amount / 100.0);
        Serial.printf("║ Энергия: %.2f кВт·ч\n", kwh_spent);
        Serial.printf("║ Пистолет: %s\n", pistol);
        Serial.println("╚═════════════════════════════════════════════════════");
    } else {
        Serial.println("[MQTT] Ошибка отправки платежа");
    }
}

