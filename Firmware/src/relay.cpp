#include "main.h"


void init_relay(){
    pinMode(RELAY_PIN_PORT_A, OUTPUT);   // задаём пин как выход
    pinMode(RELAY_PIN_PORT_B, OUTPUT);   // задаём пин как выход

    ralay_portA_off();
    ralay_portB_off();
}

