#pragma once

// Инициализация WiFi соединения контроллера
void init_wifi_connection();

// Обработка команды WIFI из Serial (ssid + password)
void process_wifi_command(const String& params);

// Переподключение к WiFi с текущими credentials
void reconnect_wifi();


