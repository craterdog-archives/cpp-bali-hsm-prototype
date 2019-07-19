/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#include <string.h>
#include <Arduino.h>
#include "EEPROM.h"  // change to <EEPROM.h> for real implemnetation
#include "SHA512.h"  // change to <SHA512.h> for real implementation
#include "Ed25519.h"  // change to <Ed25519.h> for real implementation
#include "HSM.h"


// PRIVATE FREE FUNCTIONS

/**
 * This function loads the saved account Id from the EEPROM drive.
 */
bool loadAccount(uint8_t accountId[AID_SIZE]) {
    size_t index = 0;
    bool accountExists = false;

    // load the account Id
    for (size_t i = 0; i < AID_SIZE; i++) {
        accountId[i] = EEPROM.read(index++);
        if (accountId[i] != 0x00) accountExists = true;
    }

    return accountExists;
}


/**
 * This function loads the saved keys from the EEPROM drive.
 */
bool loadKeys(uint8_t publicKey[KEY_SIZE], uint8_t encryptedKey[KEY_SIZE]) {
    size_t index = AID_SIZE;
    bool keysExist = false;

    // load the public key
    for (size_t i = 0; i < KEY_SIZE; i++) {
        publicKey[i] = EEPROM.read(index++);
        if (publicKey[i] != 0x00) keysExist = true;
    }

    if (keysExist) {
        // load the encrypted key
        for (size_t i = 0; i < KEY_SIZE; i++) {
            encryptedKey[i] = EEPROM.read(index++);
        }
    }

    return keysExist;
}


/**
 * This function saves the account Id to the EEPROM drive.
 */
void saveAccount(const uint8_t accountId[AID_SIZE]) {
    uint8_t current;
    size_t index = 0;

    // save the account Id
    for (size_t i = 0; i < AID_SIZE; i++) {
        EEPROM.update(index++, accountId[i]);
    }
}


/**
 * This function saves the current state of the hardware security module (HSM)
 * to the EEPROM drive.
 */
void saveKeys(const uint8_t publicKey[KEY_SIZE], const uint8_t encryptedKey[KEY_SIZE]) {
    uint8_t current;
    size_t index = AID_SIZE;

    if (publicKey) {

        // save the public key
        for (size_t i = 0; i < KEY_SIZE; i++) {
            EEPROM.update(index++, publicKey[i]);
        }

        // save the encrypted key
        for (size_t i = 0; i < KEY_SIZE; i++) {
            EEPROM.update(index++, encryptedKey[i]);
        }
    }
}


/**
 * This function performs the exclusive or (XOR) operation on each byte in the
 * first two data arrays and places the resulting byte in the third data array.
 *   c = a xor b
 */
void XOR(
    const uint8_t a[KEY_SIZE],
    const uint8_t b[KEY_SIZE],
    uint8_t c[KEY_SIZE]
) {
    for (size_t i = 0; i < KEY_SIZE; i++) {
        c[i] = a[i] xor b[i];
    }
}


/**
 * This function erases the specified data array and deletes its allocated
 * memory.
 */
void erase(uint8_t* data, size_t size) {
    if (data) {
        memset(data, 0x00, size);
        delete [] data;
    }
}


/**
 * This function uses a simple one-time-pad algorithm (XOR) to encrypt a private
 * key using a secret key and then erases both the secret key and private key.
 */
void encryptKey(
    const uint8_t secretKey[KEY_SIZE],
    const uint8_t privateKey[KEY_SIZE],
    uint8_t encryptedKey[KEY_SIZE]
) {
    XOR(secretKey, privateKey, encryptedKey);
}


/**
 * This function decrypts a private key that was encrypted using a secret key and
 * a simple one-time-pad algorithm (XOR) and then erases the secret key.
 */
void decryptKey(
    const uint8_t secretKey[KEY_SIZE],
    const uint8_t encryptedKey[KEY_SIZE],
    uint8_t privateKey[KEY_SIZE]
) {
    XOR(secretKey, encryptedKey, privateKey);
}


/**
 * This function returns whether or not the specified account Id is invalid.
 */
bool invalidAccount(
    const uint8_t anAccountId[AID_SIZE],
    const uint8_t expectedAccountId[AID_SIZE]
) {
    if (expectedAccountId == 0) return true;
    return (memcmp(anAccountId, expectedAccountId, AID_SIZE) != 0);
}


/**
 * This function returns whether or not the specified public-private key pair is
 * invalid.
 */
bool invalidKeyPair(
    const uint8_t publicKey[KEY_SIZE],
    const uint8_t privateKey[KEY_SIZE]
) {
    uint8_t signature[SIG_SIZE];
    Ed25519::sign(signature, privateKey, publicKey, (const void*) privateKey, KEY_SIZE);
    return !Ed25519::verify(signature, publicKey, (const void*) privateKey, KEY_SIZE);
}


// PUBLIC MEMBER FUNCTIONS

HSM::HSM() {
    // allocate space for state
    accountId = new uint8_t[AID_SIZE];
    publicKey = new uint8_t[KEY_SIZE];
    encryptedKey = new uint8_t[KEY_SIZE];
    previousPublicKey = 0;
    previousEncryptedKey = 0;

    // load the account Id from persistent memory
    if (loadAccount(accountId)) {
        if (loadKeys(publicKey, encryptedKey)) {
            return;  // everything loaded
        }
        erase(publicKey, KEY_SIZE);
        publicKey = 0;
        erase(encryptedKey, KEY_SIZE);
        encryptedKey = 0;
        return;  // account loaded
    }
    erase(accountId, AID_SIZE);
    accountId = 0;
    erase(publicKey, KEY_SIZE);
    publicKey = 0;
    erase(encryptedKey, KEY_SIZE);
    encryptedKey = 0;
}


HSM::~HSM() {
    erase(accountId, AID_SIZE);
    accountId = 0;
    erase(publicKey, KEY_SIZE);
    publicKey = 0;
    erase(encryptedKey, KEY_SIZE);
    encryptedKey = 0;
    erase(previousPublicKey, KEY_SIZE);
    previousPublicKey = 0;
    erase(previousEncryptedKey, KEY_SIZE);
    previousEncryptedKey = 0;
}


void HSM::resetHSM() {

    // erase transient data
    erase(accountId, AID_SIZE);
    accountId = 0;
    erase(publicKey, KEY_SIZE);
    publicKey = 0;
    erase(encryptedKey, KEY_SIZE);
    encryptedKey = 0;
    erase(previousPublicKey, KEY_SIZE);
    previousPublicKey = 0;
    erase(previousEncryptedKey, KEY_SIZE);
    previousEncryptedKey = 0;

    // erase persistent data
    for (size_t i = 0; i < EEPROM.length(); i++) {
        EEPROM.update(i, 0x00);
    }
}


bool HSM::registerAccount(const uint8_t anAccountId[AID_SIZE]) {
    if (accountId) {
        return false;  // already registered
    }
    accountId = new uint8_t[AID_SIZE];
    memcpy(accountId, anAccountId, AID_SIZE);
    saveAccount(accountId);
    return true;
}


// NOTE: The returned message digest must be deleted by the calling program.
const uint8_t* HSM::digestMessage(
    const uint8_t anAccountId[AID_SIZE],
    const char* message
) {
    if (invalidAccount(anAccountId, accountId)) {
        return 0;
    }
    SHA512 digester;
    size_t messageLength = strlen(message);
    uint8_t* digest = new uint8_t[DIG_SIZE];
    digester.update((const void*) message, messageLength);
    digester.finalize(digest, DIG_SIZE);
    return digest;
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(
    const uint8_t anAccountId[AID_SIZE],
    uint8_t newSecretKey[KEY_SIZE],
    uint8_t secretKey[KEY_SIZE]
) {
    if (invalidAccount(anAccountId, accountId)) {
        return 0;
    }
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous keys
    if (previousPublicKey) {
        // roll-back the previous regeneration attempt
        memcpy(publicKey, previousPublicKey, KEY_SIZE);
        memcpy(encryptedKey, previousEncryptedKey, KEY_SIZE);
        saveKeys(publicKey, encryptedKey);
        erase(previousPublicKey, KEY_SIZE);
        previousPublicKey = 0;
        erase(previousEncryptedKey, KEY_SIZE);
        previousEncryptedKey = 0;
    }

    // handle existing keys
    if (secretKey) {
        decryptKey(secretKey, encryptedKey, privateKey);

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            // clean up and bail
            erase(privateKey, KEY_SIZE);
            privateKey = 0;
            return 0;  // TODO: analyze as possible side channel
        }

        // save copies of the previous public and encrypted keys
        previousPublicKey = new uint8_t[KEY_SIZE];
        memcpy(previousPublicKey, publicKey, KEY_SIZE);
        previousEncryptedKey = new uint8_t[KEY_SIZE];
        memcpy(previousEncryptedKey, encryptedKey, KEY_SIZE);
    }

    // generate a new key pair
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    encryptKey(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);
    privateKey = 0;
    saveKeys(publicKey, encryptedKey);

    // return a copy of the public key
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);
    return copy;
}


// NOTE: The returned signature must be deleted by the calling program.
const uint8_t* HSM::signMessage(
    const uint8_t anAccountId[AID_SIZE],
    uint8_t secretKey[KEY_SIZE],
    const char* message
) {
    if (invalidAccount(anAccountId, accountId)) {
        return 0;
    }
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous key state
    const uint8_t* currentPublicKey = previousPublicKey ? previousPublicKey : publicKey;
    const uint8_t* currentEncryptedKey = previousPublicKey ? previousEncryptedKey : encryptedKey;

    // decrypt the private key
    decryptKey(secretKey, currentEncryptedKey, privateKey);

    // validate the private key
    if (invalidKeyPair(currentPublicKey, privateKey)) {
        // clean up and bail
        erase(privateKey, KEY_SIZE);
        privateKey = 0;
        return 0;  // TODO: analyze as possible side channel
    }

    // sign the message using the private key
    uint8_t* signature = new uint8_t[SIG_SIZE];
    size_t messageLength = strlen(message);
    Ed25519::sign(signature, privateKey, currentPublicKey, (const void*) message, messageLength);

    // erase the private key
    erase(privateKey, KEY_SIZE);
    privateKey = 0;

    // handle any previous key state
    if (previousPublicKey) {
        erase(previousPublicKey, KEY_SIZE);
        previousPublicKey = 0;
        erase(previousEncryptedKey, KEY_SIZE);
        previousEncryptedKey = 0;
    }

    // return the message signature
    return signature;
}


// NOTE: the specified public key need not be the same public key that is associated
// with the hardware security module (HSM). It should be the key associated with the
// private key that supposedly signed the message.
bool HSM::validSignature(
    const uint8_t anAccountId[AID_SIZE],
    const char* message,
    const uint8_t signature[SIG_SIZE],
    const uint8_t aPublicKey[KEY_SIZE]
) {
    if (invalidAccount(anAccountId, accountId)) {
        return false;
    }
    size_t messageLength = strlen(message);
    aPublicKey = aPublicKey ? aPublicKey : publicKey;  // default to the HSM public key
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) message, messageLength);
    return isValid;
}


bool HSM::eraseKeys(const uint8_t anAccountId[AID_SIZE]) {
    if (invalidAccount(anAccountId, accountId)) {
        return false;
    }

    // erase all keys in memory
    erase(publicKey, KEY_SIZE);
    publicKey = 0;
    erase(encryptedKey, KEY_SIZE);
    encryptedKey = 0;
    erase(previousPublicKey, KEY_SIZE);
    previousPublicKey = 0;
    erase(previousEncryptedKey, KEY_SIZE);
    previousEncryptedKey = 0;

    // erase the state of the EEPROM drive
    saveAccount(accountId);
    saveKeys(publicKey, encryptedKey);

    return true;
}

