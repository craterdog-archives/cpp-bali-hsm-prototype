#include "EEPROM.h"


int EEPROMClass::length() {
    return EEPROM_SIZE;
}

uint8_t EEPROMClass::read(int index) {
    return memory[index % EEPROM_SIZE];
}


void EEPROMClass::write(int index, uint8_t value) {
    memory[index % EEPROM_SIZE] = value;
}


void EEPROMClass::update(int index, uint8_t value) {
    if (memory[index % EEPROM_SIZE] != value) {
        memory[index % EEPROM_SIZE] = value;
    }
}

