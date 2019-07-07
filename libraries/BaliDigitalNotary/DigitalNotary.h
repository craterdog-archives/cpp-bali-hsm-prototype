/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This code is free software; you can redistribute it and/or modify it *
 * under the terms of The MIT License (MIT), as published by the Open   *
 * Source Initiative. (See http://opensource.org/licenses/MIT)          *
 ************************************************************************/
#ifndef DIGITAL_NOTARY_H
#define DIGITAL_NOTARY_H


class DigitalNotary {

  public:
    DigitalNotary();
    virtual ~DigitalNotary();
    char* generateKeyPair();  // returns the notarized public certificate
    void forgetKeyPair();
    const char* notarizeMessage(const char* message);  // returns the notary seal
    bool sealIsValid(const char* message, const char* seal);

  private:
    uint8_t privateKey[66];
    uint8_t publicKey[132];
    /*
     * This string acts as a lookup table for mapping five bit values to base 32 characters.
     * It eliminate 4 vowels ("E", "I", "O", "U") to reduce any confusion with 0 and O, 1 and I;
     * and reduce the likelihood of *actual* (potentially offensive) words from being included
     * in a base 32 string. Only uppercase letters are allowed.
    static const char* BASE32 = "0123456789ABCDFGHJKLMNPQRSTVWXYZ";
    static const char* base32Encode(const uint8_t* bytes, int length);
    static void base32EncodeNext(const uint8_t previous, const uint8_t current, const int byteIndex, char* base32);
    static void base32EncodeLast(const uint8_t last, const int byteIndex, char* base32);
    static void base32Decode(const char* base32, uint8_t* bytes);
    static void base32DecodeNext(const uint8_t chunk, const int characterIndex, uint8_t* bytes);
    static void base32DecodeLast(const uint8_t chunk, const int characterIndex, uint8_t* bytes);
     */

};

#endif
