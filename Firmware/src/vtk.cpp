#include "main.h"
#include <WiFi.h>

// Константы протокола связи VTK
const int KEEPALIVE_INTERVAL = 10000;   // Интервал keepalive в мс
const int OPERATION_NUMBER_LENGTH = 8;  // Длина номера операции
const int MAX_OPERATION_NUMBER = 65535;
const int MIN_MESSAGE_SIZE = 10;

const unsigned long IDL_INTERVAL = 10000; // Интервал в 10 секунд для IDLE
int message_counter = 1;                 // Счетчик сообщений
static unsigned long lastSendTime = millis();

// Хелпер: получить имя терминала для логов
const char* getTerminalName(Terminal terminal) {
    return (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B";
}

std::vector<byte> create_IDL_message(int messageLength) {

  // Выделяем память под сообщение с использованием RAII (std::vector)
  std::vector<byte> message(messageLength + 2);

  // Заполняем структуру сообщения
  message[0] = 0x1F;               // STX
  message[1] = 0x00;               // Старший байт длины
  message[2] = messageLength - 3;  // Младший байт длины (11)
  message[3] = 0x96;               // Дискриминатор VMC
  message[4] = 0xFB;

  // Message Name (IDL)
  message[5] = 0x01;  // Тег Message Name
  message[6] = 0x03;  // Длина
  message[7] = 'I';
  message[8] = 'D';
  message[9] = 'L';

  // Расчет CRC16
  uint16_t crc = calculate_CRC16_ccitt(message.data(), messageLength);
  message[messageLength] = crc >> 8;
  message[messageLength + 1] = crc & 0xFF;

  return message;
}

std::vector<byte> create_VTK_message(const std::string& messageName, int operationNumber, int& messageLength, const std::map<int, std::vector<byte>>& additionalParams = {}) {
  // Расчет базовой длины с учетом двухбайтовой длины
  int opLength = operationNumber < 10 ? 1 : static_cast<int>(log10(operationNumber)) + 1;
  int baseLength = 2 + 2 + messageName.length() + 2 + opLength;  // 2 байта протокол + 2 тег имени + байты имени + тег операции + длина номера операции

  for (const auto& param : additionalParams) {
    baseLength += 2 + param.second.size();  //добавляем длину параметров
  }

  messageLength = baseLength + 3;  //without CRC

  // Выделяем память под сообщение с использованием RAII (std::vector)
  std::vector<byte> message(messageLength + 2);  //with CRC
  int pos = 0;

  // Формирование заголовка
  message[pos++] = 0x1F;  // STX

  // Длина сообщения в двух байтах
  message[pos++] = (baseLength) >> 8;  // Старший байт
  message[pos++] = (baseLength)&0xFF;  // Младший байт

  message[pos++] = 0x96;
  message[pos++] = 0xFB;

  // Имя сообщения
  message[pos++] = 0x01;
  message[pos++] = messageName.length();
  for (size_t i = 0; i < messageName.length(); i++) {
    message[pos++] = static_cast<byte>(messageName[i]);
  }

  // Номер операции в ASCII
  message[pos++] = 0x03;      // Тег операции
  message[pos++] = opLength;  // Длина поля операции

  if (operationNumber == 0) {
    message[pos++] = '0';
  } else {
    int temp = operationNumber;
    int writePos = pos + opLength - 1;
    while (temp > 0) {
      message[writePos--] = '0' + temp % 10;
      temp /= 10;
    }
    pos += opLength;
  }

  // Дополнительные параметры
  for (const auto& param : additionalParams) {
    message[pos++] = static_cast<byte>(param.first);
    message[pos++] = static_cast<byte>(param.second.size());
    for (size_t i = 0; i < param.second.size(); i++) {
      message[pos++] = param.second[i];
    }
  }

  uint16_t crc = calculate_CRC16_ccitt(message.data(), messageLength);
  message[messageLength] = static_cast<byte>(crc >> 8);
  message[messageLength + 1] = static_cast<byte>(crc & 0xFF);
  

  //отладочный вывод в порт USB (не показываем IDL если idleLogEnabled=false)
  bool showDebug = (messageName != "IDL") || idleLogEnabled;
  if (showDebug) {
    UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.print("│ ► К ТЕРМИНАЛУ │ HEX (");
    UART0_DEBUG_PORT.print(messageLength + 2);
    UART0_DEBUG_PORT.print(" байт): ");
    for (int i = 0; i < messageLength + 2; i++) {
      if (message[i] < 0x10) UART0_DEBUG_PORT.print("0");
      UART0_DEBUG_PORT.print(message[i], HEX);
      UART0_DEBUG_PORT.print(" ");
    }
    UART0_DEBUG_PORT.println();
  }


  return message;
}


/*Отправка терминалу сообщения IDLE чтобы он был готов принять оплату*/
void send_IDL(Terminal terminal) {
    int messageLength = 10;
    std::vector<byte> message = create_IDL_message(messageLength);
    
    // Упрощенный вывод для IDL (можно включить через idleLogEnabled)
    if (idleLogEnabled) {
      UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
      UART0_DEBUG_PORT.printf("│ ► К %s │ Команда: IDL (готов к оплате)\n", getTerminalName(terminal));
      UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");
    }
    
    // Отправка сообщения на конкретный терминал
    send_message(message.data(), messageLength + 2, terminal);

    // Память освободится автоматически (RAII)
}

// Старая функция для совместимости (отправляет на терминал A)
void send_IDL() {
    send_IDL(TERMINAL_A);
}

// IDL с POS Management Data (VTK: Management Data Tag 0x06)
// Например, 'S','N','N' — команда закрыть день/сделать settlement
void send_IDL_with_management_data(char mode1, char mode2, char mode3, Terminal terminal) {
  int messageLength;
  int opNumber = get_current_operation_number();

  std::map<int, std::vector<byte>> params;
  std::vector<byte> mgmt;
  mgmt.push_back((byte)mode1);
  mgmt.push_back((byte)mode2);
  mgmt.push_back((byte)mode3);
  // 0x06 — Management Data (по VTK-протоколу)
  params[0x06] = mgmt;

  std::vector<byte> message = create_VTK_message("IDL", opNumber, messageLength, params);

  UART0_DEBUG_PORT.printf("│ К %s │ IDL+Management │ Режим: '", getTerminalName(terminal));
  UART0_DEBUG_PORT.print(mode1);
  UART0_DEBUG_PORT.print(mode2);
  UART0_DEBUG_PORT.print(mode3);
  UART0_DEBUG_PORT.print("' │ Операция: #");
  UART0_DEBUG_PORT.println(opNumber);
  UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");

  send_message(message.data(), messageLength + 2, terminal);
  // Память освободится автоматически (RAII)
}

// Старая функция для совместимости
void send_IDL_with_management_data(char mode1, char mode2, char mode3) {
  send_IDL_with_management_data(mode1, mode2, mode3, TERMINAL_A);
}

// ABR — Abort Request: отмена операции по номеру
void send_ABR(int opNumber, Terminal terminal) {
  int messageLength;
  std::vector<byte> message = create_VTK_message("ABR", opNumber, messageLength);

  UART0_DEBUG_PORT.printf("│ К %s │ ABR (отмена) │ Операция: #", getTerminalName(terminal));
  UART0_DEBUG_PORT.println(opNumber);
  UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");

  // Отправка сообщения на конкретный терминал
  send_message(message.data(), messageLength + 2, terminal);

  // Память освободится автоматически (RAII)
}

// Удобный вариант без номера — используем текущий номер операции
void send_ABR(Terminal terminal) {
  send_ABR(get_current_operation_number(), terminal);
}

// Старые функции для совместимости
void send_ABR(int opNumber) {
  send_ABR(opNumber, TERMINAL_A);
}

void send_ABR() {
  send_ABR(get_current_operation_number(), TERMINAL_A);
}

void send_DIS(Terminal terminal) {
  int messageLength;
  std::vector<byte> message = create_VTK_message("DIS", get_current_operation_number(), messageLength);

  UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
  UART0_DEBUG_PORT.printf("│ ► К %s │ Команда: DIS (отключить терминал)\n", getTerminalName(terminal));
  UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");

  // Отправка сообщения на конкретный терминал
  send_message(message.data(), messageLength + 2, terminal);

  // Память освободится автоматически (RAII)
}

// Старая функция для совместимости
void send_DIS() {
  send_DIS(TERMINAL_A);
}

void send_VRP(long amount, Terminal terminal) {
  int messageLength;
  std::map<int, std::vector<byte>> params;

  // Преобразуем сумму в строку
  String amountStr = String(amount);
  
  // Создаем вектор байтов из строки
  std::vector<byte> amountBytes;
  for (int i = 0; i < amountStr.length(); i++) {
      amountBytes.push_back(amountStr.charAt(i));
  }

  params[0x04] = amountBytes;
  increment_operation_number();
  increment_operation_number();
  std::vector<byte> message = create_VTK_message("VRP", get_current_operation_number(), messageLength, params);

  UART0_DEBUG_PORT.printf("│ К %s │ VRP │ Операция: #", getTerminalName(terminal));
  UART0_DEBUG_PORT.print(get_current_operation_number());
  UART0_DEBUG_PORT.print(" │ Сумма: ");
  UART0_DEBUG_PORT.print((float)amount / 100.0, 2);
  UART0_DEBUG_PORT.println(" руб.");
  UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");

  // Отправка сообщения на конкретный терминал
  send_message(message.data(), messageLength + 2, terminal);
  
  // Обновляем таймер "последнего ответа" чтобы дать время терминалу обработать VRP
  if (terminal == TERMINAL_A) {
      update_terminal_A_last_response();
  } else {
      update_terminal_B_last_response();
  }

  // Память освободится автоматически (RAII)
}

// Старая функция для совместимости
void send_VRP(long amount) {
  send_VRP(amount, TERMINAL_A);
}

void sendREFUND(int amount, int operationNumber, Terminal terminal) {
    // Сумма должна быть отрицательной для возврата
    amount = -amount;
    
    // Формируем параметры сообщения
    std::map<int, std::vector<byte>> params;
    
    // Форматируем сумму в ASCII (12 символов)
    std::vector<byte> amountBytes;
    amountBytes.push_back((amount / 1000000) + '0');
    amountBytes.push_back(((amount / 100000) % 10) + '0');
    amountBytes.push_back(((amount / 10000) % 10) + '0');
    amountBytes.push_back(((amount / 1000) % 10) + '0');
    amountBytes.push_back(((amount / 100) % 10) + '0');
    amountBytes.push_back(((amount / 10) % 10) + '0');
    amountBytes.push_back((amount % 10) + '0');
    params[0x04] = amountBytes;
    
    // Добавляем номер операции (8 цифр)
    std::vector<byte> opNumberBytes;
    opNumberBytes.push_back((operationNumber / 10000000) + '0');
    opNumberBytes.push_back((operationNumber / 1000000) % 10 + '0');
    opNumberBytes.push_back((operationNumber / 100000) % 10 + '0');
    opNumberBytes.push_back((operationNumber / 10000) % 10 + '0');
    opNumberBytes.push_back((operationNumber / 1000) % 10 + '0');
    opNumberBytes.push_back((operationNumber / 100) % 10 + '0');
    opNumberBytes.push_back((operationNumber / 10) % 10 + '0');
    opNumberBytes.push_back(operationNumber % 10 + '0');
    params[0x03] = opNumberBytes;
    
    // Добавляем имя сообщения
    std::vector<byte> messageName = {'V', 'R', 'P'};
    params[0x01] = messageName;
    
    // Создаем сообщение
    int messageLength;
    std::vector<byte> message = create_VTK_message("VRP", operationNumber, messageLength, params);
    
    // Логирование
    UART0_DEBUG_PORT.printf("│ К %s │ REFUND │ Сумма: ", getTerminalName(terminal));
    UART0_DEBUG_PORT.print(amount);
    UART0_DEBUG_PORT.print(" │ Операция: ");
    UART0_DEBUG_PORT.println(operationNumber);
    
    // Отправка сообщения на конкретный терминал
    send_message(message.data(), messageLength + 2, terminal);
    
    // Память освободится автоматически (RAII)
}

// Старая функция для совместимости
void sendREFUND(int amount, int operationNumber) {
    sendREFUND(amount, operationNumber, TERMINAL_A);
}

void send_FIN(float amount, int opNumber, Terminal terminal){
  int messageLength;
  std::map<int, std::vector<byte>> params;

  // Преобразуем сумму в строку
  String amountStr = String(amount);
  
  // Создаем вектор байтов из строки
  std::vector<byte> amountBytes;
  for (int i = 0; i < amountStr.length(); i++) {
      amountBytes.push_back(amountStr.charAt(i));
  }

  params[0x04] = amountBytes;
  std::vector<byte> message = create_VTK_message("FIN", opNumber, messageLength, params);

  UART0_DEBUG_PORT.printf("│ К %s │ FIN │ Операция: #", getTerminalName(terminal));
  UART0_DEBUG_PORT.print(opNumber);
  UART0_DEBUG_PORT.print(" │ Списано: ");
  UART0_DEBUG_PORT.print((float)amount / 100.0, 2);
  UART0_DEBUG_PORT.println(" руб. (остаток вернется)");
  UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");

  // Отправка сообщения на конкретный терминал
  send_message(message.data(), messageLength + 2, terminal);
  
  // Обновляем таймер "последнего ответа" чтобы дать время терминалу обработать FIN
  // (терминалу нужно до 20 сек на обработку FIN и возврат)
  if (terminal == TERMINAL_A) {
      update_terminal_A_last_response();
  } else {
      update_terminal_B_last_response();
  }

  // Память освободится автоматически (RAII)
}

// Старая функция для совместимости
void send_FIN(float amount, int opNumber){
  send_FIN(amount, opNumber, TERMINAL_A);
}

void increment_operation_number() {
  int currentOpNumber;

  noInterrupts();
  currentOpNumber = get_current_operation_number();
  operationNumber = (currentOpNumber < MAX_OPERATION_NUMBER) ? currentOpNumber + 1 : 0;
  interrupts();
}

int get_current_operation_number() {
  int currentNumber;

  noInterrupts();
  currentNumber = operationNumber;
  interrupts();

  return currentNumber;
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

// Внешние флаги для раздельного управления IDLE
extern bool stayIDLE_A;
extern bool stayIDLE_B;

void terminal_stay_IDLE(){
    if (millis() - lastSendTime > IDL_INTERVAL) {
      bool sentAny = false;
      
      // ========== ТЕРМИНАЛ A ==========
      // Отправляем IDL только если флаг включен И терминал не занят транзакцией
      if (stayIDLE_A && !is_terminal_transaction_active(TERMINAL_A)) {
        if (idleLogEnabled) {
          UART0_DEBUG_PORT.printf("Периодическая отправка на ТЕРМИНАЛ A: сообщение №%d\n", message_counter);
        }
        send_IDL(TERMINAL_A);
        sentAny = true;
      } else if (stayIDLE_A && is_terminal_transaction_active(TERMINAL_A) && idleLogEnabled) {
        UART0_DEBUG_PORT.println("Пропуск IDL для ТЕРМИНАЛ A: активная транзакция");
      }
      
      // ========== ТЕРМИНАЛ B ==========
      // Отправляем IDL только если флаг включен И терминал не занят транзакцией
      if (stayIDLE_B && !is_terminal_transaction_active(TERMINAL_B)) {
        if (idleLogEnabled) {
          UART0_DEBUG_PORT.printf("Периодическая отправка на ТЕРМИНАЛ B: сообщение №%d\n", message_counter);
        }
        send_IDL(TERMINAL_B);
        sentAny = true;
      } else if (stayIDLE_B && is_terminal_transaction_active(TERMINAL_B) && idleLogEnabled) {
        UART0_DEBUG_PORT.println("Пропуск IDL для ТЕРМИНАЛ B: активная транзакция");
      }
      
      if (sentAny) {
        lastSendTime = millis();
        message_counter = (message_counter % 99999) + 1;
      }
  }
}

/*
----------------------------------------------------------------
  МОНИТОРИНГ ДОСТУПНОСТИ ТЕРМИНАЛОВ (РАЗДЕЛЬНО ДЛЯ A И B)
----------------------------------------------------------------
Каждый терминал считается недоступным, если не было ответа более TERMINAL_TIMEOUT_MS.
При недоступности отправляется уведомление (через HTTP).
----------------------------------------------------------------
*/

// ========== ТЕРМИНАЛ A ==========
static unsigned long lastTerminalA_Response = 0;
static bool terminalA_Available = true;
static bool terminalA_AlertSent = false;

// ========== ТЕРМИНАЛ B ==========
static unsigned long lastTerminalB_Response = 0;
static bool terminalB_Available = true;
static bool terminalB_AlertSent = false;

// ========== ОБЩИЕ ==========
static unsigned long lastAvailabilityCheck = 0;

const unsigned long TERMINAL_TIMEOUT_MS = 60000;        // 60 секунд без ответа = недоступен
const unsigned long AVAILABILITY_CHECK_INTERVAL = 5000; // Проверять каждые 5 секунд

// Вызывать при получении ответа от ТЕРМИНАЛА A
void update_terminal_A_last_response() {
    lastTerminalA_Response = millis();
    
    // Если терминал A был недоступен — восстановился
    if (!terminalA_Available) {
        terminalA_Available = true;
        terminalA_AlertSent = false;
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ✓ ТЕРМИНАЛ A: Связь восстановлена!");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        // Отправляем уведомление о восстановлении через MQTT
        mqtt_send_debug(
            "[A] Терминал восстановлен",
            "Связь с терминалом A восстановлена"
        );
    }
}

// Вызывать при получении ответа от ТЕРМИНАЛА B
void update_terminal_B_last_response() {
    lastTerminalB_Response = millis();
    
    // Если терминал B был недоступен — восстановился
    if (!terminalB_Available) {
        terminalB_Available = true;
        terminalB_AlertSent = false;
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ✓ ТЕРМИНАЛ B: Связь восстановлена!");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        // Отправляем уведомление о восстановлении через MQTT
        mqtt_send_debug(
            "[B] Терминал восстановлен",
            "Связь с терминалом B восстановлена"
        );
    }
}

// Старая функция для совместимости (обновляет терминал A)
void update_terminal_last_response() {
    update_terminal_A_last_response();
}

// Проверка доступности ОБОИХ терминалов (вызывать в loop)
void check_terminal_availability() {
    unsigned long now = millis();
    
    // Не проверяем слишком часто
    if (now - lastAvailabilityCheck < AVAILABILITY_CHECK_INTERVAL) {
        return;
    }
    lastAvailabilityCheck = now;
    
    // ========== ПРОВЕРКА ТЕРМИНАЛА A ==========
    // Если ещё не было ни одного ответа — пропускаем (терминал только стартует)
    if (lastTerminalA_Response != 0) {
        // ВАЖНО: Не проверяем если:
        // - идет транзакция (оплата, зарядка, возврат)
        // - терминал отключен из-за счетчика
        // - терминал намеренно отключен нами (stayIDLE_A = false)
        if (is_terminal_transaction_active(TERMINAL_A) || is_terminal_disabled_due_to_meter() || !stayIDLE_A) {
            if (!terminalA_Available) {
                terminalA_Available = true;
                terminalA_AlertSent = false;
            }
        } else {
            // Проверяем таймаут для терминала A
            if (now - lastTerminalA_Response > TERMINAL_TIMEOUT_MS) {
                if (terminalA_Available) {
                    terminalA_Available = false;
                    UART0_DEBUG_PORT.println();
                    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
                    UART0_DEBUG_PORT.println("║ ⚠️ ТЕРМИНАЛ A: Не отвечает!");
                    UART0_DEBUG_PORT.printf("║ Последний ответ был %lu сек назад\n", (now - lastTerminalA_Response) / 1000);
                    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
                }
                
                // Отправляем уведомление только один раз через MQTT
                if (!terminalA_AlertSent) {
                    terminalA_AlertSent = true;
                    unsigned long offlineSec = (now - lastTerminalA_Response) / 1000;
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Терминал A не отвечает %lu сек", offlineSec);
                    mqtt_send_debug(
                        "[A] ОШИБКА: Терминал недоступен!",
                        msg,
                        "Проверьте подключение терминала A"
                    );
                }
            }
        }
    }
    
    // ========== ПРОВЕРКА ТЕРМИНАЛА B ==========
    // Если ещё не было ни одного ответа — пропускаем (терминал только стартует)
    if (lastTerminalB_Response != 0) {
        // ВАЖНО: Не проверяем если:
        // - идет транзакция (оплата, зарядка, возврат)
        // - терминал отключен из-за счетчика
        // - терминал намеренно отключен нами (stayIDLE_B = false)
        if (is_terminal_transaction_active(TERMINAL_B) || is_terminal_disabled_due_to_meter() || !stayIDLE_B) {
            if (!terminalB_Available) {
                terminalB_Available = true;
                terminalB_AlertSent = false;
            }
        } else {
            // Проверяем таймаут для терминала B
            if (now - lastTerminalB_Response > TERMINAL_TIMEOUT_MS) {
                if (terminalB_Available) {
                    terminalB_Available = false;
                    UART0_DEBUG_PORT.println();
                    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
                    UART0_DEBUG_PORT.println("║ ⚠️ ТЕРМИНАЛ B: Не отвечает!");
                    UART0_DEBUG_PORT.printf("║ Последний ответ был %lu сек назад\n", (now - lastTerminalB_Response) / 1000);
                    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
                }
                
                // Отправляем уведомление только один раз через MQTT
                if (!terminalB_AlertSent) {
                    terminalB_AlertSent = true;
                    unsigned long offlineSec = (now - lastTerminalB_Response) / 1000;
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Терминал B не отвечает %lu сек", offlineSec);
                    mqtt_send_debug(
                        "[B] ОШИБКА: Терминал недоступен!",
                        msg,
                        "Проверьте подключение терминала B"
                    );
                }
            }
        }
    }
}

// Текущий статус терминалов (возвращает true если хотя бы один доступен)
bool is_terminal_available() {
    return (terminalA_Available || terminalB_Available);
}

// Статус конкретного терминала A
bool is_terminal_A_available() {
    return terminalA_Available;
}

// Статус конкретного терминала B
bool is_terminal_B_available() {
    return terminalB_Available;
}

/*
----------------------------------------------------------------
  МОНИТОРИНГ WiFi — ОТКЛЮЧЕНИЕ ТЕРМИНАЛА ПРИ ПОТЕРЕ ИНТЕРНЕТА
----------------------------------------------------------------
Если ESP потеряла WiFi, терминал не должен принимать оплату,
потому что транзакция не будет обработана на сервере.
При потере WiFi отправляем DIS, при восстановлении — IDL.
----------------------------------------------------------------
*/

static bool wifiWasConnected = false;                      // Предыдущее состояние WiFi
static unsigned long lastWifiCheck = 0;                    // Время последней проверки
static bool terminalDisabledDueToWifi = false;             // Терминал отключён из-за WiFi/интернета
static unsigned long lastWifiReconnectAttempt = 0;         // Время последней попытки переподключения

static bool connectivityWasOk = false;                     // Предыдущее состояние "WiFi+интернет OK"
static bool internetAvailable = false;                     // Текущее знание о доступности интернета
static unsigned long lastInternetCheck = 0;                // Время последней проверки интернета
static bool firstConnectivityCheck = true;                 // Флаг первой проверки связи
static uint8_t internetFailCount = 0;                      // Счётчик последовательных неудачных проверок

const unsigned long WIFI_CHECK_INTERVAL = 5000;            // Проверять WiFi каждые 5 секунд
const unsigned long WIFI_RECONNECT_INTERVAL = 15000;       // Пытаться переподключиться раз в 15 секунд
const unsigned long INTERNET_CHECK_INTERVAL = 30000;       // Проверять интернет раз в 30 секунд
const uint8_t INTERNET_FAIL_THRESHOLD = 5;                 // Сколько неудач подряд = потеря интернета (5 × 30сек = 2.5 мин)

// Проверка WiFi и управление терминалом (вызывать в loop)
void check_wifi_and_manage_terminal() {
    unsigned long now = millis();
    
    // Не проверяем слишком часто
    if (now - lastWifiCheck < WIFI_CHECK_INTERVAL) {
        return;
    }
    lastWifiCheck = now;
    
    IPAddress ip = WiFi.localIP();
    bool wifiConnected = (WiFi.status() == WL_CONNECTED && ip != IPAddress(0, 0, 0, 0));

    // Обновляем знание о доступности интернета только при наличии WiFi-соединения
    if (wifiConnected) {
        // Первая проверка сразу (lastInternetCheck == 0), далее по интервалу
        if (lastInternetCheck == 0 || (now - lastInternetCheck > INTERNET_CHECK_INTERVAL)) {
            lastInternetCheck = now;
            
            // Используем статус MQTT вместо блокирующего HTTP запроса
            bool checkResult = is_mqtt_connected();
            
            if (checkResult) {
                // Успешная проверка — сбрасываем счётчик неудач
                internetFailCount = 0;
                
                // Восстановление интернета — логируем только при изменении состояния
                if (!internetAvailable) {
                    internetAvailable = true;
                    UART0_DEBUG_PORT.println("[INET] Интернет восстановлен!");
                }
            } else {
                internetFailCount++;
                UART0_DEBUG_PORT.printf("[INET] Проверка неудачна (%d/%d)\n", internetFailCount, INTERNET_FAIL_THRESHOLD);
                
                // Интернет считается потерянным только после N неудач подряд
                if (internetFailCount >= INTERNET_FAIL_THRESHOLD && internetAvailable) {
                    internetAvailable = false;
                    UART0_DEBUG_PORT.println("[INET] Интернет НЕДОСТУПЕН! (подтверждено)");
                }
            }
        }
    } else {
        internetAvailable = false;
        internetFailCount = INTERNET_FAIL_THRESHOLD;  // При потере WiFi сразу сбрасываем
    }

    bool connectivityOk = wifiConnected && internetAvailable;
    
    // При первой проверке: если интернета нет — сразу отключаем терминалы
    if (firstConnectivityCheck && lastInternetCheck > 0) {
        firstConnectivityCheck = false;
        if (!connectivityOk) {
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.println("║ ⚠️ Нет интернета при старте!");
            UART0_DEBUG_PORT.println("║ Отключаем оба терминала (DIS)...");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
            send_DIS(TERMINAL_A);
            send_DIS(TERMINAL_B);
            terminalDisabledDueToWifi = true;
            stayIDLE_A = false;
            stayIDLE_B = false;
        }
    }
    
    // Связь с сервером (WiFi или интернет) пропала — останавливаем зарядки и отключаем терминалы
    if (connectivityWasOk && !connectivityOk) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ⚠️ Потеря WiFi/интернета!");
        UART0_DEBUG_PORT.println("║ Останавливаем все зарядки и отключаем терминалы...");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        // КРИТИЧНО: Сначала останавливаем все активные зарядки!
        emergency_stop_all_charging();
        
        // Затем отключаем терминалы
        send_DIS(TERMINAL_A);
        send_DIS(TERMINAL_B);
        terminalDisabledDueToWifi = true;
        stayIDLE_A = false;  // Останавливаем периодические IDL для A
        stayIDLE_B = false;  // Останавливаем периодические IDL для B
    }

    // Если WiFi отключен — пробуем переподключиться периодически
    if (!wifiConnected && (now - lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL)) {
        UART0_DEBUG_PORT.println("[WIFI] WiFi отключен, пробуем переподключиться...");
        WiFi.disconnect();
        WiFi.reconnect();
        lastWifiReconnectAttempt = now;
    }
    
    // Связь с сервером восстановилась — включаем терминалы, у которых счётчики работают
    if (!connectivityWasOk && connectivityOk && terminalDisabledDueToWifi) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ✓ WiFi/интернет восстановлены!");
        
        // Проверяем состояние счётчиков перед включением терминалов
        bool meterA_ok = !is_terminal_A_disabled_due_to_meter();
        bool meterB_ok = !is_terminal_B_disabled_due_to_meter();
        
        if (meterA_ok) {
            UART0_DEBUG_PORT.println("║ Включаем терминал A (IDL)...");
            send_IDL(TERMINAL_A);
            stayIDLE_A = true;
        } else {
            UART0_DEBUG_PORT.println("║ ⚠️ Терминал A НЕ включён: счётчик недоступен");
        }
        
        if (meterB_ok) {
            UART0_DEBUG_PORT.println("║ Включаем терминал B (IDL)...");
            send_IDL(TERMINAL_B);
            stayIDLE_B = true;
        } else {
            UART0_DEBUG_PORT.println("║ ⚠️ Терминал B НЕ включён: счётчик недоступен");
        }
        
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        terminalDisabledDueToWifi = false;
    }
    
    wifiWasConnected = wifiConnected;
    connectivityWasOk = connectivityOk;
}

// Возвращает true, если терминал отключён из-за отсутствия WiFi
bool is_terminal_disabled_due_to_wifi() {
    return terminalDisabledDueToWifi;
}