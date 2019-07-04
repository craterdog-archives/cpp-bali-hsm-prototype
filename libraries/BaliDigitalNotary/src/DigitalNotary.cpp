/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This code is free software; you can redistribute it and/or modify it *
 * under the terms of The MIT License (MIT), as published by the Open   *
 * Source Initiative. (See http://opensource.org/licenses/MIT)          *
 ************************************************************************/
#include "DigitalNotary.h"
#include <HASH512.h>
#include <P521.h>


/**
 * This function generates a new public-private key pair.
 * 
 * \return An object containing the new public and private keys.
 */
void DigitalNotary::generateKeyPair(uint8_t privateKey[66], uint8_t publicKey[132]) {
    P521::generatePrivateKey(privateKey);
    P521::derivePublicKey(publicKey, privateKey);
};


/**
 * This function generates a digital signature of the specified message using
 * the specified private key. The resulting digital signature can then be
 * verified using the corresponding public key.
 * 
 * \param message The message to be digitally signed.
 * \param privateKey A byte buffer containing the private key.
 * \return A byte buffer containing the resulting digital signature.
 */
char* DigitalNotary::notarizeMessage(const char *message, const uint8_t privateKey[66]) {
    SHA512 Hash;
    uint8_t signature[132];
    P521::sign(signature, privateKey, message, strlen(message), &Hash);
    char* seal = base32Encode(signature);
    return seal;
};


/**
 * This function uses the specified public key to determine whether or not
 * the specified digital signature was generated using the corresponding
 * private key on the specified message.
 *
 * \param message The digitally signed message.
 * \param publicKey A byte buffer containing the public key.
 * \param signature A byte buffer containing the digital signature allegedly generated using the corresponding private key.
 * \return Whether or not the digital signature is valid.
 */
bool DigitalNotary::sealIsValid(const char *message, const char* seal, const uint8_t publicKey[132]) {
    SHA512 Hash;
    uint8_t signature[132];
    base32Decode(seal, signature);
    bool isValid = P521::verify(signature, publicKey, message, strlen(message), &Hash);
    return isValid;
};

