#pragma once


#define DEBUG_PRICE_DIVIDER 1.0       // 1.0 = реальные суммы (10.0 = тестовый режим, делит сумму)
#define PRICE_FOR_ONE_KWHOUR 1500     // Цена за 1 кВт·ч в копейках (1500 коп. = 15.00 руб.)
#define TEST_ENERGY_MULTIPLIER 1.0f   // 1.0 = реальная энергия (10.0 = тестовый режим, умножает энергию)

// Константы времени для управления платежами
extern const unsigned long REFUNDPERIOD;       // Период ожидания перед возвратом средств (мс)

void start_payment(long amount, Terminal terminal);
void start_payment(long amount);  // Совместимость (терминал A)

void process_POS_received_data();

// Обработка сообщений от терминалов (новая раздельная логика)
void processing_received_POS_message_terminalA();
void processing_received_POS_message_terminalB();

// Старая функция (обрабатывает currentReceivingTerminal)
void processing_received_POS_message();

void handle_successful_payment();
void handle_failed_payment(Terminal terminal);  // С указанием терминала
void handle_failed_payment();                   // Совместимость (терминал A)
void handle_payment_timeout();
void charging_managment_portA();
void charging_managment_portB();

struct tlv {
    String          mesName;
    int             opNumber;
    long            amount;     //копеек
    unsigned long   lastTime;
    bool            isMesProcessed;
};

enum PaymentStatus : uint8_t {
    //VRP 1000 -> IDL -> DIS -> FIN 560 = REFUND 440 -> IDL ->
    WAITING_PAYMENT     = 0,
    PAID                = 1,
    SPENDING            = 2,
    REFUND              = 3,
    INSUFFICIENT_FUNDS  = 4
};
enum ChargingStatus : uint8_t {
    WAITING_TO_CHARGE   = 0,
    START_TO_CHARGE     = 1,
    RUNNING             = 2,
    STOPPING            = 3
};


struct transactions{
    float           kWattPerHourAvailable;          // текущее значение мощности (КВт.ч)
    int             paidMinor;                      // оплачено в минорных единицах (копейки)
    PaymentStatus   paymentStatus;
    PaymentStatus   paymentStatusPrev;
    unsigned long   lastTime;
    ChargingStatus  chargingStatus;
    ChargingStatus  chargingStatusPrev;
    int             operationNumber;
    int             refundAmount;
    int             amountFIN;
    int             opNumberPrev;            
    String          paidTimeUTC;
    bool            meterError;                     // Ошибка связи со счетчиком (критическая!)

    bool  transactionActive;   // активна ли сейчас транзакция
};

// Экспорт портов для проверки их состояния из других модулей
extern transactions portA;
extern transactions portB;

// Проверка активности транзакции (для мониторинга терминала)
// Возвращает true если идет зарядка или ожидается возврат средств
bool is_charging_active();

// Проверка активности транзакции для конкретного терминала
// Возвращает true если терминал занят транзакцией (оплата, зарядка, возврат)
bool is_terminal_transaction_active(Terminal terminal);

// Аварийная остановка всех зарядок (при потере WiFi/интернета)
// Останавливает зарядку и инициирует возврат средств
void emergency_stop_all_charging();

