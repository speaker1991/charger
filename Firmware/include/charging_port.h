#pragma once
#include "payments.h"
#include "modbus.h"
#include "uart.h"

/**
 * @brief Класс для управления одним портом зарядки
 * 
 * Инкапсулирует логику управления зарядкой для одного порта,
 * устраняя дублирование кода между portA и portB
 */
class ChargingPort {
public:
    /**
     * @brief Конструктор класса управления портом
     * 
     * @param port Ссылка на структуру транзакции (portA или portB)
     * @param meter Ссылка на счетчик энергии
     * @param terminal Терминал (TERMINAL_A или TERMINAL_B)
     * @param portName Имя порта для логов ("A" или "B")
     * @param stayIDLE Ссылка на флаг периодической отправки IDL
     * @param relayOn Функция включения реле
     * @param relayOff Функция выключения реле
     */
    ChargingPort(
        transactions& port,
        ModbusEnergyMeter& meter,
        Terminal terminal,
        const char* portName,
        bool& stayIDLE,
        void (*relayOn)(),
        void (*relayOff)()
    ) : port_(port),
        meter_(meter),
        terminal_(terminal),
        portName_(portName),
        stayIDLE_(stayIDLE),
        relayOn_(relayOn),
        relayOff_(relayOff),
        debugTime_(millis()),
        debugRefundTime_(millis()),
        startChargingTime_(0),
        paidDiagnosticPrinted_(false),
        startChargingTimerStarted_(false)
    {}

    /**
     * @brief Основной цикл управления зарядкой порта
     * 
     * Эта функция должна вызываться в loop() для обработки:
     * - Проверки ошибок счетчика
     * - Переходов между состояниями зарядки
     * - Переходов между состояниями оплаты
     * - Возвратов средств
     * - Финализации транзакций
     */
    void manage();

private:
    // ========== ВНУТРЕННИЕ МЕТОДЫ ==========
    
    /**
     * @brief Проверка критических ошибок счетчика
     * Если счетчик не работает во время зарядки - немедленная остановка
     */
    void checkMeterError();

    /**
     * @brief Вывод отладочной информации о состоянии порта
     */
    void printDebugInfo();

    /**
     * @brief Обработка финализации транзакции с возвратом средств
     */
    void handleRefundFinalization();

    /**
     * @brief Проверка начала зарядки и валидации счетчика
     */
    void checkStartCharging();

    /**
     * @brief Машина состояний: переходы между состояниями зарядки
     */
    void updateChargingState();

    /**
     * @brief Обработка изменения состояния зарядки
     */
    void handleChargingStateChange();

    /**
     * @brief Обработка изменения состояния оплаты
     */
    void handlePaymentStateChange();

    // ========== ДАННЫЕ КЛАССА ==========
    
    transactions& port_;              // Ссылка на структуру транзакции (portA/portB)
    ModbusEnergyMeter& meter_;        // Ссылка на счетчик энергии
    Terminal terminal_;               // Терминал (TERMINAL_A/TERMINAL_B)
    const char* portName_;            // Имя порта для логов ("A" / "B")
    bool& stayIDLE_;                  // Ссылка на флаг периодической отправки IDL
    
    void (*relayOn_)();               // Функция включения реле
    void (*relayOff_)();              // Функция выключения реле
    
    unsigned long debugTime_;         // Таймер для отладочного вывода
    unsigned long debugRefundTime_;   // Таймер для возврата средств
    unsigned long startChargingTime_; // Таймер начала зарядки (для таймаута)
    bool paidDiagnosticPrinted_;      // Флаг для однократного вывода диагностики при PAID
    bool startChargingTimerStarted_;  // Флаг запуска таймера ожидания зарядки
    
    static constexpr unsigned long START_CHARGING_TIMEOUT_MS = 120000;  // 2 минуты таймаут
};

