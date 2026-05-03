#pragma once
#include "uart.h"  // Для enum Terminal

extern volatile int operationNumber;

std::vector<byte> create_IDL_message(int messageLength);
std::vector<byte> create_VTK_message(const std::string& messageName, int operationNumber, int& messageLength, const std::map<int, std::vector<byte>>& additionalParams);

// ========== НОВЫЕ ФУНКЦИИ С ВЫБОРОМ ТЕРМИНАЛА ==========
void send_IDL(Terminal terminal);
void send_ABR(int operationNumber, Terminal terminal);
void send_ABR(Terminal terminal);
void send_IDL_with_management_data(char mode1, char mode2, char mode3, Terminal terminal);
void send_DIS(Terminal terminal);
void send_VRP(long amount, Terminal terminal);
void send_FIN(float amount, int opNumber, Terminal terminal);
void sendREFUND(int amount, int operationNumber, Terminal terminal);

// ========== СТАРЫЕ ФУНКЦИИ ДЛЯ СОВМЕСТИМОСТИ (отправляют на терминал A) ==========
void send_IDL();
void send_ABR();
void send_ABR(int operationNumber);
void send_IDL_with_management_data(char mode1, char mode2, char mode3);
void send_DIS();
void send_VRP(long amount);
void send_FIN(float amount, int opNumber);
void sendREFUND(int amount, int operationNumber);

void increment_operation_number();
int get_current_operation_number();
void terminal_stay_IDLE();

// Мониторинг доступности терминалов
void update_terminal_A_last_response();   // Вызывать при получении ответа от терминала A
void update_terminal_B_last_response();   // Вызывать при получении ответа от терминала B
void update_terminal_last_response();     // Старая функция (обновляет терминал A)
void check_terminal_availability();       // Проверка доступности обоих терминалов (вызывать в loop)
bool is_terminal_available();             // Статус (true если хотя бы один терминал доступен)
bool is_terminal_A_available();           // Статус терминала A
bool is_terminal_B_available();           // Статус терминала B

// Мониторинг WiFi — отключение терминала при потере интернета
void check_wifi_and_manage_terminal();    // Проверка WiFi и DIS/IDL (вызывать в loop)
bool is_terminal_disabled_due_to_wifi();  // Терминал отключён из-за WiFi?