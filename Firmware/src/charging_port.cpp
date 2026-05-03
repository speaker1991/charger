#include "main.h"
#include "charging_port.h"

// ========== ОСНОВНОЙ ЦИКЛ УПРАВЛЕНИЯ ПОРТОМ ==========

void ChargingPort::manage() {
    checkMeterError();
    printDebugInfo();
    handleRefundFinalization();
    checkStartCharging();
    updateChargingState();
    handleChargingStateChange();
    handlePaymentStateChange();
}

// ========== ПРОВЕРКА ОШИБОК СЧЕТЧИКА ==========

void ChargingPort::checkMeterError() {
    // ⚠️ КРИТИЧЕСКАЯ ПРОВЕРКА: Если ошибка счетчика во время зарядки - немедленно останавливаем!
    if (port_.meterError && (port_.chargingStatus == RUNNING || port_.chargingStatus == START_TO_CHARGE)) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.printf("║ ⚠️ ПОРТ %s: АВАРИЙНАЯ ОСТАНОВКА!\n", portName_);
        UART0_DEBUG_PORT.println("║ Причина: Потеря связи со счетчиком энергии");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        // Немедленно отключаем реле
        relayOff_();
        
        // Переводим в состояние остановки
        port_.chargingStatus = STOPPING;
        port_.paymentStatus = REFUND;  // Будет возврат на основе последних данных
        
        debugRefundTime_ = millis();  // Запускаем таймер возврата
    }
}

// ========== ОТЛАДОЧНАЯ ИНФОРМАЦИЯ ==========

void ChargingPort::printDebugInfo() {
    if (millis() - debugTime_ > 3000) {
        // Выводим статусы только когда есть активность (статусы != 0)
        if (port_.chargingStatus != 0 || port_.paymentStatus != 0) {
            // Не опрашиваем счётчик здесь, чтобы не создавать коллизии на RS-485.
            // Используем последние значения, полученные в loop_update_energy().
            UART0_DEBUG_PORT.printf("┌─── ПОРТ %s ────────────────────────────\n", portName_);
            UART0_DEBUG_PORT.print("│ Зарядка: ");
            UART0_DEBUG_PORT.print(port_.chargingStatus);
            UART0_DEBUG_PORT.print(" │ Оплата: ");
            UART0_DEBUG_PORT.print(port_.paymentStatus);
            UART0_DEBUG_PORT.print(" │ ");
            UART0_DEBUG_PORT.print(meter_.sessionEnergyKWh(), 2);
            UART0_DEBUG_PORT.print(" из ");
            UART0_DEBUG_PORT.print(port_.kWattPerHourAvailable, 2);
            UART0_DEBUG_PORT.print(" кВт·ч │ ");
            UART0_DEBUG_PORT.print(meter_.powerW(), 0);
            UART0_DEBUG_PORT.print(" Вт │ ");
            UART0_DEBUG_PORT.print(meter_.voltageV(), 1);
            UART0_DEBUG_PORT.println(" В");
            if (port_.meterError) {
                UART0_DEBUG_PORT.println("│ ⚠️ ОШИБКА СЧЁТЧИКА! Данные устарели!");
            }
            UART0_DEBUG_PORT.println("└───────────────────────────────────────");
        }
        debugTime_ = millis();
    }
}

// ========== ФИНАЛИЗАЦИЯ С ВОЗВРАТОМ ==========

void ChargingPort::handleRefundFinalization() {
    if ((port_.refundAmount) && (millis() - debugRefundTime_ > REFUNDPERIOD)) {
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.printf("║ ✓ ПОРТ %s: ФИНАЛИЗАЦИЯ И СБРОС\n", portName_);
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
        
        send_FIN(port_.amountFIN, port_.opNumberPrev, terminal_);

        port_.kWattPerHourAvailable = 0.0;
        meter_.resetSessionEnergy();
        relayOff_();
        port_.amountFIN = 0.0;
        port_.refundAmount = 0.0;
        port_.paidMinor = 0.0;
        
        port_.paymentStatus = WAITING_PAYMENT;
        port_.chargingStatus = WAITING_TO_CHARGE;
        
        // ВАЖНО: Сбрасываем Prev статусы чтобы следующая транзакция корректно обнаружила изменение!
        port_.paymentStatusPrev = WAITING_PAYMENT;
        port_.chargingStatusPrev = WAITING_TO_CHARGE;

        // Возвращаем терминал в IDL ПОСЛЕ финализации возврата
        UART0_DEBUG_PORT.printf("  → Транзакция завершена, терминал %s возвращён в IDL\n", portName_);
        send_IDL(terminal_);
        stayIDLE_ = true;  // Включаем периодический IDL

        debugRefundTime_ = millis();
        paidDiagnosticPrinted_ = false;  // Сбрасываем флаг для следующей транзакции
        startChargingTimerStarted_ = false;  // Сбрасываем таймер ожидания зарядки
    }
}

// ========== ПРОВЕРКА НАЧАЛА ЗАРЯДКИ ==========

void ChargingPort::checkStartCharging() {
    // Диагностика один раз при обнаружении PAID
    if ((port_.paymentStatus == PAID) && !paidDiagnosticPrinted_) {
        paidDiagnosticPrinted_ = true;
        UART0_DEBUG_PORT.println();
        UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
        UART0_DEBUG_PORT.printf("║ 🔍 ДИАГНОСТИКА ПОРТ %s: ОБНАРУЖЕН СТАТУС PAID\n", portName_);
        UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
        UART0_DEBUG_PORT.printf("║ paymentStatus: %d (PAID=1)\n", port_.paymentStatus);
        UART0_DEBUG_PORT.printf("║ chargingStatus: %d\n", port_.chargingStatus);
        UART0_DEBUG_PORT.printf("║ kWattPerHourAvailable: %.2f кВт·ч\n", port_.kWattPerHourAvailable);
        UART0_DEBUG_PORT.printf("║ meterError: %s\n", port_.meterError ? "ДА ⚠️" : "НЕТ ✓");
        UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
    }
    
    // Запрещаем начало зарядки если счетчик не работает!
    if ((port_.paymentStatus == PAID) && (port_.kWattPerHourAvailable > 0)) {
        if (port_.meterError) {
            // Счетчик не работает - немедленный возврат средств
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ ⛔ ПОРТ %s: ЗАРЯДКА НЕВОЗМОЖНА\n", portName_);
            UART0_DEBUG_PORT.println("║ Причина: Счетчик энергии не отвечает");
            UART0_DEBUG_PORT.println("║ Выполняется полный возврат средств");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
            
            port_.chargingStatus = STOPPING;
            port_.paymentStatus = REFUND;
            port_.amountFIN = 0;  // Полный возврат - ничего не потрачено
            port_.refundAmount = port_.paidMinor;
            port_.opNumberPrev = port_.operationNumber;
            debugRefundTime_ = millis();
            startChargingTimerStarted_ = false;
        } else {
            // Счетчик работает - начинаем зарядку
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ ✓ ПОРТ %s: ПЕРЕХОД К ЗАПУСКУ ЗАРЯДКИ\n", portName_);
            UART0_DEBUG_PORT.println("║ PaymentStatus: PAID → WAITING_PAYMENT");
            UART0_DEBUG_PORT.println("║ ChargingStatus: WAITING_TO_CHARGE → START_TO_CHARGE");
            UART0_DEBUG_PORT.printf("║ Таймаут ожидания: %lu сек\n", START_CHARGING_TIMEOUT_MS / 1000);
            UART0_DEBUG_PORT.printf("║ [DEBUG] chargingStatusPrev: %d\n", port_.chargingStatusPrev);
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
            
            // ВАЖНО: Сбрасываем Prev чтобы гарантировать срабатывание handleChargingStateChange
            port_.chargingStatusPrev = WAITING_TO_CHARGE;
            port_.paymentStatusPrev = PAID;
            
            port_.chargingStatus = START_TO_CHARGE;
            port_.paymentStatus = WAITING_PAYMENT;
            
            // Сбрасываем счётчик энергии сессии для корректного расчёта
            meter_.resetSessionEnergy();
            
            // Запускаем таймер ожидания зарядки
            startChargingTime_ = millis();
            startChargingTimerStarted_ = true;
        }
    }
    
    // Проверяем таймаут ожидания зарядки
    // Если после оплаты мощность не появилась в течение таймаута - возврат денег
    if (startChargingTimerStarted_ && 
        port_.chargingStatus == START_TO_CHARGE &&
        (millis() - startChargingTime_ > START_CHARGING_TIMEOUT_MS)) {
        
        if (meter_.powerW() < 3000.f) {
            // Мощность не появилась - возвращаем деньги
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ ⏱️ ПОРТ %s: ТАЙМАУТ ОЖИДАНИЯ ЗАРЯДКИ\n", portName_);
            UART0_DEBUG_PORT.println("║ Причина: Мощность не появилась после оплаты");
            UART0_DEBUG_PORT.printf("║ Прошло: %lu сек, мощность: %.0f Вт\n", 
                (millis() - startChargingTime_) / 1000, meter_.powerW());
            UART0_DEBUG_PORT.println("║ Выполняется полный возврат средств");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
            
            relayOff_();  // На всякий случай выключаем реле
            
            port_.chargingStatus = STOPPING;
            port_.paymentStatus = REFUND;
            port_.amountFIN = 0;  // Полный возврат - ничего не потрачено
            port_.refundAmount = port_.paidMinor;
            port_.opNumberPrev = port_.operationNumber;
            debugRefundTime_ = millis();
            startChargingTimerStarted_ = false;
            // HTTP уведомление отправится в handlePaymentStateChange() при обработке REFUND
        } else {
            // Мощность появилась - сбрасываем таймер
            startChargingTimerStarted_ = false;
        }
    }
}

// ========== МАШИНА СОСТОЯНИЙ ЗАРЯДКИ ==========

void ChargingPort::updateChargingState() {
    if ((port_.chargingStatus == START_TO_CHARGE) && 
        (meter_.powerW() > 3000.f) && 
        (port_.kWattPerHourAvailable > meter_.sessionEnergyKWh())) {
        port_.chargingStatus = RUNNING;
        port_.paymentStatus = SPENDING;
        startChargingTimerStarted_ = false;  // Зарядка началась, таймер больше не нужен
    } 
    else if ((port_.chargingStatus == RUNNING) && 
             (meter_.powerW() >= 3000.f) && 
             (port_.kWattPerHourAvailable <= meter_.sessionEnergyKWh())) {
        port_.chargingStatus = STOPPING;
        port_.paymentStatus = INSUFFICIENT_FUNDS;
    } 
    else if ((meter_.powerW() < 3000) && 
             (millis() - debugRefundTime_ > 30000) && 
             (port_.kWattPerHourAvailable > meter_.sessionEnergyKWh()) && 
             (port_.paymentStatus == SPENDING)) {
        port_.chargingStatus = STOPPING;
        port_.paymentStatus = REFUND;
        debugRefundTime_ = millis();
    } 
    else if ((meter_.powerW() < 3000) && 
             (millis() - debugRefundTime_ > 3000) && 
             (port_.kWattPerHourAvailable <= meter_.sessionEnergyKWh())) {
        port_.chargingStatus = WAITING_TO_CHARGE;
        port_.paymentStatus = WAITING_PAYMENT;
    }
}

// ========== ОБРАБОТКА ИЗМЕНЕНИЯ СОСТОЯНИЯ ЗАРЯДКИ ==========

void ChargingPort::handleChargingStateChange() {
    // Отладка: всегда показываем если был вызван
    if (port_.chargingStatus != port_.chargingStatusPrev) {
        UART0_DEBUG_PORT.printf("[DEBUG] %s: chargingStatus changed %d → %d\n", 
            portName_, port_.chargingStatusPrev, port_.chargingStatus);
        switch (port_.chargingStatus) {
        case WAITING_TO_CHARGE:
            relayOff_();
            break;

        case START_TO_CHARGE:
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ ▶ ПОРТ %s: НАЧАЛО СЕССИИ ЗАРЯДКИ\n", portName_);
            UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
            UART0_DEBUG_PORT.print("║ Оплачено: ");
            UART0_DEBUG_PORT.print((float)port_.paidMinor / 100.0, 2);
            UART0_DEBUG_PORT.print(" руб. (");
            UART0_DEBUG_PORT.print(port_.paidMinor);
            UART0_DEBUG_PORT.println(" коп.)");
            UART0_DEBUG_PORT.print("║ Доступно энергии: ");
            UART0_DEBUG_PORT.print(port_.kWattPerHourAvailable, 2);
            UART0_DEBUG_PORT.println(" кВт·ч");
            UART0_DEBUG_PORT.print("║ Тариф: ");
            UART0_DEBUG_PORT.print(PRICE_FOR_ONE_KWHOUR);
            UART0_DEBUG_PORT.println(" коп./кВт·ч");
            UART0_DEBUG_PORT.println("║ Реле: ВКЛ | Счетчик: СБРОС");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

            relayOn_();
            meter_.resetSessionEnergy();
            break;

        case RUNNING:
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ ⚡ ПОРТ %s: ИДЕТ ЗАРЯДКА\n", portName_);
            UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
            UART0_DEBUG_PORT.print("║ Мощность: ");
            UART0_DEBUG_PORT.print(meter_.powerW(), 1);
            UART0_DEBUG_PORT.println(" Вт");
            UART0_DEBUG_PORT.print("║ Время начала: ");
            UART0_DEBUG_PORT.println(getISO8601Time());
            UART0_DEBUG_PORT.println("║ Линия данных: ПОДКЛЮЧЕНА (реле ВКЛ)");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
            break;

        case STOPPING:
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ ⏹ ПОРТ %s: ЗАРЯДКА ОСТАНОВЛЕНА\n", portName_);
            UART0_DEBUG_PORT.print("║ Мощность: ");
            UART0_DEBUG_PORT.print(meter_.powerW(), 1);
            UART0_DEBUG_PORT.println(" Вт");
            UART0_DEBUG_PORT.println("║ Линия данных: ОТКЛЮЧЕНА (реле ВЫКЛ)");
            UART0_DEBUG_PORT.println("║ Ожидание финализации транзакции...");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

            relayOff_();
            break;

        default:
            break;
        }

        // Отправка статуса через MQTT (неблокирующая)
        char statusMsg[128];
        snprintf(statusMsg, sizeof(statusMsg), 
            "[%s] Статус: оплата=%d, зарядка=%d, энергия=%.2f кВт·ч",
            portName_, port_.paymentStatus, port_.chargingStatus, meter_.sessionEnergyKWh());
        mqtt_send_debug(statusMsg);

        port_.chargingStatusPrev = port_.chargingStatus;
    }
}

// ========== ОБРАБОТКА ИЗМЕНЕНИЯ СОСТОЯНИЯ ОПЛАТЫ ==========

void ChargingPort::handlePaymentStateChange() {
    if (port_.paymentStatus != port_.paymentStatusPrev) {
        switch (port_.paymentStatus) {
        case PAID:
            break;

        case WAITING_PAYMENT:
            break;

        case SPENDING:
            break;

        case INSUFFICIENT_FUNDS:
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ 💰 ПОРТ %s: СРЕДСТВА ИЗРАСХОДОВАНЫ\n", portName_);
            UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
            UART0_DEBUG_PORT.print("║ Было оплачено: ");
            UART0_DEBUG_PORT.print((float)port_.paidMinor / 100.0, 2);
            UART0_DEBUG_PORT.print(" руб. (");
            UART0_DEBUG_PORT.print(port_.kWattPerHourAvailable, 2);
            UART0_DEBUG_PORT.println(" кВт·ч)");
            UART0_DEBUG_PORT.print("║ Израсходовано: ");
            UART0_DEBUG_PORT.print(meter_.sessionEnergyKWh(), 2);
            UART0_DEBUG_PORT.print(" кВт·ч (тест x");
            UART0_DEBUG_PORT.print((int)TEST_ENERGY_MULTIPLIER);
            UART0_DEBUG_PORT.print(", реальная: ");
            UART0_DEBUG_PORT.print(meter_.sessionEnergyKWh() / TEST_ENERGY_MULTIPLIER, 2);
            UART0_DEBUG_PORT.println(" кВт·ч)");
            UART0_DEBUG_PORT.print("║ Время окончания: ");
            UART0_DEBUG_PORT.println(getISO8601Time());
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");

            // Отправляем РЕАЛЬНУЮ энергию через MQTT (делим на TEST_ENERGY_MULTIPLIER)
            mqtt_send_payment(port_.paidTimeUTC.c_str(), port_.paidMinor, 0, meter_.sessionEnergyKWh() / TEST_ENERGY_MULTIPLIER, portName_);
            
            // Вся оплаченная сумма израсходована — финализируем операцию через FIN
            port_.amountFIN = port_.paidMinor;
            port_.opNumberPrev = port_.operationNumber;
            send_FIN(port_.amountFIN, port_.opNumberPrev, terminal_);

            port_.kWattPerHourAvailable = 0.0;
            port_.paidMinor = 0.0;
            meter_.resetSessionEnergy();
            
            // Сбрасываем статусы для следующей транзакции
            port_.chargingStatus = WAITING_TO_CHARGE;
            port_.paymentStatus = WAITING_PAYMENT;
            port_.chargingStatusPrev = WAITING_TO_CHARGE;
            port_.paymentStatusPrev = WAITING_PAYMENT;
            
            // Возвращаем терминал в IDL ПОСЛЕ финализации
            UART0_DEBUG_PORT.printf("  → Транзакция завершена, терминал %s возвращён в IDL\n", portName_);
            send_IDL(terminal_);
            stayIDLE_ = true;
            paidDiagnosticPrinted_ = false;  // Сбрасываем флаг для следующей транзакции
            break;

        case REFUND:
            // Учитываем TEST_ENERGY_MULTIPLIER при расчете стоимости израсходованной энергии
            port_.amountFIN = (meter_.sessionEnergyKWh() / TEST_ENERGY_MULTIPLIER) * PRICE_FOR_ONE_KWHOUR;
            port_.opNumberPrev = port_.operationNumber;
            port_.refundAmount = long(port_.paidMinor) - port_.amountFIN;
            
            UART0_DEBUG_PORT.println();
            UART0_DEBUG_PORT.println("╔═════════════════════════════════════════════════════");
            UART0_DEBUG_PORT.printf("║ 💸 ПОРТ %s: ВОЗВРАТ СРЕДСТВ\n", portName_);
            UART0_DEBUG_PORT.println("╠─────────────────────────────────────────────────────");
            UART0_DEBUG_PORT.print("║ Было оплачено: ");
            UART0_DEBUG_PORT.print((float)port_.paidMinor / 100.0, 2);
            UART0_DEBUG_PORT.print(" руб. (");
            UART0_DEBUG_PORT.print(port_.paidMinor);
            UART0_DEBUG_PORT.println(" коп.)");
            UART0_DEBUG_PORT.print("║ Списано: ");
            UART0_DEBUG_PORT.print((float)port_.amountFIN / 100.0, 2);
            UART0_DEBUG_PORT.print(" руб. (");
            UART0_DEBUG_PORT.print(port_.amountFIN);
            UART0_DEBUG_PORT.println(" коп.)");
            UART0_DEBUG_PORT.print("║ ► К ВОЗВРАТУ: ");
            UART0_DEBUG_PORT.print((float)port_.refundAmount / 100.0, 2);
            UART0_DEBUG_PORT.print(" руб. (");
            UART0_DEBUG_PORT.print(port_.refundAmount);
            UART0_DEBUG_PORT.println(" коп.)");
            UART0_DEBUG_PORT.print("║ Израсходовано энергии: ");
            UART0_DEBUG_PORT.print(meter_.sessionEnergyKWh(), 2);
            UART0_DEBUG_PORT.print(" кВт·ч (тест x");
            UART0_DEBUG_PORT.print((int)TEST_ENERGY_MULTIPLIER);
            UART0_DEBUG_PORT.print(", реальная: ");
            UART0_DEBUG_PORT.print(meter_.sessionEnergyKWh() / TEST_ENERGY_MULTIPLIER, 2);
            UART0_DEBUG_PORT.println(" кВт·ч)");
            UART0_DEBUG_PORT.println("║ Ожидание отправки FIN через ");
            UART0_DEBUG_PORT.print(REFUNDPERIOD / 1000);
            UART0_DEBUG_PORT.println(" сек...");
            UART0_DEBUG_PORT.println("╚═════════════════════════════════════════════════════");
            
            closeTransaction(terminal_, port_, "REFUND");

            // Отправляем РЕАЛЬНУЮ энергию через MQTT (делим на TEST_ENERGY_MULTIPLIER)
            mqtt_send_payment(port_.paidTimeUTC.c_str(), port_.paidMinor, port_.refundAmount, meter_.sessionEnergyKWh() / TEST_ENERGY_MULTIPLIER, portName_);
            debugRefundTime_ = millis();
            meter_.resetSessionEnergy();
            break;
        
        default:
            break;
        }

        // Отправка статуса оплаты через MQTT (неблокирующая)
        char paymentMsg[128];
        snprintf(paymentMsg, sizeof(paymentMsg), 
            "[%s] Оплата: статус=%d, энергия=%.2f кВт·ч, мощность=%.0f Вт",
            portName_, port_.paymentStatus, meter_.sessionEnergyKWh(), meter_.powerW());
        mqtt_send_debug(paymentMsg);

        port_.paymentStatusPrev = port_.paymentStatus;
    }
}

