#include "EEPROM.h"

#define EEPROM_SIZE 100


EEPROM::EEPROM() {
    memory = new uint8_t[EEPROM_SIZE];
    memset(memory, 0x00, EEPROM_SIZE);
}


EEPROM::~EEPROM() {
    delete [] memory;
}


uint8_t EEPROM::read(int index) {
    return memory[index % EEPROM_SIZE];
}


void EEPROM::write(int index, uint8_t value) {
    memory[index % EEPROM_SIZE] = value;
}


void EEPROM::update(int index, uint8_t value) {
    if (memory[index % EEPROM_SIZE] != value) {
        memory[index % EEPROM_SIZE] = value;
    }
}

