#pragma once


#define UART0_DEBUG_PORT Serial
#define UART0_DEBUG_PORT_BAUDRATE 115200

// ========== ТЕРМИНАЛ A (UART1 - аппаратный) ==========
#define UART1_TERMINAL_A_PORT Serial1
#define UART1_TERMINAL_A_BAUDRATE 115200
#define UART1_TERMINAL_A_RX_PIN 9
#define UART1_TERMINAL_A_TX_PIN 14

// Совместимость со старым кодом (будет удалено после полной миграции)
#define UART1_POS_PORT Serial1
#define UART1_POS_PORT_BAUDRATE 115200
#define UART1_POS_PORT_RX_PIN 9
#define UART1_POS_PORT_TX_PIN 14

// ========== ТЕРМИНАЛ B (UART0 - аппаратный) ==========
// ВАЖНО: Serial занят USB CDC, поэтому создаём отдельный объект для UART0
// Объект создаётся в uart.cpp как: HardwareSerial TerminalB_Serial(0);
#define UART0_TERMINAL_B_BAUDRATE 115200
#define UART0_TERMINAL_B_RX_PIN 18
#define UART0_TERMINAL_B_TX_PIN 17

// Внешний объект для терминала B
extern HardwareSerial TerminalB_Serial;

#define UART2_ENERGY_READ_PORT Serial2
#define UART2_ENERGY_READ_PORT_BAUDRATE 2400
#define UART2_ENERGY_READ_PORT_RX_PIN 44
#define UART2_ENERGY_READ_PORT_TX_PIN 43

#define BUFFER_SIZE 256

#define RX2_PIN 21 //для связи с энергосчетчика (устаревшее, не используется)
#define TX2_PIN 20

extern bool stayIDLE; // Флаг для периодической отправки сообщения IDL - регулярно переходить в состояние IDLE

// Перечисление терминалов
enum Terminal {
    TERMINAL_A = 0,
    TERMINAL_B = 1
};

void UART_Setup();
void UART_Commands_processing();
void send_HEX(const String& hexString);
void decode_HEX(const String& hexString);

// Новые функции с выбором терминала
void send_message(byte* message, int messageLength, Terminal terminal);
void send_message_to_terminal_A(byte* message, int messageLength);
void send_message_to_terminal_B(byte* message, int messageLength);

// Старая функция для совместимости (отправляет на терминал A)
void send_message(byte* message, int messageLength);

void process_received_data();
void UART_TerminalA_received_data();
void UART_TerminalB_received_data();

// Совместимость со старым кодом
void UART_POS_received_data();

void start_payment(long amount, Terminal terminal);
void start_payment(long amount);  // Совместимость (терминал A)
void send_VRP(long amount);


