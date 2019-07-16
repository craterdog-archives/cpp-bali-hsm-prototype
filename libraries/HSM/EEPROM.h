#ifndef EEPROM_h
#define EEPROM_h

#define EEPROM_SIZE 100

#include <inttypes.h>


class EEPROMClass final {

  public:
    int length();
    uint8_t read(int index);
    void write(int index, uint8_t value);
    void update(int index, uint8_t value);

  private:
    uint8_t memory[EEPROM_SIZE];

};

static EEPROMClass EEPROM;

#endif
