#include "main.h"
#include <WiFi.h>

extern bool stayIDLE_A;
extern bool stayIDLE_B;

// ========== UART0 для терминала B (аппаратный RS232) ==========
// Serial занят USB CDC, поэтому создаём новый объект для физического UART0
HardwareSerial TerminalB_Serial(0);  // 0 = UART0

volatile int operationNumber = 0;

// Буферы для двух терминалов
uint8_t receiveBufferA[BUFFER_SIZE];
uint8_t receiveBufferB[BUFFER_SIZE];
int bufferIndexA = 0;
int bufferIndexB = 0;

// Старый глобальный буфер для совместимости
uint8_t receiveBuffer[BUFFER_SIZE];
int bufferIndex = 0;

static unsigned long lastRXTimeA = 0;
static unsigned long lastRXTimeB = 0;
static unsigned long lastRXTime = 0;  // Для совместимости

// Глобальная переменная для отслеживания источника данных (для payments.cpp)
extern Terminal currentReceivingTerminal;


const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;

const int MIN_MESSAGE_SIZE = 10;
const byte START_BYTE = 0x1F;                       // Стартовый байт

//SoftwareSerial SOFTSERIAL_ENERGY_PORT(RX2_PIN, TX2_PIN); //для связи с энергосчетчика



void UART_Setup(){

    UART0_DEBUG_PORT.setDebugOutput(false);    // пусть ядро логает в USB
    UART2_ENERGY_READ_PORT.setDebugOutput(false);  // на всякий случай запретим тут

    UART0_DEBUG_PORT.begin(UART0_DEBUG_PORT_BAUDRATE);
    
    // ========== Терминал A (UART1 - аппаратный RS232) ==========
    UART1_TERMINAL_A_PORT.begin(UART1_TERMINAL_A_BAUDRATE, SERIAL_8N1, UART1_TERMINAL_A_RX_PIN, UART1_TERMINAL_A_TX_PIN);
    
    // ========== Терминал B (UART0 - аппаратный RS232) ==========
    TerminalB_Serial.begin(UART0_TERMINAL_B_BAUDRATE, SERIAL_8N1, UART0_TERMINAL_B_RX_PIN, UART0_TERMINAL_B_TX_PIN);
    
    // ========== Modbus счетчик (UART2 - RS485) ==========
    UART2_ENERGY_READ_PORT.begin(UART2_ENERGY_READ_PORT_BAUDRATE, SERIAL_8E1, UART2_ENERGY_READ_PORT_RX_PIN, UART2_ENERGY_READ_PORT_TX_PIN);

    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ 🚀 СИСТЕМА ЗАПУЩЕНА");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.println("║ Debug: USB CDC (Serial)");
    UART0_DEBUG_PORT.printf("║ Терминал A: UART1 RS232 (RX=%d, TX=%d)\n", UART1_TERMINAL_A_RX_PIN, UART1_TERMINAL_A_TX_PIN);
    UART0_DEBUG_PORT.printf("║ Терминал B: UART0 RS232 (RX=%d, TX=%d)\n", UART0_TERMINAL_B_RX_PIN, UART0_TERMINAL_B_TX_PIN);
    UART0_DEBUG_PORT.printf("║ Modbus: UART2 RS485 (RX=%d, TX=%d)\n", UART2_ENERGY_READ_PORT_RX_PIN, UART2_ENERGY_READ_PORT_TX_PIN);
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void printHexFrame(const uint8_t *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) UART0_DEBUG_PORT.print('0');
    UART0_DEBUG_PORT.print(buf[i], HEX);
    UART0_DEBUG_PORT.print(' ');
  }
  UART0_DEBUG_PORT.println();
}


// ========== ПРИЁМ ОТ ТЕРМИНАЛА A ==========
void UART_TerminalA_received_data() {
  // Приём байтов от терминала A
  while (UART1_TERMINAL_A_PORT.available() > 0) {
    if (bufferIndexA < BUFFER_SIZE) {
      receiveBufferA[bufferIndexA++] = UART1_TERMINAL_A_PORT.read();
      lastRXTimeA = millis();
    } else {
      UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
      UART0_DEBUG_PORT.println("║ ✗ ТЕРМИНАЛ A: Буфер переполнен!");
      UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
      bufferIndexA = 0;
    }
  }

  // Как только поток "замолчал" на небольшое время — считаем кадр законченным.
  if (bufferIndexA > 0 && ((millis() - lastRXTimeA) > 100)) {
      if (idleLogEnabled) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
        UART0_DEBUG_PORT.print("│ [ТЕРМИНАЛ A] RX (");
        UART0_DEBUG_PORT.print(bufferIndexA);
        UART0_DEBUG_PORT.println(" байт):");
        UART0_DEBUG_PORT.print("│ ");
        printHexFrame(receiveBufferA, bufferIndexA);
        UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");
      }

      // Копируем в глобальный буфер для обработки (временно для совместимости)
      memcpy(receiveBuffer, receiveBufferA, bufferIndexA);
      bufferIndex = bufferIndexA;
      
      // Указываем что данные от терминала A
      currentReceivingTerminal = TERMINAL_A;
      process_POS_received_data();
      bufferIndexA = 0;
  }
}

// ========== ПРИЁМ ОТ ТЕРМИНАЛА B ==========
void UART_TerminalB_received_data() {
  // Приём байтов от терминала B
  while (TerminalB_Serial.available() > 0) {
    if (bufferIndexB < BUFFER_SIZE) {
      receiveBufferB[bufferIndexB++] = TerminalB_Serial.read();
      lastRXTimeB = millis();
    } else {
      UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
      UART0_DEBUG_PORT.println("║ ✗ ТЕРМИНАЛ B: Буфер переполнен!");
      UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
      bufferIndexB = 0;
    }
  }

  // Как только поток "замолчал" на небольшое время — считаем кадр законченным.
  if (bufferIndexB > 0 && ((millis() - lastRXTimeB) > 100)) {
      if (idleLogEnabled) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
        UART0_DEBUG_PORT.print("│ [ТЕРМИНАЛ B] RX (");
        UART0_DEBUG_PORT.print(bufferIndexB);
        UART0_DEBUG_PORT.println(" байт):");
        UART0_DEBUG_PORT.print("│ ");
        printHexFrame(receiveBufferB, bufferIndexB);
        UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");
      }

      // Копируем в глобальный буфер для обработки (временно для совместимости)
      memcpy(receiveBuffer, receiveBufferB, bufferIndexB);
      bufferIndex = bufferIndexB;
      
      // Указываем что данные от терминала B
      currentReceivingTerminal = TERMINAL_B;
      process_POS_received_data();
      bufferIndexB = 0;
  }
}

// Старая функция для совместимости
void UART_POS_received_data() {
  UART_TerminalA_received_data();
}


/*
----------------------------------------------------------------
  ХЕЛПЕР: ПАРСИНГ ТЕРМИНАЛА ИЗ КОМАНДЫ
----------------------------------------------------------------
*/

// Парсит терминал из команды. Возвращает TERMINAL_A или TERMINAL_B.
// Формат: "команда A" или "команда B", по умолчанию A
// Также удаляет суффикс терминала из команды
Terminal parseTerminalFromCommand(String &command) {
  command.trim();
  command.toUpperCase();
  
  // Проверяем последний символ
  if (command.endsWith(" A")) {
    command = command.substring(0, command.length() - 2);
    command.trim();
    return TERMINAL_A;
  } else if (command.endsWith(" B")) {
    command = command.substring(0, command.length() - 2);
    command.trim();
    return TERMINAL_B;
  }
  
  // По умолчанию терминал A
  return TERMINAL_A;
}

/*
----------------------------------------------------------------
  ОБРАБОТКА КОМАНД ИЗ SERIAL
----------------------------------------------------------------
*/

void UART_Commands_processing(){
if (UART0_DEBUG_PORT.available() > 0) {
  String command = UART0_DEBUG_PORT.readStringUntil('\n');
  command.trim();

  if (command == "IDLE") {
    // Переключаем оба терминала одновременно
    stayIDLE_A = !stayIDLE_A;
    stayIDLE_B = !stayIDLE_B;
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.printf("║ Режим stay-IDLE: %s\n", stayIDLE_A ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.printf("║ ТЕРМИНАЛ A: %s\n", stayIDLE_A ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.printf("║ ТЕРМИНАЛ B: %s\n", stayIDLE_B ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

  } else if (command == "IDLOG") {
    idleLogEnabled = !idleLogEnabled;
    UART0_DEBUG_PORT.print("Лог периодических IDL-сообщений: ");
    UART0_DEBUG_PORT.println(idleLogEnabled ? "ВКЛ" : "ВЫКЛ");

  } else if (command.startsWith("IDL")) {
    // IDL, IDL A, IDL B
    String cmd = command;
    Terminal terminal = parseTerminalFromCommand(cmd);
    
    if (cmd == "IDL") {
      send_IDL(terminal);
      UART0_DEBUG_PORT.printf("✓ IDL отправлен на %s\n", (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");
    } else if (cmd == "IDLM") {
      // IDL с POS Management Data — по умолчанию 'S','N','N' (закрытие дня/settlement)
      send_IDL_with_management_data('S', 'N', 'N', terminal);
      UART0_DEBUG_PORT.printf("✓ IDLM отправлен на %s\n", (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");
    }

  } else if (command.startsWith("ABR")) {
    // ABR, ABR A, ABR B, ABR 123, ABR 123 A, ABR 123 B
    String cmd = command;
    Terminal terminal = parseTerminalFromCommand(cmd);
    
    // Теперь cmd содержит команду без суффикса терминала
    int spacePos = cmd.indexOf(' ');
    if (spacePos == -1) {
      // ABR без номера - отменяем текущую операцию
      send_ABR(terminal);
      UART0_DEBUG_PORT.printf("✓ ABR отправлен на %s\n", (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");
    } else {
      String opStr = cmd.substring(spacePos + 1);
      int opNumber = opStr.toInt();
      if (opNumber > 0) {
        send_ABR(opNumber, terminal);
        UART0_DEBUG_PORT.printf("✓ ABR #%d отправлен на %s\n", opNumber, (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");
      } else {
        UART0_DEBUG_PORT.println("✗ Ошибка: неверный номер операции для ABR");
      }
    }

  } else if (command.startsWith("DIS")) {
    // DIS, DIS A, DIS B
    String cmd = command;
    Terminal terminal = parseTerminalFromCommand(cmd);
    send_DIS(terminal);
    UART0_DEBUG_PORT.printf("✓ DIS отправлен на %s\n", (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");

  } else if (command.startsWith("VRP")) {
    // VRP 1000, VRP 1000 A, VRP 1000 B
    String cmd = command;
    Terminal terminal = parseTerminalFromCommand(cmd);
    
    int spacePos = cmd.indexOf(' ');
    if (spacePos != -1) {
      String amountStr = cmd.substring(spacePos + 1);
      long amount = amountStr.toInt();
      if (amount > 0 && amount <= 1000000) {
        send_VRP(amount, terminal);
        UART0_DEBUG_PORT.printf("✓ VRP %.2f руб отправлен на %s\n", 
                                (float)amount / 100.0, 
                                (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");
      } else {
        UART0_DEBUG_PORT.print("✗ Ошибка: диапазон платежа (1-1000000 коп.) ");
        UART0_DEBUG_PORT.println(amount);
      }
    } else {
      UART0_DEBUG_PORT.println("✗ Ошибка: укажите сумму (VRP <сумма> [A|B])");
    }

  } else if (command.startsWith("PAY")) {
    int spacePos = command.indexOf(' ');
    if (spacePos != -1) {
      String amountStr = command.substring(spacePos + 1);
      long amount = amountStr.toInt();
      start_payment(amount);
    }

  } else if (command.startsWith("REFUND")) {
    int spacePos = command.indexOf(' ');
    if (spacePos != -1) {
      String params = command.substring(spacePos + 1);
      int secondSpace = params.indexOf(' ');
      if (secondSpace != -1) {
        int amount = params.substring(0, secondSpace).toInt();
        int operationNumber = params.substring(secondSpace + 1).toInt();
        if (amount > 0 && amount <= 1000000 && operationNumber > 0) {
          // sendREFUND(amount, operationNumber);
        } else {
          UART0_DEBUG_PORT.println("Ошибка: неверные параметры возврата");
        }
      }
    }

  } else if (command.startsWith("FIN")) {
    // FIN, FIN A, FIN B, FIN 500, FIN 500 A, FIN 500 123, FIN 500 123 A, FIN 500 123 B
    String cmd = command;
    Terminal terminal = parseTerminalFromCommand(cmd);
    
    String params = cmd.substring(3);
    params.trim();

    float amount = 0;
    int opNumber = 0;

    if (params.length() == 0) {
      // FIN без параметров: FIN(0) для текущей операции
      opNumber = get_current_operation_number();
    } else {
      int spacePos = params.indexOf(' ');
      if (spacePos == -1) {
        // Только сумма: FIN <amount> (копейки), номер операции возьмём текущий
        amount = params.toInt();
        opNumber = get_current_operation_number();
      } else {
        String amountStr = params.substring(0, spacePos);
        String opStr     = params.substring(spacePos + 1);
        amount   = amountStr.toInt();
        opNumber = opStr.toInt();
      }
    }

    if (opNumber <= 0) {
      UART0_DEBUG_PORT.println("✗ Ошибка: неверный номер операции для FIN");
    } else {
      send_FIN(amount, opNumber, terminal);
      UART0_DEBUG_PORT.printf("✓ FIN %.2f руб (оп #%d) отправлен на %s\n", 
                              (float)amount / 100.0, 
                              opNumber,
                              (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B");
    }

  } else if (command.startsWith("HEX")) {
    String hexString = command.substring(8);
    send_HEX(hexString);
  } else if (command.startsWith("dcdHEX")) {
    String hexString = command.substring(10);
    if (hexString.length() > 0) {
      decode_HEX(hexString);
    } else {
      UART0_DEBUG_PORT.println("Ошибка: пустая HEX-строка");
    }

  } else if (command.startsWith("CRC")) {
    String hexString = command.substring(8);
    if (hexString.length() > 0) {
      calculate_CRC(hexString);
    } else {
      UART0_DEBUG_PORT.println("Ошибка: пустая HEX-строка");
    }

  } else if (command == "STATUS" || command == "STAT") {
    // Статус системы и терминалов
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ 📊 СТАТУС СИСТЕМЫ");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.print("║ Терминалы доступны: ");
    UART0_DEBUG_PORT.println(is_terminal_available() ? "ДА" : "НЕТ");
    UART0_DEBUG_PORT.print("║ Режим stayIDLE A: ");
    UART0_DEBUG_PORT.println(stayIDLE_A ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.print("║ Режим stayIDLE B: ");
    UART0_DEBUG_PORT.println(stayIDLE_B ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.print("║ Лог периодических IDL: ");
    UART0_DEBUG_PORT.println(idleLogEnabled ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.print("║ Режим отладки терминала: ");
    UART0_DEBUG_PORT.println(terminalDebugMode ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    
    // WiFi статус (теперь считаем "подключён", только если есть непустой IP)
    IPAddress ip = WiFi.localIP();
    bool wifiOk = (WiFi.status() == WL_CONNECTED && ip != IPAddress(0, 0, 0, 0));

    UART0_DEBUG_PORT.print("║ WiFi подключён: ");
    UART0_DEBUG_PORT.println(wifiOk ? "ДА" : "НЕТ");
    if (wifiOk) {
      UART0_DEBUG_PORT.print("║ IP адрес: ");
      UART0_DEBUG_PORT.println(ip);
    }
    UART0_DEBUG_PORT.print("║ Терминал отключён из-за WiFi: ");
    UART0_DEBUG_PORT.println(is_terminal_disabled_due_to_wifi() ? "ДА" : "НЕТ");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.printf("║ ТЕРМИНАЛ A: UART1 (RX=%d, TX=%d)\n", UART1_TERMINAL_A_RX_PIN, UART1_TERMINAL_A_TX_PIN);
    UART0_DEBUG_PORT.printf("║ ТЕРМИНАЛ B: UART0 (RX=%d, TX=%d)\n", UART0_TERMINAL_B_RX_PIN, UART0_TERMINAL_B_TX_PIN);
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

  } else if (command == "MQTT") {
    // Статус MQTT
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ 📡 MQTT СТАТУС");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.printf("║ Сервер: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    UART0_DEBUG_PORT.printf("║ Station ID: %d\n", STATION_ID);
    UART0_DEBUG_PORT.printf("║ Подключен: %s\n", is_mqtt_connected() ? "ДА ✓" : "НЕТ ✗");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.printf("║ Topic commands: stations/%d/commands\n", STATION_ID);
    UART0_DEBUG_PORT.printf("║ Topic status: stations/%d/status\n", STATION_ID);
    UART0_DEBUG_PORT.printf("║ Topic response: stations/%d/response\n", STATION_ID);
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

  } else if (command == "MQTTPUB") {
    // Принудительная публикация статуса
    UART0_DEBUG_PORT.println("Публикация MQTT статуса...");
    mqtt_publish_status();

  } else if (command.startsWith("WIFI")) {
    // Установка WiFi SSID и пароля: WIFI <ssid> <password>
    String params = command.substring(4);
    params.trim();
    process_wifi_command(params);

  } else if (command.startsWith("APIKEY")) {
    // Установка или просмотр API ключа
    String params = command.substring(6);
    params.trim();
    
    if (params.length() == 0) {
      // Показать текущий статус ключа (не сам ключ!)
      UART0_DEBUG_PORT.println();
      UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
      UART0_DEBUG_PORT.println("║ 🔑 API КЛЮЧ СТАНЦИИ");
      UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
      if (is_api_key_set()) {
        const char* key = get_api_key();
        int len = strlen(key);
        UART0_DEBUG_PORT.printf("║ Статус: УСТАНОВЛЕН (%d символов)\n", len);
        // Показываем первые и последние 4 символа
        if (len > 8) {
          UART0_DEBUG_PORT.printf("║ Ключ: %.4s...%.4s\n", key, key + len - 4);
        }
      } else {
        UART0_DEBUG_PORT.println("║ Статус: НЕ УСТАНОВЛЕН");
        UART0_DEBUG_PORT.println("║ Используйте: APIKEY <ваш_ключ>");
      }
      UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
    } else {
      // Установить новый ключ
      UART0_DEBUG_PORT.println();
      save_api_key(params);
    }

  } else if (command == "HELP" || command == "?") {
    // Справка по командам
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ 📖 СПРАВКА ПО КОМАНДАМ");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.println("║ ОБЩИЕ КОМАНДЫ:");
    UART0_DEBUG_PORT.println("║   STATUS (STAT) - Статус системы");
    UART0_DEBUG_PORT.println("║   IDLE          - Переключить режим stay-IDLE");
    UART0_DEBUG_PORT.println("║   IDLOG         - Переключить лог IDL-сообщений");
    UART0_DEBUG_PORT.println("║   MQTT          - Статус MQTT подключения");
    UART0_DEBUG_PORT.println("║   MQTTPUB       - Принудительно опубликовать статус");
    UART0_DEBUG_PORT.println("║   WIFI              - Показать настройки WiFi");
    UART0_DEBUG_PORT.println("║   WIFI <ssid> <pwd> - Установить WiFi (сохр. в NVS)");
    UART0_DEBUG_PORT.println("║   APIKEY        - Показать статус API ключа");
    UART0_DEBUG_PORT.println("║   APIKEY <ключ> - Установить новый API ключ");
    UART0_DEBUG_PORT.println("║   HELP (?)      - Эта справка");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.println("║ КОМАНДЫ К ТЕРМИНАЛАМ [A|B]:");
    UART0_DEBUG_PORT.println("║   IDL [A|B]        - Отправить IDLE");
    UART0_DEBUG_PORT.println("║   DIS [A|B]        - Отключить терминал");
    UART0_DEBUG_PORT.println("║   VRP <сумма> [A|B]   - Запрос оплаты (копейки)");
    UART0_DEBUG_PORT.println("║   ABR [номер] [A|B]   - Отменить операцию");
    UART0_DEBUG_PORT.println("║   FIN <сумма> <оп> [A|B] - Завершить транзакцию");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.println("║ ПРИМЕРЫ:");
    UART0_DEBUG_PORT.println("║   IDL           - IDL на терминал A");
    UART0_DEBUG_PORT.println("║   IDL B         - IDL на терминал B");
    UART0_DEBUG_PORT.println("║   VRP 1000 A    - Оплата 10 руб на терминал A");
    UART0_DEBUG_PORT.println("║   VRP 2000 B    - Оплата 20 руб на терминал B");
    UART0_DEBUG_PORT.println("║   FIN 500 123 A - Списать 5 руб, операция #123, терминал A");
    UART0_DEBUG_PORT.println("║   DIS B         - Отключить терминал B");
    UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.println("║ ДРУГИЕ:");
    UART0_DEBUG_PORT.println("║   HEX <hex>     - Отправить сырые HEX данные");
    UART0_DEBUG_PORT.println("║   dcdHEX <hex>  - Декодировать HEX");
    UART0_DEBUG_PORT.println("║   CRC <hex>     - Вычислить CRC16");
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

  } else {
    UART0_DEBUG_PORT.println("✗ Неизвестная команда. Введите HELP для справки.");
  }
}

}


/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

// ========== ОТПРАВКА НА ТЕРМИНАЛЫ ==========

// Отправка на конкретный терминал по enum
void send_message(byte* message, int messageLength, Terminal terminal) {
  const char* terminalName = (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B";
  
  // Выбираем порт для отправки
  if (terminal == TERMINAL_A) {
    UART1_TERMINAL_A_PORT.write(message, messageLength);
  } else {
    TerminalB_Serial.write(message, messageLength);
  }

  if (idleLogEnabled) {
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
    UART0_DEBUG_PORT.printf("│ [%s] TX (%d байт):\n", terminalName, messageLength);
    UART0_DEBUG_PORT.print("│ ");
    for (int i = 0; i < messageLength; i++) {
      if (message[i] < 0x10) UART0_DEBUG_PORT.print("0");
      UART0_DEBUG_PORT.print(message[i], HEX);
      UART0_DEBUG_PORT.print(" ");
    }
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");
  }
}

// Отправка на терминал A
void send_message_to_terminal_A(byte* message, int messageLength) {
  send_message(message, messageLength, TERMINAL_A);
}

// Отправка на терминал B
void send_message_to_terminal_B(byte* message, int messageLength) {
  send_message(message, messageLength, TERMINAL_B);
}

// Старая функция для совместимости (отправляет на терминал A)
void send_message(byte* message, int messageLength) {
  send_message(message, messageLength, TERMINAL_A);
}


void send_HEX(const String& hexString) {
  static byte messageBuffer[256]; // Статический для повторного использования
  size_t bufferIndex = 0;
  
  const char* ptr = hexString.c_str();
  size_t length = hexString.length();
  
  // Быстрый парсинг HEX
  for (size_t i = 0; i < length && bufferIndex < sizeof(messageBuffer); ) {
    // Пропускаем не-HEX символы
    while (i < length && !isxdigit(ptr[i])) i++;
    
    if (i + 1 >= length) break;
    
    // Преобразуем HEX
    char high = ptr[i++];
    char low = ptr[i++];
    
    messageBuffer[bufferIndex++] = 
      (hex_char_to_byte(high) << 4) | hex_char_to_byte(low);
  }
  
  // Проверки
  if (bufferIndex < 5) { // Минимальная длина VTK сообщения
    UART0_DEBUG_PORT.println("Ошибка: слишком короткое сообщение");
    return;
  }
  
  if (messageBuffer[0] != 0x1F) {
    UART0_DEBUG_PORT.println("Ошибка: неверный STX байт");
    return;
  }
  
  // Дополнительная проверка длины из заголовка
  if (bufferIndex >= 3) {
    uint16_t declaredLength = (messageBuffer[1] << 8) | messageBuffer[2];
    if (bufferIndex != declaredLength + 5) { // +3 заголовок +2 CRC
      UART0_DEBUG_PORT.println("Предупреждение: несоответствие длины");
    }
  }
  
  // Отправка и логирование
  UART0_DEBUG_PORT.print("Отправка HEX: ");
  UART0_DEBUG_PORT.println(hexString);
  
  UART1_POS_PORT.write(messageBuffer, bufferIndex);
  
  UART0_DEBUG_PORT.print("Отправлено: ");
  for (size_t i = 0; i < bufferIndex; i++) {
    if (messageBuffer[i] < 0x10) UART0_DEBUG_PORT.print("0");
    UART0_DEBUG_PORT.print(messageBuffer[i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println();
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void decode_HEX(const String& hexString) {
  const char* ptr = hexString.c_str();
  size_t length = hexString.length();
  
  int bufferIndex = 0;
  
  // Парсим HEX напрямую в receiveBuffer
  for (size_t i = 0; i < length && bufferIndex < BUFFER_SIZE; ) {
    // Пропускаем не-HEX символы
    while (i < length && !isxdigit(ptr[i])) i++;
    
    // Проверяем, что есть два HEX символа
    if (i + 1 >= length) {
      if (bufferIndex % 2 != 0) {
        UART0_DEBUG_PORT.println("Ошибка: нечётная длина HEX-строки");
        bufferIndex = 0;
        return;
      }
      break;
    }
    
    // Быстрое преобразование HEX в байт
    char high = ptr[i];
    char low = ptr[i + 1];
    
    receiveBuffer[bufferIndex++] = 
      (hex_char_to_byte(high) << 4) | hex_char_to_byte(low);
    
    i += 2;
  }
  
  // Проверяем минимальную длину сообщения
  if (bufferIndex < 5) {
    UART0_DEBUG_PORT.println("Ошибка: слишком короткое сообщение");
    bufferIndex = 0;
    return;
  }
  
  // Вызываем обработку
  //processReceivedData();
}






