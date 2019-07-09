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

// sizes in bytes
const size_t PRIVATE_SIZE = 32;
const size_t PUBLIC_SIZE = 32;

class DigitalNotary {

  public:
    DigitalNotary();
    virtual ~DigitalNotary();
    char* generateKeyPair();  // returns the notarized public certificate
    void forgetKeyPair();
    const char* notarizeMessage(const char* message);  // returns the notary seal
    bool sealIsValid(const char* message, const char* seal);

  private:
    uint8_t publicKey[PUBLIC_SIZE];
    uint8_t privateKey[PRIVATE_SIZE];

};

#endif
