/**
 * @file http_api.cpp
 * @brief HTTP API функции (DEPRECATED - используйте MQTT)
 * 
 * ВНИМАНИЕ: HTTP функции send_POST_json и send_POST_debug заменены на MQTT:
 * - mqtt_send_payment() - для отправки платежей
 * - mqtt_send_debug()   - для отладочных сообщений
 * 
 * HTTP функции блокируют main loop и мешают MQTT.
 * Этот файл сохранён для обратной совместимости.
 */

#include "main.h"
#include <WiFi.h>
#include <Preferences.h>

// ========== API KEY MANAGEMENT ==========
static Preferences apiPrefs;
static String apiKeyStr = "";  // Кэш ключа в памяти

// Загрузка API ключа из NVS
void load_api_key() {
    apiPrefs.begin("apiconfig", true);  // readonly
    apiKeyStr = apiPrefs.getString("apikey", "");
    apiPrefs.end();
    
    if (apiKeyStr.length() > 0) {
        Serial.println("╔═════════════════════════════════════════════════════");
        Serial.println("║ ✓ API ключ загружен из памяти");
        Serial.printf("║ Длина: %d символов\n", apiKeyStr.length());
        Serial.println("╚═════════════════════════════════════════════════════");
    } else {
        Serial.println("╔═════════════════════════════════════════════════════");
        Serial.println("║ ⚠️ API ключ НЕ УСТАНОВЛЕН!");
        Serial.println("║ Используйте команду: APIKEY <ваш_ключ>");
        Serial.println("╚═════════════════════════════════════════════════════");
    }
}

// Сохранение API ключа в NVS
bool save_api_key(const String& newKey) {
    if (newKey.length() < 10) {
        Serial.println("║ ✗ Ошибка: ключ слишком короткий (минимум 10 символов)");
        return false;
    }
    
    apiPrefs.begin("apiconfig", false);  // read-write
    apiPrefs.putString("apikey", newKey);
    apiPrefs.end();
    
    apiKeyStr = newKey;  // Обновляем кэш
    
    Serial.println("╔═════════════════════════════════════════════════════");
    Serial.println("║ ✓ API ключ успешно сохранён!");
    Serial.printf("║ Длина: %d символов\n", newKey.length());
    Serial.println("║ Ключ будет использоваться для всех запросов к серверу");
    Serial.println("╚═════════════════════════════════════════════════════");
    return true;
}

// Получение текущего API ключа
const char* get_api_key() {
    return apiKeyStr.c_str();
}

// Проверка, установлен ли API ключ
bool is_api_key_set() {
    return apiKeyStr.length() > 0;
}

unsigned long previousMillis = 0;
const unsigned long interval = 100; // 1 секунда



// Отправка JSON POST на сервер с API Key
void send_POST_json(String occurred_at, int amount_paid, int refund_amount, float kwh_spent, const String &pistol) {

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://31.186.145.255/ingest/payments"); // адрес сервера

        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-Station-Key", get_api_key()); // добавляем ключ станции

        // Формируем JSON
        // ВАЖНО: 
        // - amount_paid и refund_amount уже в "отладочных" значениях (поделены на DEBUG_PRICE_DIVIDER в payments.cpp)
        // - kwh_spent уже в реальных значениях (поделена на TEST_ENERGY_MULTIPLIER в charging_port.cpp)
        JsonDocument doc;
        doc["occurred_at"] = occurred_at;
        doc["amount_paid"] = (float)amount_paid / 100.0;      // В рублях (копейки → рубли)
        doc["refund_amount"] = (float)refund_amount / 100.0;  // В рублях (копейки → рубли)
        doc["kwh_spent"] = kwh_spent;                         // Реальная энергия (уже поделена на TEST_ENERGY_MULTIPLIER)
        doc["pistol"] = pistol;
        String jsonStr;
        serializeJson(doc, jsonStr);

        int httpResponseCode = http.POST(jsonStr);

        if (httpResponseCode > 0) {
            String response = http.getString();
            
            Serial.println();
            Serial.println("╔═════════════════════════════════════════════════════");
            Serial.printf("║ 📤 ПЛАТЁЖ ОТПРАВЛЕН НА СЕРВЕР (код %d)\n", httpResponseCode);
            Serial.println("╠─────────────────────────────────────────────────────");
            Serial.printf("║ Время: %s\n", occurred_at.c_str());
            Serial.printf("║ Оплачено: %.2f руб.\n", (float)amount_paid / 100.0);      // Только перевод в рубли
            Serial.printf("║ Возврат: %.2f руб.\n", (float)refund_amount / 100.0);     // Только перевод в рубли
            Serial.printf("║ Энергия: %.2f кВт·ч\n", kwh_spent);
            Serial.printf("║ Пистолет: %s\n", pistol.c_str());
            
            // Парсим ответ сервера
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);
            if (!error && doc.containsKey("id")) {
                Serial.printf("║ ID записи: %d\n", doc["id"].as<int>());
            }
            
            Serial.println("╚═════════════════════════════════════════════════════");
        } else {
            Serial.println();
            Serial.println("╔═════════════════════════════════════════════════════");
            Serial.printf("║ ✗ ОШИБКА ОТПРАВКИ ПЛАТЕЖА (код %d)\n", httpResponseCode);
            Serial.printf("║ %s\n", http.errorToString(httpResponseCode).c_str());
            Serial.println("╚═════════════════════════════════════════════════════");
        }

        http.end();
    } else {
        Serial.println();
        Serial.println("╔═════════════════════════════════════════════════════");
        Serial.println("║ ✗ ПЛАТЁЖ НЕ ОТПРАВЛЕН: WiFi не подключён");
        Serial.println("╚═════════════════════════════════════════════════════");
    }


    }

    
}

void send_POST_debug(
    const String &text1,
    const String &text2,
    const String &text3,
    const String &text4,
    const String &text5,
    const String &text6,
    const String &occurred_at   // ISO8601 время, как в curl
) {
    // Настройки ретраев
    const uint8_t  maxRetries    = 3;      // сколько всего попыток
    const uint32_t retryDelayMs  = 10;   // пауза между попытками, мс

    unsigned long currentMillis = millis();

    // Ограничиваем частоту первой отправки (по твоему interval)
    if (currentMillis - previousMillis < interval) {
        return;
    }
    previousMillis = currentMillis;

    // Если сразу нет Wi-Fi — даже не пробуем
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println();
        Serial.println("┌─────────────────────────────────────────────────────");
        Serial.println("│ ✗ HTTP DEBUG: WiFi не подключён");
        Serial.println("└─────────────────────────────────────────────────────");
        return;
    }

    uint8_t attempt = 0;
    bool    done    = false;

    while (!done && attempt < maxRetries) {
        attempt++;

        HTTPClient http;
        http.begin("http://31.186.145.255/ingest/debug");

        http.addHeader("Content-Type", "application/json");
        http.addHeader("accept", "application/json");
        http.addHeader("X-Station-Key", get_api_key());  // ключ станции

        // Формируем JSON как в curl
        JsonDocument doc;
        doc["text1"]       = text1;
        doc["text2"]       = text2;
        doc["text3"]       = text3;
        doc["text4"]       = text4;
        doc["text5"]       = text5;
        doc["text6"]       = text6;
        doc["occurred_at"] = occurred_at;

        String payload;
        serializeJson(doc, payload);

        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0) {
            // Есть HTTP-ответ
            if (httpResponseCode >= 200 && httpResponseCode < 300) {
                // Успех
                String response = http.getString();
                
                // Красивый вывод в стиле проекта
                Serial.println();
                Serial.println("┌─────────────────────────────────────────────────────");
                Serial.printf("│ ✓ HTTP DEBUG ОТПРАВЛЕН (код %d)\n", httpResponseCode);
                Serial.println("├─────────────────────────────────────────────────────");
                
                // Парсим JSON ответ для красивого вывода
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, response);
                
                if (!error) {
                    if (doc.containsKey("text1")) Serial.printf("│ %s\n", doc["text1"].as<const char*>());
                    if (doc.containsKey("text2")) Serial.printf("│ %s\n", doc["text2"].as<const char*>());
                    if (doc.containsKey("text3")) Serial.printf("│ %s\n", doc["text3"].as<const char*>());
                    if (doc.containsKey("text4")) Serial.printf("│ %s\n", doc["text4"].as<const char*>());
                    if (doc.containsKey("text5")) Serial.printf("│ %s\n", doc["text5"].as<const char*>());
                    if (doc.containsKey("text6")) Serial.printf("│ %s\n", doc["text6"].as<const char*>());
                    if (doc.containsKey("id")) Serial.printf("│ Запись ID: %d\n", doc["id"].as<int>());
                } else {
                    // Если не удалось распарсить - выводим как есть
                    Serial.print("│ Ответ: ");
                    Serial.println(response);
                }
                
                Serial.println("└─────────────────────────────────────────────────────");
                done = true;
            } else if (httpResponseCode >= 500 && httpResponseCode < 600 &&
                       attempt < maxRetries) {
                // 5xx — ошибка сервера, пробуем ещё раз
                Serial.println();
                Serial.println("┌─────────────────────────────────────────────────────");
                Serial.printf("│ ⚠️ HTTP DEBUG: Ошибка сервера %d\n", httpResponseCode);
                Serial.printf("│ Повторная попытка %d/%d...\n", attempt, maxRetries);
                Serial.println("└─────────────────────────────────────────────────────");
                http.end();
                delay(retryDelayMs);
                continue;
            } else {
                // 4xx и всё остальное — не ретраим
                Serial.println();
                Serial.println("┌─────────────────────────────────────────────────────");
                Serial.printf("│ ✗ HTTP DEBUG: Ошибка %d\n", httpResponseCode);
                Serial.println("│ Повторная попытка не выполняется");
                Serial.println("└─────────────────────────────────────────────────────");
                done = true;
            }
        } else {
            // httpResponseCode <= 0 — сетевые ошибки (в т.ч. connection refused)
            String err = http.errorToString(httpResponseCode);
            Serial.println();
            Serial.println("┌─────────────────────────────────────────────────────");
            Serial.printf("│ ✗ HTTP DEBUG: Сетевая ошибка (%d)\n", httpResponseCode);
            Serial.printf("│ %s\n", err.c_str());

            if (attempt < maxRetries) {
                Serial.printf("│ Повторная попытка %d/%d...\n", attempt, maxRetries);
                Serial.println("└─────────────────────────────────────────────────────");
                http.end();
                delay(retryDelayMs);
                continue;
            } else {
                Serial.println("│ Достигнуто максимальное количество попыток");
                Serial.println("└─────────────────────────────────────────────────────");
                done = true;
            }
        }

        http.end();
    }
}

// Проверка наличия интернета: пробуем установить HTTP-соединение с сервером Dominion.
// Любой положительный HTTP-код (>0) считаем признаком доступности интернета,
// даже если это 404/405 и т.п. (нам важен сам факт соединения).
bool is_internet_available() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    // Используем debug-эндпоинт, но с GET — нам не важно содержимое
    http.begin("http://31.186.145.255/ingest/debug");
    http.setConnectTimeout(3000);   // увеличен таймаут до 3 сек для стабильности
    http.setTimeout(5000);          // общий таймаут на операцию

    int httpCode = http.GET();
    http.end();

    return (httpCode > 0);
}