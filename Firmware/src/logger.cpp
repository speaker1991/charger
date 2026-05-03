#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "main.h"

// Пины как в рабочем тесте
static constexpr int PIN_SD_CS   = 10;
static constexpr int PIN_SD_MOSI = 11;
static constexpr int PIN_SD_MISO = 12;
static constexpr int PIN_SD_SCK  = 13;

// Частота SPI как в тесте
static constexpr uint32_t SD_SPI_HZ = 4000000;

// FSPI под SD
static SPIClass spiSD(FSPI);

static bool sdInitialized = false;

// Вызвать один раз в setup(): initSDCard();
// Возвращает true если SD готова
bool initSDCard() {
    if (sdInitialized) return true;

    // как в твоём рабочем примере
    pinMode(PIN_SD_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);

    spiSD.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);

    if (!SD.begin(PIN_SD_CS, spiSD, SD_SPI_HZ)) {
        // можно сделать Serial.println(...) если нужно
        sdInitialized = false;
        return false;
    }

    sdInitialized = true;
    return true;
}

// Имя лог-файла по текущей дате: /trans_2025-12-04.log
static String getCurrentLogFilename() {
    String iso = getISO8601Time();  // ожидается формат типа 2025-12-04T12:34:56Z
    if (iso.length() < 10) {
        return "/trans_unknown.log";
    }
    String date = iso.substring(0, 10); // "2025-12-04"
    return "/trans_" + date + ".log";
}

static bool appendLineToFile(const String& filename, const String& line) {
    if (!sdInitialized) {
        // если init не вызывали — попробуем поднять SD автоматически
        if (!initSDCard()) return false;
    }

    File f = SD.open(filename.c_str(), FILE_APPEND);
    if (!f) {
        return false;
    }

    f.println(line);
    f.flush(); // полезно для логов, чтобы не потерять при ребуте
    f.close();
    return true;
}

void logTransactionToSD(Terminal terminal, const transactions& port, const String& endReason) {
    String filename = getCurrentLogFilename();

    const char* termName = (terminal == TERMINAL_A) ? "A" : "B";
    String endTime = getISO8601Time();

    String block;
    block.reserve(420);

    block  = "================ TRANSACTION ================\n";
    block += "Port: ";                block += termName;                      block += "\n";
    block += "Operation: ";           block += String(port.operationNumber);  block += "\n";
    block += "PaymentTime: ";         block += port.paidTimeUTC;              block += "\n";
    block += "AmountPaidMinor: ";     block += String(port.paidMinor);        block += " (";
    block += String((float)port.paidMinor / 100.0f, 2);                       block += " RUB)\n";
    block += "RefundAmountMinor: ";   block += String(port.refundAmount);     block += " (";
    block += String((float)port.refundAmount / 100.0f, 2);                    block += " RUB)\n";
    block += "EndTime: ";             block += endTime;                       block += "\n";
    block += "EndReason: ";           block += endReason;                     block += "\n";
    block += "MeterError: ";          block += (port.meterError ? "YES" : "NO"); block += "\n";
    block += "============================================";

    (void)appendLineToFile(filename, block);
}

void closeTransaction(Terminal terminal, transactions& port, const String& endReason) {
    if (!port.transactionActive) {
        return;
    }

    // 1) Логируем транзакцию
    logTransactionToSD(terminal, port, endReason);

    // 2) Сбрасываем состояние
    port.transactionActive      = false;
    port.paidMinor              = 0;
    port.kWattPerHourAvailable  = 0.0f;
    port.refundAmount           = 0;
    port.amountFIN              = 0;
    port.operationNumber        = 0;
    port.paidTimeUTC            = "";
    port.paymentStatus          = WAITING_PAYMENT;
    port.chargingStatus         = WAITING_TO_CHARGE;
}
