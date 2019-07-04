/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This code is free software; you can redistribute it and/or modify it *
 * under the terms of The MIT License (MIT), as published by the Open   *
 * Source Initiative. (See http://opensource.org/licenses/MIT)          *
 ************************************************************************/
#include <SHA512.h>
#include <P521.h>
#include <string.h>
#include "DigitalNotary.h"

#define PRIVATE_SIZE 66
#define PUBLIC_SIZE 132

DigitalNotary::DigitalNotary() {
}


DigitalNotary::~DigitalNotary() {
    for (int i = 0; i < PRIVATE_SIZE; i++) privateKey[i] = 0;
    for (int j = 0; j < PUBLIC_SIZE; j++) publicKey[j] = 0;
}


/**
 * This function generates a new public-private key pair.
 * 
 * \return An object containing the new public and private keys.
 */
char* DigitalNotary::generateKeyPair() {
    P521::generatePrivateKey(privateKey);
    P521::derivePublicKey(publicKey, privateKey);
    // TODO: generate and notarize the public certificate
    return 0;
}


/**
 * This function generates a digital signature of the specified message using
 * the specified private key. The resulting digital signature can then be
 * verified using the corresponding public key.
 * 
 * \param message The message to be digitally signed.
 * \return A byte buffer containing the resulting digital signature.
 */
char* DigitalNotary::notarizeMessage(const char* message) {
    SHA512 Hash;
    uint8_t signature[PUBLIC_SIZE];
    P521::sign(signature, privateKey, message, strlen(message), &Hash);
    char* seal = base32Encode(signature);
    return seal;
}


/**
 * This function uses the specified public key to determine whether or not
 * the specified digital signature was generated using the corresponding
 * private key on the specified message.
 *
 * \param message The digitally signed message.
 * \param seal A byte buffer containing the digital signature allegedly generated using the corresponding private key.
 * \return Whether or not the digital signature is valid.
 */
bool DigitalNotary::sealIsValid(const char* message, const char* seal) {
    SHA512 Hash;
    uint8_t signature[PUBLIC_SIZE];
    base32Decode(seal, signature);
    bool isValid = P521::verify(signature, publicKey, message, strlen(message), &Hash);
    return isValid;
}


char* DigitalNotary::base32Encode(const uint8_t bytes[PUBLIC_SIZE]) {
    return 0;
}


void DigitalNotary::base32Decode(const char* base32, uint8_t bytes[PUBLIC_SIZE]) {
}

