#pragma once

/*
Реле управляют линиями данных между пистолетами и контроллером зарядной станции:
- HIGH (ВКЛ) = линия данных подключена, станция видит пистолет, зарядка возможна
- LOW (ВЫКЛ) = линия данных отключена, станция не видит пистолет, зарядка останавливается
*/

#define RELAY_PIN_PORT_A 8   // GPIO8 управляет линией данных порта A
#define RELAY_PIN_PORT_B 2   // GPIO2 управляет линией данных порта B

void init_relay();

// Порт A: отключить линию данных от контроллера (остановить зарядку)
inline void ralay_portA_off(){
    digitalWrite(RELAY_PIN_PORT_A, LOW);
}

// Порт A: подключить линию данных к контроллеру (разрешить зарядку)
inline void ralay_portA_on(){
    digitalWrite(RELAY_PIN_PORT_A, HIGH);
}

// Порт B: отключить линию данных от контроллера (остановить зарядку)
inline void ralay_portB_off(){
    digitalWrite(RELAY_PIN_PORT_B, LOW);
}

// Порт B: подключить линию данных к контроллеру (разрешить зарядку)
inline void ralay_portB_on(){
    digitalWrite(RELAY_PIN_PORT_B, HIGH);
}