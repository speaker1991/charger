#pragma once
#include <Arduino.h>

// WiFi и HTTP (ESP32)
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

#include <SoftwareSerial.h>
#include <map>
#include <vector>
#include <string>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <NTPClient.h>
#include <ESPDash.h>
#include <ModbusMaster.h>

#include <SD.h>

#include "uart.h"
#include "vtk.h"
#include "utils.h"
#include "payments.h"
#include "http_api.h"
#include "relay.h"
#include "wifi_connection.h"
#include "modbus.h"
#include "mqtt_client.h"
#include "logger.h"

// Флаг режима отладки терминала (устанавливается в main.cpp)
extern bool terminalDebugMode;

// Флаг вывода лога "Периодическая отправка: сообщение №..."
extern bool idleLogEnabled;

// Режим бесплатной зарядки (без терминала)
extern bool freeChargingMode_A;
extern bool freeChargingMode_B;

