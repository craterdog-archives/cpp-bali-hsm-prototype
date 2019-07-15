#ifndef EEPROM_h
#define EEPROM_h

#include <inttypes.h>


class EEPROM final {

  public:
    static uint8_t read(int index);
    static void write(int index, uint8_t value);
    static void update(int index, uint8_t value);

  private:
    static uint8_t* memory;

};

#endif
