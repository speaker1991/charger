#include "main.h"

extern bool stayIDLE_A;
extern bool stayIDLE_B;


// Один GPIO на DE и /RE BL3085
#define RS485_DIR_PIN 48

ModbusEnergyMeter energyMeterPortA(
    UART2_ENERGY_READ_PORT,
    1,              // Modbus slave id
    RS485_DIR_PIN,  // dePin
    RS485_DIR_PIN   // rePin (тот же пин, DE и /RE объединены)
);

ModbusEnergyMeter energyMeterPortB(
    UART2_ENERGY_READ_PORT,
    2,              // Modbus slave id
    RS485_DIR_PIN,  // dePin
    RS485_DIR_PIN   // rePin (тот же пин, DE и /RE объединены)
);

ModbusEnergyMeter* ModbusEnergyMeter::_instanceForHooks = nullptr;

ModbusEnergyMeter::ModbusEnergyMeter(HardwareSerial &port,
                                     uint8_t slaveId,
                                     int8_t dePin,
                                     int8_t rePin)
    : _port(port),
      _slaveId(slaveId),
      _dePin(dePin),
      _rePin(rePin)
{
}


void init_modbus_energy_meter(){
    energyMeterPortA.begin();
    energyMeterPortA.resetSessionEnergy();

    energyMeterPortB.begin();
    energyMeterPortB.resetSessionEnergy();
}

// Статические переменные для защиты от спама уведомлений и отслеживания состояния
static bool errorSentA = false;  // Флаг: уведомление об ошибке порта A уже отправлено
static bool errorSentB = false;  // Флаг: уведомление об ошибке порта B уже отправлено
static unsigned long lastErrorTimeA = 0;  // Время последней ошибки порта A
static unsigned long lastErrorTimeB = 0;  // Время последней ошибки порта B
static uint8_t errorCountA = 0;  // Счётчик последовательных ошибок порта A
static uint8_t errorCountB = 0;  // Счётчик последовательных ошибок порта B
static bool terminalDisabledA = false;  // Терминал отключён из-за счётчика A
static bool terminalDisabledB = false;  // Терминал отключён из-за счётчика B

const unsigned long errorNotifyInterval = 60000;  // Интервал между повторными уведомлениями (60 сек)
const uint8_t ERROR_THRESHOLD = 3;  // Сколько ошибок подряд = отключение терминала (уменьшено с 5 до 3)
const unsigned long METER_RETRY_INTERVAL = 15000; // Интервал повторных попыток для недоступных счётчиков (15 сек)
const unsigned long METER_POLL_INTERVAL = 10000;  // Интервал опроса счётчиков в нормальном режиме (10 сек)
const unsigned long METER_DELAY_BETWEEN_AB = 2000; // Задержка между опросом счётчика A и B (2 сек)
static unsigned long lastMeterRetryA = 0;
static unsigned long lastMeterRetryB = 0;
static unsigned long lastMeterPollA = 0;  // Время последнего успешного опроса счётчика A
static unsigned long lastMeterPollB = 0;  // Время последнего успешного опроса счётчика B

// Проверка: отключен ли терминал из-за ошибок счетчиков
bool is_terminal_disabled_due_to_meter() {
    return terminalDisabledA || terminalDisabledB;
}

// Проверка статуса конкретного терминала (для раздельного управления)
bool is_terminal_A_disabled_due_to_meter() {
    return terminalDisabledA;
}

bool is_terminal_B_disabled_due_to_meter() {
    return terminalDisabledB;
}

void loop_update_energy(){
    unsigned long now = millis();

    // ===== Порт A =====
    // Проверяем интервал опроса в зависимости от состояния счётчика
    bool skipMeterA = false;
    if (terminalDisabledA) {
        // Если счётчик недоступен - опрашиваем редко (раз в 15 сек)
        skipMeterA = (now - lastMeterRetryA < METER_RETRY_INTERVAL);
    } else {
        // Если счётчик работает - опрашиваем с нормальным интервалом (раз в 10 сек)
        skipMeterA = (now - lastMeterPollA < METER_POLL_INTERVAL);
    }
    
    if (!skipMeterA) {
        if (terminalDisabledA) {
            lastMeterRetryA = now;
        } else {
            lastMeterPollA = now;
        }
        
        if (energyMeterPortA.updateAll()){
            // Успешное чтение - сбрасываем счётчик ошибок
            errorCountA = 0;
            
            // Выводим полученные значения в Serial
            UART0_DEBUG_PORT.println("┌─────────────────────────────────────────");
            UART0_DEBUG_PORT.println("│ [СЧЁТЧИК A] Данные получены:");
            UART0_DEBUG_PORT.print("│  Энергия:    ");
            UART0_DEBUG_PORT.print(energyMeterPortA.energyWh(), 3);
            UART0_DEBUG_PORT.println(" Wh");
            UART0_DEBUG_PORT.print("│  Мощность:   ");
            UART0_DEBUG_PORT.print(energyMeterPortA.powerW(), 2);
            UART0_DEBUG_PORT.println(" W");
            UART0_DEBUG_PORT.print("│  Напряжение: ");
            UART0_DEBUG_PORT.print(energyMeterPortA.voltageV(), 1);
            UART0_DEBUG_PORT.println(" V");
            UART0_DEBUG_PORT.println("└─────────────────────────────────────────");
            
            // Сбрасываем флаг ошибки счетчика для порта A
            if (portA.meterError) {
                portA.meterError = false;
                UART0_DEBUG_PORT.println("[METER A] ✓ Связь со счетчиком восстановлена");
            }
            
            if (errorSentA) {
                errorSentA = false;
                mqtt_send_debug(
                    "[A] Счётчик восстановлен",
                    "Связь со счётчиком порта A восстановлена"
                );
            }
            
            // Восстанавливаем терминал A независимо от статуса счётчика B
            if (terminalDisabledA) {
                UART0_DEBUG_PORT.println();
                UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
                UART0_DEBUG_PORT.println("║ ✓ Счётчик A восстановлен!");
                UART0_DEBUG_PORT.println("║ Включаем терминал A (IDL)...");
                UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
                send_IDL(TERMINAL_A);
                stayIDLE_A = true;  // Включаем периодический IDL для терминала A
                terminalDisabledA = false;
            }
        } else {
            // Увеличиваем счетчик только если терминал ещё не отключен
            if (!terminalDisabledA) {
                errorCountA++;
            }
            
            // Выводим информацию об ошибке только если терминал активен
            if (!terminalDisabledA) {
                UART0_DEBUG_PORT.print("[METER A] ⚠️ Ошибка чтения счетчика (код: 0x");
                UART0_DEBUG_PORT.print(energyMeterPortA.lastError(), HEX);
                UART0_DEBUG_PORT.print(", попытка ");
                UART0_DEBUG_PORT.print(errorCountA);
                UART0_DEBUG_PORT.print("/");
                UART0_DEBUG_PORT.print(ERROR_THRESHOLD);
                UART0_DEBUG_PORT.println(")");
            }
            
            // КРИТИЧНО: Устанавливаем флаг ошибки счетчика для порта A
            // Это немедленно остановит зарядку в charging_managment_portA()
            if (!portA.meterError) {
                portA.meterError = true;
                UART0_DEBUG_PORT.println("[METER A] 🔴 КРИТИЧЕСКАЯ ОШИБКА: Останавливаем зарядку!");
            }
            
            // Отключаем терминал A после нескольких ошибок подряд
            if (errorCountA >= ERROR_THRESHOLD && !terminalDisabledA) {
                UART0_DEBUG_PORT.println();
                UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
                UART0_DEBUG_PORT.println("║ ⚠️ Счётчик A недоступен!");
                UART0_DEBUG_PORT.println("║ Отключаем терминал A (DIS)...");
                UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
                send_DIS(TERMINAL_A);
                stayIDLE_A = false;  // Отключаем периодический IDL для терминала A
                terminalDisabledA = true;
            }
            
            // Отправляем уведомление если: ещё не отправляли ИЛИ прошло достаточно времени
            if (!errorSentA || (now - lastErrorTimeA >= errorNotifyInterval)) {
                mqtt_send_debug(
                    "[A] Счётчик недоступен!",
                    "Проверьте подключение счётчика порта A",
                    "Убедитесь что счётчик не повреждён"
                );
                errorSentA = true;
                lastErrorTimeA = now;
            }
        }
    }

    // Вызываем mqtt_loop() между опросами счётчиков чтобы не блокировать приём команд
    mqtt_loop();

    // ===== Порт B =====
    // ВАЖНО: Добавляем задержку между опросом счётчиков A и B,
    // так как они используют одну RS-485 шину
    if (!skipMeterA) {
        // Даём время счётчику A полностью ответить перед опросом счётчика B
        delay(METER_DELAY_BETWEEN_AB);
    }
    
    // Проверяем интервал опроса в зависимости от состояния счётчика
    bool skipMeterB = false;
    if (terminalDisabledB) {
        // Если счётчик недоступен - опрашиваем редко (раз в 15 сек)
        skipMeterB = (now - lastMeterRetryB < METER_RETRY_INTERVAL);
    } else {
        // Если счётчик работает - опрашиваем с нормальным интервалом (раз в 10 сек)
        skipMeterB = (now - lastMeterPollB < METER_POLL_INTERVAL);
    }
    
    if (!skipMeterB) {
        if (terminalDisabledB) {
            lastMeterRetryB = now;
        } else {
            lastMeterPollB = now;
        }
        
        if (energyMeterPortB.updateAll()){
            // Успешное чтение - сбрасываем счётчик ошибок
            errorCountB = 0;
            
            // Выводим полученные значения в Serial
            UART0_DEBUG_PORT.println("┌─────────────────────────────────────────");
            UART0_DEBUG_PORT.println("│ [СЧЁТЧИК B] Данные получены:");
            UART0_DEBUG_PORT.print("│  Энергия:    ");
            UART0_DEBUG_PORT.print(energyMeterPortB.energyWh(), 3);
            UART0_DEBUG_PORT.println(" Wh");
            UART0_DEBUG_PORT.print("│  Мощность:   ");
            UART0_DEBUG_PORT.print(energyMeterPortB.powerW(), 2);
            UART0_DEBUG_PORT.println(" W");
            UART0_DEBUG_PORT.print("│  Напряжение: ");
            UART0_DEBUG_PORT.print(energyMeterPortB.voltageV(), 1);
            UART0_DEBUG_PORT.println(" V");
            UART0_DEBUG_PORT.println("└─────────────────────────────────────────");
            
            // Сбрасываем флаг ошибки счетчика для порта B
            if (portB.meterError) {
                portB.meterError = false;
                UART0_DEBUG_PORT.println("[METER B] ✓ Связь со счетчиком восстановлена");
            }
            
            if (errorSentB) {
                errorSentB = false;
                mqtt_send_debug(
                    "[B] Счётчик восстановлен",
                    "Связь со счётчиком порта B восстановлена"
                );
            }
            
            // Восстанавливаем терминал B независимо от статуса счётчика A
            if (terminalDisabledB) {
                UART0_DEBUG_PORT.println();
                UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
                UART0_DEBUG_PORT.println("║ ✓ Счётчик B восстановлен!");
                UART0_DEBUG_PORT.println("║ Включаем терминал B (IDL)...");
                UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
                send_IDL(TERMINAL_B);
                stayIDLE_B = true;  // Включаем периодический IDL для терминала B
                terminalDisabledB = false;
            }
        } else {
            // Увеличиваем счетчик только если терминал ещё не отключен
            if (!terminalDisabledB) {
                errorCountB++;
            }
            
            // Выводим информацию об ошибке только если терминал активен
            if (!terminalDisabledB) {
                UART0_DEBUG_PORT.print("[METER B] ⚠️ Ошибка чтения счетчика (код: 0x");
                UART0_DEBUG_PORT.print(energyMeterPortB.lastError(), HEX);
                UART0_DEBUG_PORT.print(", попытка ");
                UART0_DEBUG_PORT.print(errorCountB);
                UART0_DEBUG_PORT.print("/");
                UART0_DEBUG_PORT.print(ERROR_THRESHOLD);
                UART0_DEBUG_PORT.println(")");
            }
            
            // КРИТИЧНО: Устанавливаем флаг ошибки счетчика для порта B
            // Это немедленно остановит зарядку в charging_managment_portB()
            if (!portB.meterError) {
                portB.meterError = true;
                UART0_DEBUG_PORT.println("[METER B] 🔴 КРИТИЧЕСКАЯ ОШИБКА: Останавливаем зарядку!");
            }
            
            // Отключаем терминал B после нескольких ошибок подряд
            if (errorCountB >= ERROR_THRESHOLD && !terminalDisabledB) {
                UART0_DEBUG_PORT.println();
                UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
                UART0_DEBUG_PORT.println("║ ⚠️ Счётчик B недоступен!");
                UART0_DEBUG_PORT.println("║ Отключаем терминал B (DIS)...");
                UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
                send_DIS(TERMINAL_B);
                stayIDLE_B = false;  // Отключаем периодический IDL для терминала B
                terminalDisabledB = true;
            }
            
            // Отправляем уведомление если: ещё не отправляли ИЛИ прошло достаточно времени
            if (!errorSentB || (now - lastErrorTimeB >= errorNotifyInterval)) {
                mqtt_send_debug(
                    "[B] Счётчик недоступен!",
                    "Проверьте подключение счётчика порта B",
                    "Убедитесь что счётчик не повреждён"
                );
                errorSentB = true;
                lastErrorTimeB = now;
            }
        }
    }
}


void ModbusEnergyMeter::begin()
{
    _instanceForHooks = this;

    if (_dePin >= 0) {
        pinMode(_dePin, OUTPUT);
        digitalWrite(_dePin, LOW);
    }
    if (_rePin >= 0) {
        pinMode(_rePin, OUTPUT);
        digitalWrite(_rePin, LOW);
    }

    _node.begin(_slaveId, _port);
    _node.preTransmission(preTransmissionStatic);
    _node.postTransmission(postTransmissionStatic);
}

void ModbusEnergyMeter::preTransmission()
{
    if (_dePin >= 0) digitalWrite(_dePin, HIGH);
    if (_rePin >= 0) digitalWrite(_rePin, HIGH);
}

void ModbusEnergyMeter::postTransmission()
{
    if (_dePin >= 0) digitalWrite(_dePin, LOW);
    if (_rePin >= 0) digitalWrite(_rePin, LOW);
}

void ModbusEnergyMeter::preTransmissionStatic()
{
    if (_instanceForHooks) _instanceForHooks->preTransmission();
}

void ModbusEnergyMeter::postTransmissionStatic()
{
    if (_instanceForHooks) _instanceForHooks->postTransmission();
}

uint32_t ModbusEnergyMeter::regsToUint32(uint16_t hi, uint16_t lo)
{
    return (static_cast<uint32_t>(hi) << 16) | static_cast<uint32_t>(lo);
}

bool ModbusEnergyMeter::readUint32(uint16_t startReg, uint32_t &value)
{
    _lastError = _node.readHoldingRegisters(startReg, 2);

    if (_lastError != _node.ku8MBSuccess)
        return false;

    uint16_t hi = _node.getResponseBuffer(0);
    uint16_t lo = _node.getResponseBuffer(1);

    value = regsToUint32(hi, lo);
    return true;
}

bool ModbusEnergyMeter::readEnergy(uint32_t &value)
{
    return readUint32(0, value);
}

bool ModbusEnergyMeter::readPower(uint32_t &value)
{
    return readUint32(20, value);
}

bool ModbusEnergyMeter::readVoltage(uint32_t &value)
{
    return readUint32(24, value);
}

bool ModbusEnergyMeter::updateAll()
{
    bool ok = true;
    uint32_t tmp;

    // Читаем энергию
    if (readEnergy(tmp)) {
        _energyRaw = tmp;
    } else {
        ok = false;
    }
    delay(100); // Задержка между Modbus-запросами для стабильности RS-485

    // Читаем мощность
    if (readPower(tmp)) {
        _powerRaw = tmp;
    } else {
        ok = false;
    }
    delay(100); // Задержка между Modbus-запросами для стабильности RS-485

    // Читаем напряжение
    if (readVoltage(tmp)) {
        _voltageRaw = tmp;
    } else {
        ok = false;
    }

    return ok;
}

bool ModbusEnergyMeter::resetSessionEnergy()
{
    uint32_t total = 0;
    if (!readEnergy(total)) return false;

    _energyRaw = total;
    _energySessionBase = total;
    return true;
}

uint32_t ModbusEnergyMeter::sessionEnergyRaw() const
{
    const uint32_t current = _energyRaw;
    const uint32_t base    = _energySessionBase;

    if (current >= base) {
        return current - base;
    } else {
        // счётчик “откатился” (эмулятор перезапущен/сброшен) –
        // для сессии зарядки считаем 0, а не rollover
        return 0;
    }
}
