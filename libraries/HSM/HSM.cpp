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
void loadState(uint8_t accountId[20], uint8_t publicKey[32], uint8_t encryptedKey[32]) {
    size_t index = 0;

    // load the account Id
    for (size_t i = 0; i < 20; i++) {
        accountId[i] = EEPROM.read(index++);
    }

    // load the public key
    for (size_t i = 0; i < 32; i++) {
        publicKey[i] = EEPROM.read(index++);
    }

    // load the encrypted key
    for (size_t i = 0; i < 32; i++) {
        encryptedKey[i] = EEPROM.read(index++);
    }
}


/**
 * This function saves the current state of the hardware security module (HSM)
 * to the EEPROM drive.
 */
void saveState(
    const uint8_t accountId[20],
    const uint8_t publicKey[32] = 0,
    const uint8_t encryptedKey[32] = 0
) {
    size_t index = 0;

    // save the account Id
    for (size_t i = 0; i < 20; i++) {
        EEPROM.write(index++, accountId[i]);
    }

    // save the public key
    if (publicKey) {
        for (size_t i = 0; i < 32; i++) {
            EEPROM.write(index++, publicKey[i]);
        }
    }

    // save the encrypted key
    if (encryptedKey) {
        for (size_t i = 0; i < 32; i++) {
            EEPROM.write(index++, encryptedKey[i]);
        }
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
    // allocate space for state
    accountId = new uint8_t[20];
    publicKey = new uint8_t[32];
    encryptedKey = new uint8_t[32];

    // load the state from persistent memory
    loadState(accountId, publicKey, encryptedKey);

    // check to see if an accountId exists
    for (size_t i = 0; i < 20; i++) {
        if (accountId[i] != 0x00) return;
    }

    // if no accountId, delete the state
    delete [] accountId;
    delete [] publicKey;
    delete [] encryptedKey;
}


HSM::~HSM() {
    if (accountId) {
        erase(accountId, 20);
        delete [] accountId;
    }
    if (publicKey) {
        erase(publicKey);
        delete [] publicKey;
        erase(encryptedKey);
        delete [] encryptedKey;
    }
    if (previousPublicKey) {
        erase(previousPublicKey);
        delete [] previousPublicKey;
        erase(previousEncryptedKey);
        delete [] previousEncryptedKey;
    }
}


bool HSM::registerAccount(const uint8_t newAccountId[20]) {
    if (accountId) return false;  // already registered
    accountId = new uint8_t[20];
    memcpy(accountId, newAccountId, 20);
    saveState(accountId);
    return true;
}


// NOTE: The returned message digest must be deleted by the calling program.
const uint8_t* HSM::digestMessage(const uint8_t accountId[20], const char* message) {
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
bool HSM::validSignature(
    const uint8_t accountId[20],
    const char* message,
    const uint8_t signature[64],
    const uint8_t aPublicKey[32]
) {
    size_t messageLength = strlen(message);
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) message, messageLength);
    return isValid;
}


// INVARIANT: newSecretKey and secretKey will have been erased when this function returns.
// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(const uint8_t accountId[20], uint8_t newSecretKey[32], uint8_t secretKey[32]) {
    uint8_t privateKey[32];

    // handle any previous keys
    if (previousPublicKey) {
        // roll-back the previous regeneration attempt
        memcpy(publicKey, previousPublicKey, 32);
        memcpy(encryptedKey, previousEncryptedKey, 32);
        saveState(accountId, publicKey, encryptedKey);
        erase(previousPublicKey);
        delete [] previousPublicKey;
        erase(previousEncryptedKey);
        delete [] previousEncryptedKey;
    }

    // handle existing keys
    if (secretKey) {
        decryptKey(secretKey, encryptedKey, privateKey);  // erases secretKey

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            // clean up and bail
            erase(newSecretKey);
            erase(privateKey);
            return 0;  // TODO: analyze as possible side channel
        }

        // save copies of the previous public and encrypted keys
        previousPublicKey = new uint8_t[32];
        memcpy(previousPublicKey, publicKey, 32);
        previousEncryptedKey = new uint8_t[32];
        memcpy(previousEncryptedKey, encryptedKey, 32);
    }

    // generate a new key pair
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    encryptKey(newSecretKey, privateKey, encryptedKey);  // erases newSecretKey and privateKey
    saveState(accountId, publicKey, encryptedKey);

    // return a copy of the public key
    uint8_t* copy = new uint8_t[32];
    memcpy(copy, publicKey, 32);
    return copy;
}


// INVARIANT: secretKey will have been erased when this function returns.
// NOTE: The returned signature must be deleted by the calling program.
const uint8_t* HSM::signMessage(const uint8_t accountId[20], uint8_t secretKey[32], const char* message) {
    uint8_t privateKey[32];

    // handle any previous key state
    const uint8_t* currentPublicKey = previousPublicKey ? previousPublicKey : publicKey;
    const uint8_t* currentEncryptedKey = previousEncryptedKey ? previousEncryptedKey : encryptedKey;

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

    // handle any previous key state
    if (previousPublicKey) {
        erase(previousPublicKey);
        delete [] previousPublicKey;
        erase(previousEncryptedKey);
        delete [] previousEncryptedKey;
    }

    // return the message signature
    return signature;
}


// INVARIANT: No trace of any keys will remain when this function returns.
void HSM::eraseKeys(const uint8_t accountId[20]) {

    // erase all keys in memory
    erase(publicKey);
    erase(encryptedKey);
    erase(previousPublicKey);
    erase(previousEncryptedKey);

    // erase the state of the EEPROM drive
    saveState(accountId, publicKey, encryptedKey);

    // clean up
    delete [] publicKey;
    delete [] encryptedKey;
    delete [] previousPublicKey;
    delete [] previousEncryptedKey;
}

