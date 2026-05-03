#pragma once

#include <Arduino.h>
#include <ModbusMaster.h>

/**
 * Класс для чтения энергосчётчика по Modbus RTU (32-bit big-endian регистры)
 * через RS-485 трансивер BL3085 (или аналогичный).
 *
 * Предполагается:
 *  - UART (например, Serial2) уже инициализирован в твоём коде.
 *  - BL3085 подключен к этому UART (DI ← TX, RO → RX).
 *  - DE и /RE могут быть на одном или на разных пинах.
 */

 void init_modbus_energy_meter();

void loop_update_energy();

// Проверка: отключен ли терминал из-за ошибок счетчиков
bool is_terminal_disabled_due_to_meter();

// Проверка статуса конкретного терминала (для раздельного управления)
bool is_terminal_A_disabled_due_to_meter();
bool is_terminal_B_disabled_due_to_meter();


class ModbusEnergyMeter {
public:
    /**
     * Масштабы пересчёта регистров:
     */

    // Энергия — 1 LSB = 1 Wh
    static constexpr float ENERGY_SCALE_WH_PER_LSB = 0.01f;

    // Напряжение: raw=6475000 → 647.5000 В (деление на 10000)
    static constexpr float VOLTAGE_SCALE_V_PER_LSB = 0.0001f;

    // Мощность: raw=233430 → 23343 Вт (деление на 10)
    static constexpr float POWER_SCALE_W_PER_LSB   = 0.1f;

    /**
     * @param port    – уже инициализированный HardwareSerial (например, Serial2)
     * @param slaveId – Modbus-адрес счётчика (по умолчанию 1)
     * @param dePin   – GPIO для DE (HIGH = передача)
     * @param rePin   – GPIO для /RE (LOW = приём). Если DE=RE → указываем один и тот же пин.
     */
    ModbusEnergyMeter(HardwareSerial &port,
                      uint8_t slaveId = 1,
                      int8_t dePin = -1,
                      int8_t rePin = -1);

    void begin();

    bool readUint32(uint16_t startReg, uint32_t &value);

    bool readEnergy(uint32_t &value);
    bool readPower(uint32_t &value);
    bool readVoltage(uint32_t &value);

    bool updateAll();

    uint32_t energyRaw()   const { return _energyRaw; }
    uint32_t powerRaw()    const { return _powerRaw; }
    uint32_t voltageRaw()  const { return _voltageRaw; }

    /// ----- Геттеры с пересчётом -----
    float energyWh() const {
        return static_cast<float>(_energyRaw) * ENERGY_SCALE_WH_PER_LSB;
    }

    float sessionEnergyKWh() const {
        return static_cast<float>(sessionEnergyRaw()) * ENERGY_SCALE_WH_PER_LSB;
    }

    float powerW() const {
        return static_cast<float>(_powerRaw) * POWER_SCALE_W_PER_LSB;
    }

    float voltageV() const {
        return static_cast<float>(_voltageRaw) * VOLTAGE_SCALE_V_PER_LSB;
    }

    // -------- Работа с сессией зарядки --------
    bool resetSessionEnergy();
    uint32_t sessionEnergyRaw() const;

    uint8_t lastError() const { return _lastError; }

private:
    ModbusMaster    _node;
    HardwareSerial &_port;
    uint8_t         _slaveId;
    int8_t          _dePin;
    int8_t          _rePin;
    uint8_t         _lastError{0};

    uint32_t _energyRaw{0};
    uint32_t _powerRaw{0};
    uint32_t _voltageRaw{0};

    uint32_t _energySessionBase{0};

    static ModbusEnergyMeter* _instanceForHooks;

    void preTransmission();
    void postTransmission();

    static void preTransmissionStatic();
    static void postTransmissionStatic();

    static uint32_t regsToUint32(uint16_t hi, uint16_t lo);
};
