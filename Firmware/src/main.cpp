#include "main.h"

// ========== РАЗДЕЛЬНОЕ УПРАВЛЕНИЕ IDLE ДЛЯ ДВУХ ТЕРМИНАЛОВ ==========
bool stayIDLE_A = true;          // Терминал A в режиме IDL
bool stayIDLE_B = true;          // Терминал B в режиме IDL
bool stayIDLE = true;            // Совместимость (общий флаг)

// ========== РЕЖИМ БЕСПЛАТНОЙ ЗАРЯДКИ ==========
bool freeChargingMode_A = false; // Бесплатная зарядка на порту A
bool freeChargingMode_B = false; // Бесплатная зарядка на порту B

bool terminalDebugMode =  false;  
bool idleLogEnabled = false;   // управление выводом "Периодическая отправка: сообщение №..."
//#define TERMINAL_DEBUG_ONLY

void setup() {
    
#ifdef TERMINAL_DEBUG_ONLY
    terminalDebugMode = true;
#else
    terminalDebugMode = false;
#endif

    UART_Setup();            // Инициализация UART (терминал + отладка)
    load_api_key();          // Загрузка API ключа из NVS
    init_wifi_connection();  // WiFi нужен всегда (для мониторинга и уведомлений)
    init_time_client();      // NTP для временных меток
    init_mqtt();             // MQTT для удалённого управления
    initSDCard();

#if !defined(TERMINAL_DEBUG_ONLY)
    init_relay();
    init_modbus_energy_meter();
#endif
}
/* ДЛЯ ТЕСТА SD карты!!!!!!!!!!!!!!!!!!!!!!!!!!*/
// uint32_t lastTest = 0;
// uint32_t testCounter = 0;


void loop() {
    // Терминалы и отладочные команды (всегда)
    UART_Commands_processing();        // Обработка команд меню для тестирования терминала
    
    // ========== MQTT ==========
    mqtt_loop();                       // Обработка MQTT команд и переподключение
    
    // ========== ПРИЁМ ОТ ДВУХ ТЕРМИНАЛОВ ==========
    UART_TerminalA_received_data();    // Прием байтов от терминала A
    UART_TerminalB_received_data();    // Прием байтов от терминала B
    
    // ========== ОБРАБОТКА СООБЩЕНИЙ ОТ ДВУХ ТЕРМИНАЛОВ ==========
    processing_received_POS_message_terminalA(); // Обработка сообщений от терминала A
    processing_received_POS_message_terminalB(); // Обработка сообщений от терминала B
    
    terminal_stay_IDLE();              // Периодическая отправка IDL, чтобы терминалы были готовы принять оплату
    check_terminal_availability();     // Проверка доступности терминалов (уведомление при недоступности)
    check_wifi_and_manage_terminal();  // Проверка WiFi: DIS при потере, IDL при восстановлении

    /*ДЛЯ ТЕСТА SD карты!!!!!!!!!!!!!!!!!!!!!!!!!!    
        uint32_t now = millis();
        if (now - lastTest >= 2000) {
            lastTest = now;

            testCounter++;

            transactions t{};
            t.transactionActive = true;
            t.operationNumber = testCounter;
            t.paidMinor = 12345;
            t.refundAmount = 0;
            t.paidTimeUTC = getISO8601Time();
            t.meterError = false;

            logTransactionToSD(TERMINAL_A, t, "LOOP_TEST");

            Serial.printf("SD LOG TEST #%lu\n", (unsigned long)testCounter);
        }
    */
   
#if !defined(TERMINAL_DEBUG_ONLY)
    charging_managment_portA();        // Обработка процесса зарядки (порт A)
    charging_managment_portB();        // Обработка процесса зарядки (порт B)
    loop_update_energy();  
#endif
}
