/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This code is free software; you can redistribute it and/or modify it *
 * under the terms of The MIT License (MIT), as published by the Open   *
 * Source Initiative. (See http://opensource.org/licenses/MIT)          *
 ************************************************************************/
#ifndef DIGITAL_NOTARY_h
#define DIGITAL_NOTARY_h


class DigitalNotary {

  public:
    static void generateKeyPair(uint8_t privateKey[66], uint8_t publicKey[132]);
    static char* notarizeMessage(const char* message, const uint8_t privateKey[66]);
    static bool sealIsValid(const char* message, const char* seal, const uint8_t publicKey[132]);

  private:
    DigitalNotary() {}
   ~DigitalNotary() {}
    static char* base32Encode(const uint8_t bytes[132]);
    static void base32Decode(const char* base32, uint8_t bytes[132]);

};

#endif
