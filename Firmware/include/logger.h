#pragma once
#include "main.h"  // здесь должны быть: enum Terminal, struct transactions, enum PaymentStatus/ChargingStatus, getISO8601Time()

// Инициализация SD-карты (вызвать один раз в setup)
bool initSDCard();

// Логирование завершённой транзакции (обычно через closeTransaction)
void logTransactionToSD(Terminal terminal, const transactions& port, const String& endReason);

// Закрыть транзакцию: залогировать и сбросить состояние
void closeTransaction(Terminal terminal, transactions& port, const String& endReason);
