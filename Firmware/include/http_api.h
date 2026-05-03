#pragma once


void send_POST_json(String occurred_at, int amount_paid, int refund_amount, float kwh_spent, const String &pistol);
void send_POST_debug(
    const String &text1,
    const String &text2,
    const String &text3,
    const String &text4,
    const String &text5,
    const String &text6,
    const String &occurred_at   // ISO8601 время, как в curl
);

// Быстрая проверка доступности интернета (доступности сервера Dominion)
// Возвращает true, если удалось установить HTTP-соединение и получить любой код ответа (>0)
bool is_internet_available();

// ========== API KEY MANAGEMENT ==========
// Загрузить API ключ из NVS (вызывать в setup)
void load_api_key();

// Сохранить новый API ключ в NVS
bool save_api_key(const String& newKey);

// Получить текущий API ключ
const char* get_api_key();

// Проверить, установлен ли API ключ
bool is_api_key_set();