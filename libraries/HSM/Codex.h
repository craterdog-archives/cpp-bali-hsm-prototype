/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#ifndef CODEX_H
#define CODEX_H

#include <inttypes.h>
#include <stddef.h>

class Codex final {
  public:
    /**
     * This function is passed a byte array and the length of the byte array.
     * It returns a string containing the corresponding base 32 encoding of
     * the byte array.
     */
    static const char* encode(const uint8_t* bytes, size_t length);

    /**
     * This function is passed a string containing the base 32 encoding of a
     * byte array. It returns the corresponding decoded byte array.
     */
    static const uint8_t* decode(const char* base32);

  private:
    // These are private since this is a utility class with only static functions.
    Codex();
   ~Codex();
};

#endif
