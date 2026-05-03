#pragma once

#include <Arduino.h>

// ===== MQTT Configuration =====
#define MQTT_SERVER "31.186.145.255"
#define MQTT_PORT 1883
#define STATION_ID 1

// Для TLS (будущее):
// #define MQTT_PORT_TLS 8883
// #define MQTT_USE_TLS false  // Включить когда настроите сертификаты

// MQTT авторизация (раскомментировать когда включите на сервере):
// #define MQTT_USERNAME "station1"
// #define MQTT_PASSWORD "your_secure_password"

// Интервалы (мс)
#define MQTT_RECONNECT_INTERVAL 3000    // Быстрый reconnect (было 5000)
#define MQTT_STATUS_INTERVAL 30000      // Интервал публикации статуса
#define MQTT_KEEPALIVE 30               // Быстрое обнаружение обрыва (было 60)
#define MQTT_SOCKET_TIMEOUT 5           // Таймаут сокета в секундах
#define MQTT_BUFFER_SIZE 1024           // Увеличенный буфер для JSON

// ===== Функции =====

/**
 * @brief Инициализация MQTT клиента
 * Вызывать в setup() после init_wifi_connection()
 */
void init_mqtt();

/**
 * @brief Основной цикл MQTT
 * Вызывать в loop() - обрабатывает входящие сообщения и переподключение
 */
void mqtt_loop();

/**
 * @brief Проверка подключения к MQTT
 * @return true если подключен
 */
bool is_mqtt_connected();

/**
 * @brief Публикация статуса станции
 * Отправляет текущее состояние в топик stations/{id}/status
 */
void mqtt_publish_status();

/**
 * @brief Отправка ответа на команду
 * @param command_id UUID команды
 * @param status Статус выполнения ("executed", "failed")
 * @param message Сообщение/результат
 */
void mqtt_send_response(const char* command_id, const char* status, const char* message);

/**
 * @brief Отправка детального статуса станции (для TG бота)
 * @param command_id UUID команды для ответа
 */
void mqtt_send_detailed_status(const char* command_id);

/**
 * @brief Отправка debug лога через MQTT (вместо HTTP)
 * Публикует в топик stations/{id}/debug
 */
void mqtt_send_debug(
    const char* text1,
    const char* text2 = "",
    const char* text3 = "",
    const char* text4 = "",
    const char* text5 = "",
    const char* text6 = ""
);

/**
 * @brief Отправка информации о платеже через MQTT (вместо HTTP)
 * Публикует в топик stations/{id}/payments
 */
void mqtt_send_payment(
    const char* occurred_at,
    int amount_paid,        // в копейках
    int refund_amount,      // в копейках
    float kwh_spent,
    const char* pistol
);

