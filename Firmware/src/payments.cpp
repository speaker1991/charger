#include "main.h"
#include "charging_port.h"

const unsigned long REFUNDPERIOD = 15000;      // Период ожидания перед возвратом средств (мс)
const unsigned long PAYMENT_TIMEOUT = 800000;  // Таймаут в миллисекундах

extern bool terminalDebugMode;

extern volatile int operationNumber;
extern uint8_t receiveBuffer[BUFFER_SIZE];
extern int bufferIndex;

// Текущий терминал, от которого получены данные (устанавливается в uart.cpp)
Terminal currentReceivingTerminal = TERMINAL_A;

//для charging_management
//long refundAmount;
//long amountFIN;
//int opNumberPrev;


const byte START_BYTE = 0x1F;                       // Стартовый байт
const byte PROTOCOL_DISCRIMINATOR_HIGH = 0x96;      // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;  // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;       // Дискриминатор протокола (младшие биты)
const byte MESSAGE_ID_IDL = 0x01;                   // ID сообщения IDL

bool FundingFLAG = false;

// ========== TLV ДЛЯ ТЕРМИНАЛА A ==========
tlv receivedTLV_A { receivedTLV_A.mesName = "", 
                    receivedTLV_A.opNumber = 0, 
                    receivedTLV_A.amount = 0, 
                    receivedTLV_A.lastTime = millis(), 
                    receivedTLV_A.isMesProcessed = true};

tlv sentTLV_A{"", 0, 0, millis(), true};

// ========== TLV ДЛЯ ТЕРМИНАЛА B ==========
tlv receivedTLV_B { receivedTLV_B.mesName = "", 
                    receivedTLV_B.opNumber = 0, 
                    receivedTLV_B.amount = 0, 
                    receivedTLV_B.lastTime = millis(), 
                    receivedTLV_B.isMesProcessed = true};

tlv sentTLV_B{"", 0, 0, millis(), true};

// ========== СТАРЫЕ ССЫЛКИ ДЛЯ СОВМЕСТИМОСТИ ==========
// Указывают на структуры терминала A по умолчанию
tlv& receivedTLV = receivedTLV_A;
tlv& sentTLV = sentTLV_A;

transactions portA {      portA.kWattPerHourAvailable   = 0, 
                          portA.paidMinor               = 0, 
                          portA.paymentStatus           = WAITING_PAYMENT, 
                          portA.paymentStatusPrev       = WAITING_PAYMENT, 
                          portA.lastTime                = millis(), 
                          portA.chargingStatus          = WAITING_TO_CHARGE, 
                          portA.chargingStatusPrev      = WAITING_TO_CHARGE,
                          portA.operationNumber         = 0,
                          portA.refundAmount            = 0,
                          portA.amountFIN               = 0,
                          portA.opNumberPrev            = 0,
                          portA.paidTimeUTC             = "",
                          portA.meterError              = false,

                          portA.transactionActive       = false
                        };
                          
transactions portB {      portB.kWattPerHourAvailable   = 0, 
                          portB.paidMinor               = 0, 
                          portB.paymentStatus           = WAITING_PAYMENT, 
                          portB.paymentStatusPrev       = WAITING_PAYMENT, 
                          portB.lastTime                = millis(), 
                          portB.chargingStatus          = WAITING_TO_CHARGE, 
                          portB.chargingStatusPrev      = WAITING_TO_CHARGE,
                          portB.operationNumber         = 0,
                          portB.refundAmount            = 0,
                          portB.amountFIN               = 0,
                          portB.opNumberPrev            = 0,
                          portB.paidTimeUTC             = "",
                          portB.meterError              = false,

                          portB.transactionActive       = false
                        };                          


extern ModbusEnergyMeter energyMeterPortA;
extern ModbusEnergyMeter energyMeterPortB;

extern bool stayIDLE_A;
extern bool stayIDLE_B;

// ========== ЭКЗЕМПЛЯРЫ КЛАССОВ ДЛЯ УПРАВЛЕНИЯ ПОРТАМИ ==========
// Заменяют дублирующиеся функции charging_managment_portA/B на RAII-подход
ChargingPort chargingPortA(
    portA,
    energyMeterPortA,
    TERMINAL_A,
    "A",
    stayIDLE_A,
    ralay_portA_on,
    ralay_portA_off
);

ChargingPort chargingPortB(
    portB,
    energyMeterPortB,
    TERMINAL_B,
    "B",
    stayIDLE_B,
    ralay_portB_on,
    ralay_portB_off
);

void start_payment(long amount, Terminal terminal) {
    const char* terminalName = (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B";
    
    // Отключаем периодический IDL для этого терминала
    if (terminal == TERMINAL_A) {
        stayIDLE_A = false;
    } else {
        stayIDLE_B = false;
    }
    
    UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
    UART0_DEBUG_PORT.printf("  │ [%s] ▶ Начата оплата на сумму: %.2f руб.\n", terminalName, (float)amount / 100.0);
    UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
    
    // Обновляем соответствующий sentTLV
    if (terminal == TERMINAL_A) {
        sentTLV_A.amount = amount;
    } else {
        sentTLV_B.amount = amount;
    }
    
    // Отправляем VRP на соответствующий терминал
    send_VRP(amount, terminal);
}

// Старая функция для совместимости
void start_payment(long amount) {
    start_payment(amount, TERMINAL_A);
}

/*В этой функции мы обрабатываем различные сообщения от терминала по сценариям
Когда пользователь выбирает на экране сумму к оплате и хочет зарядить электромобиль, то терминал посылает сообщение STA в ESP32 о том что есть желание у пользователя оплатить.
Чтобы терминал на своем экране предложил именно оплатить картой, т.е начать операцию перевода денег на банковский счет, то нужно отправить терминалу сообщение VRP с суммой.
Если пользователь оплатил, то терминал отправляет сообщение VRP об успешной оплате с той же суммой и тогда можно уже начать зарядку и установить переменную portTemp.paymentStatus в статус PAID
и затем производить обработку различных сценариев при зарядке в функции charging_managment()
*/
void processing_received_POS_message() {
  int lastOperationNumber = get_current_operation_number();
  
  // Выбираем правильный TLV в зависимости от того, какой терминал прислал данные
  tlv* currentTLV = (currentReceivingTerminal == TERMINAL_A) ? &receivedTLV_A : &receivedTLV_B;
  tlv* currentSentTLV = (currentReceivingTerminal == TERMINAL_A) ? &sentTLV_A : &sentTLV_B;
  const char* terminalName = (currentReceivingTerminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B";
  
  if (currentTLV->isMesProcessed) return;      // одно сообщение — одно действие
  
  // Для IDL выводим только при idleLogEnabled, остальные — всегда
  if (currentTLV->mesName != "IDL" || idleLogEnabled) {
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.printf("║ 📥 [%s] ОБРАБОТКА СООБЩЕНИЯ: %s\n", terminalName, currentTLV->mesName.c_str());
    UART0_DEBUG_PORT.printf("║ Операция: #%d\n", currentTLV->opNumber);
    if (currentTLV->amount > 0) {
      UART0_DEBUG_PORT.printf("║ Сумма: %.2f руб.\n", (float)currentTLV->amount / 100.0);
    }
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
  }
  const unsigned long now = millis();

/*
Получено сообщение о том что пользователь желает внести оплату, но это только начало
В теле условия мы должны отправить сообщение VRP с суммой 
*/
  if (currentTLV->mesName == "STA") {
    currentTLV->isMesProcessed = true;         // пометить до сайд-эффектов
    
    // Проверяем подключение к серверу (MQTT)
    // Без интернета нельзя принимать оплату — транзакция не будет сохранена
    if (!is_mqtt_connected()) {
      UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
      UART0_DEBUG_PORT.printf("  │ [%s] ⚠️ НЕТ СВЯЗИ С СЕРВЕРОМ!\n", terminalName);
      UART0_DEBUG_PORT.println("  │ Оплата недоступна. VRP не отправлен.");
      UART0_DEBUG_PORT.println("  │ Ожидаем восстановления подключения...");
      UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
      
      // Возвращаем терминал в режим ожидания
      if (currentReceivingTerminal == TERMINAL_A) {
          stayIDLE_A = true;
      } else {
          stayIDLE_B = true;
      }
      return;
    }
    
    // Останавливаем периодический IDL для этого терминала
    if (currentReceivingTerminal == TERMINAL_A) {
        stayIDLE_A = false;
    } else {
        stayIDLE_B = false;
    }
    if (currentTLV->amount > 0) {
      // Сырая сумма из терминала (обычно в копейках)
      long rawAmount = currentTLV->amount;
      // Отладочная "урезанная" сумма для реального списания
      long debugAmount = (long)(rawAmount / DEBUG_PRICE_DIVIDER);

      // Внутри логики оплаты и при сравнении с ответом терминала
      // работаем именно с уменьшенной суммой
      currentTLV->amount = debugAmount;
      currentSentTLV->amount     = debugAmount;
      currentSentTLV->mesName    = "VRP";
      currentSentTLV->lastTime   = now;

      UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
      UART0_DEBUG_PORT.printf("  │ [%s] Сумма на экране: %.2f руб.\n", terminalName, (float)rawAmount / 100.0);
      UART0_DEBUG_PORT.print("  │ К списанию (debug÷");
      UART0_DEBUG_PORT.print((int)DEBUG_PRICE_DIVIDER);
      UART0_DEBUG_PORT.print("): ");
      UART0_DEBUG_PORT.print((float)debugAmount / 100.0, 2);
      UART0_DEBUG_PORT.println(" руб.");
      UART0_DEBUG_PORT.println("  └───────────────────────────────────────");

      // Отправляем VRP на соответствующий терминал
      send_VRP(debugAmount, currentReceivingTerminal);
      
    } else {
      UART0_DEBUG_PORT.printf("  [%s] STA без суммы — VRP не отправляем\n", terminalName);
    }
    return;
  }

  // 2) Ответ по оплате: имя может быть VRP/RES/VRA (в зависимости от прошивки)
  if (currentTLV->mesName == "VRP") {
    
    // Проверка на слишком быстрый ответ (эхо от глючащего терминала)
    // Пользователь не может оплатить быстрее чем за 3 секунды
    const unsigned long MIN_PAYMENT_TIME_MS = 3000;
    unsigned long responseTime = millis() - currentSentTLV->lastTime;
    
    if (responseTime < MIN_PAYMENT_TIME_MS) {
      UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
      UART0_DEBUG_PORT.printf("  │ [%s] ⚠️ ПОДОЗРИТЕЛЬНО БЫСТРЫЙ ОТВЕТ!\n", terminalName);
      UART0_DEBUG_PORT.printf("  │ Ответ за %lu мс (мин. %lu мс)\n", responseTime, MIN_PAYMENT_TIME_MS);
      UART0_DEBUG_PORT.println("  │ Терминал вернул эхо без реальной оплаты");
      UART0_DEBUG_PORT.println("  │ ⛔ Отменяю операцию и отключаю терминал");
      UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
      
      // Отменяем операцию и отключаем терминал
      send_ABR(currentReceivingTerminal);
      delay(100);  // Даём время на обработку ABR
      send_DIS(currentReceivingTerminal);
      handle_failed_payment(currentReceivingTerminal);
      
      // Отключаем периодический IDL - терминал отключен
      if (currentReceivingTerminal == TERMINAL_A) {
          stayIDLE_A = false;
      } else {
          stayIDLE_B = false;
      }
      
      currentTLV->isMesProcessed = true;
      return;
    }
    
    if (currentTLV->amount == currentSentTLV->amount){
      UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
      UART0_DEBUG_PORT.printf("  │ [%s] ✓ Оплата подтверждена: %.2f руб.\n", terminalName, (float)currentTLV->amount / 100.0);
      UART0_DEBUG_PORT.printf("  │ Время ответа: %lu мс\n", responseTime);
      UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
      if (lastOperationNumber == get_current_operation_number()) {
        UART0_DEBUG_PORT.printf("  ✓ Номер операции совпадает: #%d\n", get_current_operation_number()); 
        handle_successful_payment();                    // пополняем счет и вычисляем оплаченный объем энергии
      }
      else {
        UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
        UART0_DEBUG_PORT.printf("  │ [%s] ✗ ОШИБКА: несовпадение номера операции\n", terminalName);
        UART0_DEBUG_PORT.printf("  │ Ожидали: #%d\n", lastOperationNumber);
        UART0_DEBUG_PORT.printf("  │ Получили: #%d\n", get_current_operation_number());
        UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
        send_ABR(currentReceivingTerminal);  // Отмена на соответствующем терминале
        handle_failed_payment(currentReceivingTerminal);
        
        // Возвращаем терминал в режим IDL — не критическая ошибка
        send_IDL(currentReceivingTerminal);
        if (currentReceivingTerminal == TERMINAL_A) {
            stayIDLE_A = true;
        } else {
            stayIDLE_B = true;
        }
      }
    } else {
      // Проверяем: если терминал вернул сумму 0 — пользователь просто не приложил карту
      // Это НЕ критическая ошибка, а штатная отмена операции
      bool userCancelled = (currentTLV->amount == 0 || currentTLV->amount == -1);
      
      UART0_DEBUG_PORT.println("  ┌───────────────────────────────────────");
      if (userCancelled) {
        UART0_DEBUG_PORT.printf("  │ [%s] ⚠️ Пользователь не приложил карту\n", terminalName);
        UART0_DEBUG_PORT.printf("  │ Ожидали: %.2f руб.\n", (float)currentSentTLV->amount / 100.0);
        UART0_DEBUG_PORT.println("  │ Получили: 0.00 руб. (отмена)");
        UART0_DEBUG_PORT.println("  │ 🔄 Возвращаю терминал в режим ожидания");
        UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
        
        // Отправляем DIS для завершения текущей сессии
        send_DIS(currentReceivingTerminal);
        delay(500);  // Даём время терминалу обработать DIS
        
        // Возвращаем терминал в режим IDL — готов к новой оплате
        send_IDL(currentReceivingTerminal);
        
        // Включаем периодическую отправку IDL
        if (currentReceivingTerminal == TERMINAL_A) {
            stayIDLE_A = true;
        } else {
            stayIDLE_B = true;
        }
      } else {
        // Терминал вернул другую ненулевую сумму — это действительно странно
        UART0_DEBUG_PORT.printf("  │ [%s] ✗ ОШИБКА: несовпадение суммы\n", terminalName);
        UART0_DEBUG_PORT.printf("  │ Ожидали: %.2f руб.\n", (float)currentSentTLV->amount / 100.0);
        UART0_DEBUG_PORT.printf("  │ Получили: %.2f руб.\n", (float)currentTLV->amount / 100.0);
        UART0_DEBUG_PORT.println("  │ ⛔ Критическая ошибка — отключаю терминал");
        UART0_DEBUG_PORT.println("  └───────────────────────────────────────");
        
        // Критическая ошибка - отключаем терминал
        send_DIS(currentReceivingTerminal);
        
        // НЕ возвращаем терминал в IDL - требуется разбор ситуации
        if (currentReceivingTerminal == TERMINAL_A) {
            stayIDLE_A = false;
        } else {
            stayIDLE_B = false;
        }
      }
      
      // Уведомляем об ошибке (для обоих случаев)
      handle_failed_payment(currentReceivingTerminal);
    }
    currentTLV->isMesProcessed = true;                 // пометить до сайд-эффектов
    // очистка контекста отправленного VRP
    currentSentTLV->amount   = 0;
    currentSentTLV->mesName  = "";
    currentSentTLV->lastTime = 0;
    currentSentTLV->opNumber = 0;
    return;
  }

  // 3) Прочие сообщения — пометить обработанными
  currentTLV->isMesProcessed = true;

  // 4) Таймаут ожидания ответа на наш VRP
  if (currentSentTLV->amount > 0 && (now - currentSentTLV->lastTime) > PAYMENT_TIMEOUT) {
    UART0_DEBUG_PORT.printf("  [%s] Payment timeout\n", terminalName);
    handle_payment_timeout();
    currentSentTLV->amount   = 0;
    currentSentTLV->mesName  = "";
    currentSentTLV->lastTime = 0;
    currentSentTLV->opNumber = 0;
  }
}

// ========== РАЗДЕЛЬНАЯ ОБРАБОТКА ДЛЯ ДВУХ ТЕРМИНАЛОВ ==========

// Обработка сообщений от терминала A (порт A)
void processing_received_POS_message_terminalA() {
  currentReceivingTerminal = TERMINAL_A;
  processing_received_POS_message();
}

// Обработка сообщений от терминала B (порт B)
void processing_received_POS_message_terminalB() {
  currentReceivingTerminal = TERMINAL_B;
  processing_received_POS_message();
}




void process_POS_received_data() {
  int pos = 0;  // текущая позиция в receiveBuffer

  while (pos < bufferIndex) {
    // ---- РЕСИНХ ПО START_BYTE ----
    if (receiveBuffer[pos] != START_BYTE) {
      pos++;     // ищем следующий возможный старт кадра
      continue;
    }

    // Минимальный размер кадра: 1 (start) + 2 (len) + 2 (proto) + 2 (CRC) = 7
    if (bufferIndex - pos < 7) {
      UART0_DEBUG_PORT.println("Неполный заголовок кадра, ждём байты");
      break;  // выходим, считаем, что остаток докинут следующим приёмом
    }

    uint16_t msgLen = (uint16_t(receiveBuffer[pos+1]) << 8) | receiveBuffer[pos+2];
    uint16_t proto  = (uint16_t(receiveBuffer[pos+3]) << 8) | receiveBuffer[pos+4];

    uint16_t totalLen = 1 + 2 + msgLen + 2; // start + len + payload + CRC
    int frameEnd = pos + totalLen;          // позиция ПЕРВЫЙ байт после кадра

    // Проверка, что кадр целиком в буфере
    if (frameEnd > bufferIndex) {
      UART0_DEBUG_PORT.println("Неполный кадр в конце буфера, ждём продолжение");
      break;  // остаток кадра придёт в следующий раз
    }

    // ---- CRC ----
    uint16_t rxCrc =
      (uint16_t(receiveBuffer[frameEnd - 2]) << 8) |
       uint16_t(receiveBuffer[frameEnd - 1]);

    // CRC считаем по кадру от START_BYTE до последнего байта перед CRC
    uint16_t calcCrc = calculate_CRC16_ccitt(&receiveBuffer[pos], totalLen - 2);

    if (rxCrc != calcCrc) {
      UART0_DEBUG_PORT.println("CRC error, пропускаем кадр. Кадр целиком:");
      // Выводим проблемный кадр для анализа "грязных" сообщений
      for (int i = pos; i < frameEnd; i++) {
        if (receiveBuffer[i] < 0x10) UART0_DEBUG_PORT.print('0');
        UART0_DEBUG_PORT.print(receiveBuffer[i], HEX);
        UART0_DEBUG_PORT.print(' ');
      }
      UART0_DEBUG_PORT.println();
      pos++;     // сдвигаемся на 1 байт, пытаемся ресинхронизироваться
      continue;
    }

    // ---- ПРОТОКОЛ ----
    if (proto != 0x97FB && proto != 0x96FB) {
      UART0_DEBUG_PORT.print("Ошибка: неверный протокол 0x");
      UART0_DEBUG_PORT.print(proto, HEX);
      UART0_DEBUG_PORT.println(", пропускаем кадр. Кадр целиком:");
      for (int i = pos; i < frameEnd; i++) {
        if (receiveBuffer[i] < 0x10) UART0_DEBUG_PORT.print('0');
        UART0_DEBUG_PORT.print(receiveBuffer[i], HEX);
        UART0_DEBUG_PORT.print(' ');
      }
      UART0_DEBUG_PORT.println();
      pos = frameEnd;   // этот кадр валидный по длине и CRC, но протокол не наш
      continue;
    }

    // ---- TLV-парсинг для ЭТОГО кадра ----
    const int appStart = pos + 5;     // после start + len(2) + proto(2)
    const int appEnd   = frameEnd - 2;  // до CRC

    String msgName = "";
    long operation_Number = -1;
    long amount = -1;
    int productNumber = -1;
    
    // Структура для хранения неизвестных тегов (для отложенного вывода)
    struct UnknownTag {
      uint8_t tag;
      uint8_t len;
      int dataPos;
    };
    UnknownTag unknownTags[16];
    int unknownTagCount = 0;

    // Первый проход: парсим все теги
    for (int p = appStart; p < appEnd; ) {
      uint8_t tag = receiveBuffer[p++];

      if (p >= appEnd) break;
      uint8_t len = receiveBuffer[p++];

      if (p + len > appEnd) {
        UART0_DEBUG_PORT.println("TLV выходит за границы");
        break;
      }

      switch (tag) {
        case 0x01: {  // Имя сообщения (IDL, STA, ...)
          msgName = "";
          for (int i = 0; i < len; i++) {
            msgName += char(receiveBuffer[p + i]);
          }
        } break;

        case 0x02: {  // Product Number (номер продукта/кнопки)
          String s = "";
          for (int i = 0; i < len; i++) s += char(receiveBuffer[p + i]);
          productNumber = s.toInt();
        } break;

        case 0x03: {  // Номер операции
          String s = "";
          for (int i = 0; i < len; i++) s += char(receiveBuffer[p + i]);
          operation_Number = s.toInt();
          operationNumber  = operation_Number;  // твой глобальный, если надо
        } break;

        case 0x04: {  // Сумма
          String s = "";
          for (int i = 0; i < len; i++) s += char(receiveBuffer[p + i]);
          amount = s.toInt();
        } break;

        default: {  // Неизвестный тег — сохраняем для отложенного вывода
          if (unknownTagCount < 16) {
            unknownTags[unknownTagCount].tag = tag;
            unknownTags[unknownTagCount].len = len;
            unknownTags[unknownTagCount].dataPos = p;
            unknownTagCount++;
          }
        } break;
      }

      p += len;
    }
    
    // Определяем, нужно ли выводить отладку для этого сообщения
    bool showDebug = (msgName != "IDL") || idleLogEnabled;
    
    // ---- ВЫВОД HEX ВАЛИДНОГО КАДРА ----
    // Выводим hex-дамп для всех кадров, кроме IDL (если idleLogEnabled = false)
    if (showDebug) {
      UART0_DEBUG_PORT.println("┌─────────────────────────────────────────────────────");
      UART0_DEBUG_PORT.print("│ ◄ ОТ ТЕРМИНАЛА │ HEX: ");
      for (int i = pos; i < frameEnd; i++) {
        if (receiveBuffer[i] < 0x10) UART0_DEBUG_PORT.print('0');
        UART0_DEBUG_PORT.print(receiveBuffer[i], HEX);
        UART0_DEBUG_PORT.print(' ');
      }
      UART0_DEBUG_PORT.println();
    }
    
    // Выводим номер продукта (если был)
    if (showDebug && productNumber >= 0) {
      UART0_DEBUG_PORT.print("  -> Номер продукта/кнопки: ");
      UART0_DEBUG_PORT.println(productNumber);
    }
    
    // Выводим неизвестные теги (только для не-IDL или при idleLogEnabled)
    if (showDebug) {
      for (int i = 0; i < unknownTagCount; i++) {
        UART0_DEBUG_PORT.print("  -> Неизвестный тег 0x");
        UART0_DEBUG_PORT.print(unknownTags[i].tag, HEX);
        UART0_DEBUG_PORT.print(" len=");
        UART0_DEBUG_PORT.print(unknownTags[i].len);
        UART0_DEBUG_PORT.print(" data: ");
        for (int j = 0; j < unknownTags[i].len; j++) {
          UART0_DEBUG_PORT.print(char(receiveBuffer[unknownTags[i].dataPos + j]));
        }
        UART0_DEBUG_PORT.println();
      }
    }

    // ---- ОТЛАДОЧНЫЙ ВЫВОД ПО КАЖДОМУ КАДРУ ----
    // Для IDL выводим только при idleLogEnabled, остальные — всегда
    if (msgName != "IDL" || idleLogEnabled) {
      UART0_DEBUG_PORT.print("│ Сообщение: ");
      UART0_DEBUG_PORT.print(msgName);
      UART0_DEBUG_PORT.print(" │ Операция: ");
      UART0_DEBUG_PORT.print(operation_Number);
      UART0_DEBUG_PORT.print(" │ Сумма: ");
      if (amount > 0) {
        UART0_DEBUG_PORT.print((float)amount / 100.0, 2);
        UART0_DEBUG_PORT.print(" руб.");
      } else {
        UART0_DEBUG_PORT.print("—");
      }
      UART0_DEBUG_PORT.println();
      UART0_DEBUG_PORT.println("└─────────────────────────────────────────────────────");
    }

    // Обновляем время последнего ответа терминала (для мониторинга доступности)
    if (currentReceivingTerminal == TERMINAL_A) {
        update_terminal_A_last_response();
    } else {
        update_terminal_B_last_response();
    }

    // Сохраняем данные в соответствующую структуру TLV
    // ВАЖНО: всегда перезаписываем, чтобы не потерять важные сообщения (STA, VRP)
    // если в буфере несколько пакетов (например IDL + STA)
    if (currentReceivingTerminal == TERMINAL_A) {
        receivedTLV_A.amount = amount;
        receivedTLV_A.mesName = msgName;
        receivedTLV_A.opNumber = operationNumber;
        receivedTLV_A.isMesProcessed = false;
        receivedTLV_A.lastTime = millis();
    } else {
        receivedTLV_B.amount = amount;
        receivedTLV_B.mesName = msgName;
        receivedTLV_B.opNumber = operationNumber;
        receivedTLV_B.isMesProcessed = false;
        receivedTLV_B.lastTime = millis();
    }

    // Переходим к следующему кадру в буфере
    pos = frameEnd;
  }

  // После обработки всех кадров в буфере — очищаем общий буфер
  clear_buffer();
}




// Обработка успешного платежа
void handle_successful_payment() {
    //send_IDL();
    UART0_DEBUG_PORT.println();
    UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
    UART0_DEBUG_PORT.println("║ ✓ ОПЛАТА УСПЕШНО ПРОВЕДЕНА");
    UART0_DEBUG_PORT.print("║ Режим отладки терминала: ");
    UART0_DEBUG_PORT.println(terminalDebugMode ? "ВКЛ" : "ВЫКЛ");
    UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

    // В режиме отладки терминала сразу делаем возврат, минуя логику портов
    if (terminalDebugMode) {
      UART0_DEBUG_PORT.println("  [DEBUG] Полный возврат средств через FIN через 4 сек...");
      delay(4000);  // даём терминалу "переварить" успешный VRP перед FIN

      float finAmount = 0.0f;                   // ничего не потрачено
      
      // Определяем с какого терминала пришла оплата
      if (currentReceivingTerminal == TERMINAL_A) {
        int finOp = receivedTLV_A.opNumber;
        send_FIN(finAmount, finOp, TERMINAL_A);     // FIN(0) => полный возврат на терминал A
      } else {
        int finOp = receivedTLV_B.opNumber;
        send_FIN(finAmount, finOp, TERMINAL_B);     // FIN(0) => полный возврат на терминал B
      }
      
      // Возвращаем терминалы в режим IDL (в режиме отладки)
      stayIDLE_A = true;
      stayIDLE_B = true;
      return;
    }

    // ========== ПРИВЯЗКА: ТЕРМИНАЛ A → ПОРТ A, ТЕРМИНАЛ B → ПОРТ B ==========
    // Определяем порт по терминалу, с которого пришла оплата
    if (currentReceivingTerminal == TERMINAL_A) {
      // ========== ТЕРМИНАЛ A → ПОРТ A ==========
      portA.paidTimeUTC = getISO8601Time();
      portA.paymentStatus = PAID;
      portA.paidMinor = receivedTLV_A.amount;
      portA.kWattPerHourAvailable = (((float)portA.paidMinor) / PRICE_FOR_ONE_KWHOUR) * TEST_ENERGY_MULTIPLIER;
      portA.operationNumber = receivedTLV_A.opNumber;
      
      UART0_DEBUG_PORT.println();
      UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
      UART0_DEBUG_PORT.println("║ 🔌 ТЕРМИНАЛ A → ПОРТ A");
      UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
      UART0_DEBUG_PORT.printf("║ Оплачено: %.2f руб.\n", (float)portA.paidMinor / 100.0);
      UART0_DEBUG_PORT.print("║ Доступно энергии (x");
      UART0_DEBUG_PORT.print((int)TEST_ENERGY_MULTIPLIER);
      UART0_DEBUG_PORT.print("): ");
      UART0_DEBUG_PORT.print(portA.kWattPerHourAvailable, 2);
      UART0_DEBUG_PORT.println(" кВт·ч");
      UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
      
    } else {
      // ========== ТЕРМИНАЛ B → ПОРТ B ==========
      portB.paidTimeUTC = getISO8601Time();
      portB.paymentStatus = PAID;
      portB.paidMinor = receivedTLV_B.amount;
      portB.kWattPerHourAvailable = (((float)portB.paidMinor) / PRICE_FOR_ONE_KWHOUR) * TEST_ENERGY_MULTIPLIER;
      portB.operationNumber = receivedTLV_B.opNumber;
      
      UART0_DEBUG_PORT.println();
      UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
      UART0_DEBUG_PORT.println("║ 🔌 ТЕРМИНАЛ B → ПОРТ B");
      UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
      UART0_DEBUG_PORT.printf("║ Оплачено: %.2f руб.\n", (float)portB.paidMinor / 100.0);
      UART0_DEBUG_PORT.print("║ Доступно энергии (x");
      UART0_DEBUG_PORT.print((int)TEST_ENERGY_MULTIPLIER);
      UART0_DEBUG_PORT.print("): ");
      UART0_DEBUG_PORT.print(portB.kWattPerHourAvailable, 2);
      UART0_DEBUG_PORT.println(" кВт·ч");
      UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
    }

    // В обычном режиме: дальнейшее завершение (FIN/REFUND) делает charging_managment_portA/B
}

// Обработка ошибки платежа (с указанием терминала)
void handle_failed_payment(Terminal terminal) {
    const char* terminalName = (terminal == TERMINAL_A) ? "ТЕРМИНАЛ A" : "ТЕРМИНАЛ B";
    UART0_DEBUG_PORT.printf("[%s] Ошибка при проведении оплаты\n", terminalName);
    // НЕ отправляем IDL здесь — это делает вызывающий код при необходимости
}

// Старая функция для совместимости (отправляет IDL на терминал A)
void handle_failed_payment() {
    UART0_DEBUG_PORT.println("Ошибка при проведении оплаты");
    send_IDL(TERMINAL_A);
}

// Обработка таймаута
void handle_payment_timeout() {
    UART0_DEBUG_PORT.println("Превышено время ожидания оплаты");
    send_IDL();
}

// Проверка, идет ли активная зарядка на любом из портов
// Возвращает true, если на терминале висит открытая транзакция
bool is_charging_active() {
    // Порт A: зарядка активна, если:
    // 1) статус не WAITING_TO_CHARGE или оплата не WAITING_PAYMENT
    // 2) ИЛИ есть ожидающий возврат средств (refundAmount > 0) - терминал занят транзакцией
    bool portA_active = (portA.chargingStatus != WAITING_TO_CHARGE) || 
                        (portA.paymentStatus != WAITING_PAYMENT) ||
                        (portA.refundAmount > 0);
    
    // Порт B: зарядка активна, если:
    // 1) статус не WAITING_TO_CHARGE или оплата не WAITING_PAYMENT
    // 2) ИЛИ есть ожидающий возврат средств (refundAmount > 0) - терминал занят транзакцией
    bool portB_active = (portB.chargingStatus != WAITING_TO_CHARGE) || 
                        (portB.paymentStatus != WAITING_PAYMENT) ||
                        (portB.refundAmount > 0);
    
    return portA_active || portB_active;
}

// Проверка активности транзакции для конкретного терминала
// Возвращает true если терминал занят транзакцией (оплата, зарядка, возврат)
bool is_terminal_transaction_active(Terminal terminal) {
    transactions* port = (terminal == TERMINAL_A) ? &portA : &portB;
    
    // Терминал занят транзакцией, если:
    // 1) Идет зарядка (chargingStatus != WAITING_TO_CHARGE)
    // 2) Идет обработка оплаты (paymentStatus != WAITING_PAYMENT)
    // 3) Есть ожидающий возврат средств (refundAmount > 0)
    return (port->chargingStatus != WAITING_TO_CHARGE) || 
           (port->paymentStatus != WAITING_PAYMENT) ||
           (port->refundAmount > 0);
}

/*
В этой функции происходит обработка различных сценариев после прохождения платежа:
1) Оплата произведена → включаем реле, подключая линию данных пистолета к контроллеру станции для начала зарядки
2) Зарядка идет успешно, но оплаченных денег не хватает → отключаем реле, разрывая линию данных и останавливая зарядку
3) Зарядку прервали (клиент достал пистолет), но деньги еще есть → нужно сделать возврат средств

РЕФАКТОРИНГ: Логика вынесена в класс ChargingPort для устранения дублирования кода
*/
void charging_managment_portA(){
    chargingPortA.manage();
} 
////////////////////////////////////////////////////////////////////////////////////////////////////////
void charging_managment_portB(){
    chargingPortB.manage();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
// АВАРИЙНАЯ ОСТАНОВКА ВСЕХ ЗАРЯДОК (при потере WiFi/интернета)
////////////////////////////////////////////////////////////////////////////////////////////////////////
void emergency_stop_all_charging() {
    bool stoppedAny = false;
    
    // Проверяем порт A
    if (portA.chargingStatus == RUNNING || portA.chargingStatus == START_TO_CHARGE) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ⛔ ПОРТ A: АВАРИЙНАЯ ОСТАНОВКА (потеря связи)");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        ralay_portA_off();  // Выключаем реле
        portA.chargingStatus = STOPPING;
        portA.paymentStatus = REFUND;
        stoppedAny = true;
    }
    
    // Проверяем порт B
    if (portB.chargingStatus == RUNNING || portB.chargingStatus == START_TO_CHARGE) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.println("║ ⛔ ПОРТ B: АВАРИЙНАЯ ОСТАНОВКА (потеря связи)");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        ralay_portB_off();  // Выключаем реле
        portB.chargingStatus = STOPPING;
        portB.paymentStatus = REFUND;
        stoppedAny = true;
    }
    
    if (stoppedAny) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("║ ⚠️ Зарядки остановлены, средства будут возвращены");
        UART0_DEBUG_PORT.println("║ после восстановления связи с сервером.");
    }
}