#include "main.h"
#include <WiFi.h>
#include <Preferences.h>

// WiFi credentials - дефолтные значения
static const char* DEFAULT_SSID = "DIR-825-135";
static const char* DEFAULT_PASSWORD = "11223360";

// Текущие credentials (могут быть загружены из NVS)
static String wifiSSID = "";
static String wifiPassword = "";
static Preferences wifiPrefs;

// Загрузка WiFi credentials из NVS
void load_wifi_credentials() {
    wifiPrefs.begin("wificonfig", true);  // readonly
    wifiSSID = wifiPrefs.getString("ssid", DEFAULT_SSID);
    wifiPassword = wifiPrefs.getString("password", DEFAULT_PASSWORD);
    wifiPrefs.end();
}

// Сохранение WiFi credentials в NVS
bool save_wifi_credentials(const String& ssid, const String& password) {
    if (ssid.length() < 1 || ssid.length() > 32) {
        UART0_DEBUG_PORT.println("║ ✗ Ошибка: SSID должен быть от 1 до 32 символов");
        return false;
    }
    if (password.length() < 8 || password.length() > 64) {
        UART0_DEBUG_PORT.println("║ ✗ Ошибка: пароль должен быть от 8 до 64 символов");
        return false;
    }

    wifiPrefs.begin("wificonfig", false);  // read-write
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("password", password);
    wifiPrefs.end();
    
    wifiSSID = ssid;
    wifiPassword = password;
    
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ ✓ WiFi credentials сохранены в NVS");
    UART0_DEBUG_PORT.printf("║   SSID: %s\n", ssid.c_str());
    UART0_DEBUG_PORT.println("║   Пароль: ********");
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
    
    return true;
}

// Переподключение к WiFi с новыми credentials
void reconnect_wifi() {
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ 📶 Переподключение к WiFi...");
    UART0_DEBUG_PORT.printf("║   SSID: %s\n", wifiSSID.c_str());
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    
    WiFi.disconnect(true);
    delay(100);
    
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    const unsigned long WIFI_CONNECT_TIMEOUT = 10000;
    unsigned long start = millis();
    
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT) {
        delay(500);
        UART0_DEBUG_PORT.print(".");
    }
    
    UART0_DEBUG_PORT.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        UART0_DEBUG_PORT.println("║ ✓ Подключено к WiFi!");
        UART0_DEBUG_PORT.printf("║   IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        UART0_DEBUG_PORT.println("║ ✗ Не удалось подключиться");
    }
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
}

// Обработка команды WIFI
void process_wifi_command(const String& params) {
    if (params.length() == 0) {
        // Показать текущие настройки
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ 📶 НАСТРОЙКИ WIFI");
        UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
        UART0_DEBUG_PORT.printf("║ SSID: %s\n", wifiSSID.c_str());
        UART0_DEBUG_PORT.println("║ Пароль: ********");
        UART0_DEBUG_PORT.printf("║ Статус: %s\n", WiFi.status() == WL_CONNECTED ? "Подключено ✓" : "Не подключено ✗");
        if (WiFi.status() == WL_CONNECTED) {
            UART0_DEBUG_PORT.printf("║ IP: %s\n", WiFi.localIP().toString().c_str());
            UART0_DEBUG_PORT.printf("║ RSSI: %d dBm\n", WiFi.RSSI());
        }
        UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
        UART0_DEBUG_PORT.println("║ Использование: WIFI <ssid> <password>");
        UART0_DEBUG_PORT.println("║ Пример: WIFI MyNetwork MyPassword123");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        return;
    }
    
    // Парсим ssid и password
    int spacePos = params.indexOf(' ');
    if (spacePos == -1) {
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ✗ Ошибка: укажите SSID и пароль");
        UART0_DEBUG_PORT.println("║ Использование: WIFI <ssid> <password>");
        UART0_DEBUG_PORT.println("║ Пример: WIFI MyNetwork MyPassword123");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        return;
    }
    
    String newSSID = params.substring(0, spacePos);
    String newPassword = params.substring(spacePos + 1);
    newSSID.trim();
    newPassword.trim();
    
    if (save_wifi_credentials(newSSID, newPassword)) {
        // Автоматически переподключаемся с новыми данными
        reconnect_wifi();
    }
}

// Инициализация WiFi
void init_wifi_connection() {
  // ДОБАВЬТЕ ЭТУ СТРОКУ ВРЕМЕННО:
  save_wifi_credentials(DEFAULT_SSID, DEFAULT_PASSWORD);
  // Загружаем credentials из NVS
  load_wifi_credentials();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  UART0_DEBUG_PORT.printf("Connecting to WiFi: %s", wifiSSID.c_str());

  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

  const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // максимум 10 секунд ждём подключение
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT)
  {
    delay(500);
    UART0_DEBUG_PORT.print(".");
  }

    if (WiFi.status() == WL_CONNECTED) {
        UART0_DEBUG_PORT.println("\nConnected to WiFi!");
        UART0_DEBUG_PORT.print("IP Address: ");
        UART0_DEBUG_PORT.println(WiFi.localIP());
    } else {
        UART0_DEBUG_PORT.println("\nWiFi не подключён при старте, будут попытки переподключения в фоне");
    }
}