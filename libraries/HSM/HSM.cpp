/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#include <string.h>
#include <EEPROM.h>
#include "SHA512.h"  // change to <SHA512.h> for real implementation
#include "Ed25519.h"  // change to <Ed25519.h> for real implementation
#include "HSM.h"


// PRIVATE FUNCTIONS

/**
 * This function loads the saved state of the hardware security module (HSM)
 * from the EEPROM drive.
 */
void loadState(uint8_t publicKey[32], uint8_t encryptedKey[32]) {
    for (size_t i = 0; i < 32; i++) {
        // the bytes are interleaved
        publicKey[i] = EEPROM.read(i * 2);
        encryptedKey[i] = EEPROM.read(i * 2 + 1);
    }
}


/**
 * This function saves the current state of the hardware security module (HSM)
 * to the EEPROM drive.
 */
void saveState(const uint8_t publicKey[32], const uint8_t encryptedKey[32]) {
    for (size_t i = 0; i < 32; i++) {
        // interleave the bytes
        EEPROM.write(i * 2, publicKey[i]);
        EEPROM.write(i * 2 + 1, encryptedKey[i]);
    }
}


/**
 * This function erases each byte in the specified data array.
 */
void erase(uint8_t* data, size_t size = 32) {
    memset(data, 0x00, size);
}


/**
 * This function performs the exclusive or (XOR) operation on each byte in the
 * first two data arrays and places the resulting byte in the third data array.
 */
void XOR(const uint8_t a[32], const uint8_t b[32], uint8_t c[32]) {
    for (size_t i = 0; i < 32; i++) {
        c[i] = a[i] xor b[i];
    }
}


/**
 * This function uses a simple one-time-pad algorithm (XOR) to encrypt a private
 * key using a secret key and then erases both the secret key and private key.
 */
void encryptKey(uint8_t secretKey[32], uint8_t privateKey[32], uint8_t encryptedKey[32]) {
    XOR(secretKey, privateKey, encryptedKey);
    erase(secretKey);
    erase(privateKey);
}


/**
 * This function decrypts a private key that was encrypted using a secret key and
 * a simple one-time-pad algorithm (XOR) and then erases the secret key.
 */
void decryptKey(uint8_t secretKey[32], const uint8_t encryptedKey[32], uint8_t privateKey[32]) {
    XOR(secretKey, encryptedKey, privateKey);
    erase(secretKey);
}


/**
 * This function returns whether or not the specified public-private key pair is invalid.
 */
bool invalidKeyPair(const uint8_t publicKey[32], const uint8_t privateKey[32]) {
    uint8_t signature[64];
    Ed25519::sign(signature, privateKey, publicKey, (const void*) privateKey, 32);
    return !Ed25519::verify(signature, publicKey, (const void*) privateKey, 32);
}


// PUBLIC METHODS

HSM::HSM() {
    transitioning = false;
    erase(publicKey);
    erase(encryptedKey);
    erase(oldPublicKey);
    erase(oldEncryptedKey);
    loadState(publicKey, encryptedKey);
}


HSM::~HSM() {
    transitioning = false;
    erase(publicKey);
    erase(encryptedKey);
    erase(oldPublicKey);
    erase(oldEncryptedKey);
}


// NOTE: The returned message digest must be deleted by the calling program.
const uint8_t* HSM::digestMessage(const char* message) {
    SHA512 digester;
    size_t messageLength = strlen(message);
    uint8_t* digest = new uint8_t[64];
    digester.update((const void*) message, messageLength);
    digester.finalize(digest, 64);
    return digest;
}


// NOTE: the specified public key need not be the same public key that is associated
// with the hardware security module (HSM). It should be the key associated with the
// private key that supposedly signed the message.
bool HSM::validSignature(const uint8_t aPublicKey[32], const char* message, const uint8_t signature[64]) {
    size_t messageLength = strlen(message);
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) message, messageLength);
    return isValid;
}


// INVARIANT: newSecretKey and secretKey will have been erased when this function returns.
// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(uint8_t newSecretKey[32], uint8_t secretKey[32]) {
    uint8_t privateKey[32];

    // handle existing keys TODO: what if we are in transitioning mode?
    if (secretKey) {
        decryptKey(secretKey, encryptedKey, privateKey);  // erases secretKey

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            // clean up and bail
            erase(newSecretKey);
            erase(privateKey);
            return 0;  // TODO: analyze as possible side channel
        }

        // save copies of the old public and encrypted keys
        transitioning = true;
        memcpy(oldPublicKey, publicKey, 32);
        memcpy(oldEncryptedKey, encryptedKey, 32);
    }

    // generate a new key pair
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    encryptKey(newSecretKey, privateKey, encryptedKey);  // erases newSecretKey and privateKey
    saveState(publicKey, encryptedKey);

    // return a copy of the public key
    uint8_t* copy = new uint8_t[32];
    memcpy(copy, publicKey, 32);
    return copy;
}


// INVARIANT: secretKey will have been erased when this function returns.
// NOTE: The returned signature must be deleted by the calling program.
const uint8_t* HSM::signMessage(uint8_t secretKey[32], const char* message) {
    uint8_t privateKey[32];

    // handle the transitioning state
    const uint8_t* currentPublicKey = transitioning ? oldPublicKey : publicKey;
    const uint8_t* currentEncryptedKey = transitioning ? oldEncryptedKey : encryptedKey;

    // decrypt the private key
    decryptKey(secretKey, currentEncryptedKey, privateKey);  // erases secretKey

    // validate the private key
    if (invalidKeyPair(currentPublicKey, privateKey)) {
        // clean up and bail
        erase(privateKey);
        return 0;  // TODO: analyze as possible side channel
    }

    // sign the message using the private key
    uint8_t* signature = new uint8_t[64];
    size_t messageLength = strlen(message);
    Ed25519::sign(signature, privateKey, currentPublicKey, (const void*) message, messageLength);

    // handle the transitioning state
    if (transitioning) {
        erase(oldPublicKey);
        erase(oldEncryptedKey);
        transitioning = false;
    }

    // return the message signature
    return signature;
}


// INVARIANT: No trace of any keys will remain when this function returns.
void HSM::eraseKeys() {

    // erase all keys in memory
    erase(publicKey);
    erase(encryptedKey);
    erase(oldPublicKey);
    erase(oldEncryptedKey);

    // erase the state of the EEPROM drive
    saveState(publicKey, encryptedKey);
}

