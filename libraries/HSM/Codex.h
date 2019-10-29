/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This source code is for reference purposes only.  It is protected by *
 * US Patent 9,853,813 and any use of this source code will be deemed   *
 * an infringement of the patent.  Crater Dog Technologies(TM) retains  *
 * full ownership of this source code.                                  *
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
    static char* encode(const uint8_t* bytes, size_t length);

    /**
     * This function is passed a string containing the base 32 encoding of a
     * byte array. It returns the corresponding decoded byte array.
     */
    static uint8_t* decode(const char* base32);

  private:
    // These are private since this is a utility class with only static functions.
    Codex();
   ~Codex();
};

#endif
