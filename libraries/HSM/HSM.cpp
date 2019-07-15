/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#include <string.h>
#include <Arduino.h>
#include <EEPROM.h>
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
    Serial.println(F("loading the account Id..."));
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
    Serial.println(F("loading the public key..."));
    for (size_t i = 0; i < KEY_SIZE; i++) {
        publicKey[i] = EEPROM.read(index++);
        if (publicKey[i] != 0x00) keysExist = true;
    }

    if (keysExist) {
        // load the encrypted key
        Serial.println(F("loading the encrypted key..."));
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
        current = EEPROM.read(index);
        // limited EEPROM life-time, only write to EEPROM if necessary
        if (current != accountId[i]) {
            Serial.print(F("writing account Id to EEPROM["));
            Serial.print(index);
            Serial.println(F("]"));
            EEPROM.write(index, accountId[i]);
        }
        index++;
    }
}


/**
 * This function saves the current state of the hardware security module (HSM)
 * to the EEPROM drive.
 */
void saveKeys(const uint8_t publicKey[KEY_SIZE], const uint8_t encryptedKey[KEY_SIZE]) {
    uint8_t current;
    size_t index = 0;

    if (publicKey) {

        // save the public key
        for (size_t i = 0; i < KEY_SIZE; i++) {
            current = EEPROM.read(index);
            // limited EEPROM life-time, only write to EEPROM if necessary
            if (current != publicKey[i]) {
                Serial.print(F("writing public key to EEPROM["));
                Serial.print(index);
                Serial.println(F("]"));
                EEPROM.write(index, publicKey[i]);
            }
            index++;
        }

        // save the encrypted key
        for (size_t i = 0; i < KEY_SIZE; i++) {
            current = EEPROM.read(index);
            // limited EEPROM life-time, only write to EEPROM if necessary
            if (current != encryptedKey[i]) {
                Serial.print(F("writing encrypted key to EEPROM["));
                Serial.print(index);
                Serial.println(F("]"));
                EEPROM.write(index, encryptedKey[i]);
            }
            index++;
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
    Serial.println(F("signing the private key to validate..."));
    uint8_t signature[SIG_SIZE];
    Ed25519::sign(signature, privateKey, publicKey, (const void*) privateKey, KEY_SIZE);
    return !Ed25519::verify(signature, publicKey, (const void*) privateKey, KEY_SIZE);
}


// PUBLIC MEMBER FUNCTIONS

HSM::HSM() {
    // allocate space for state
    Serial.println(F("allocating space for the state..."));
    accountId = new uint8_t[AID_SIZE];
    publicKey = new uint8_t[KEY_SIZE];
    encryptedKey = new uint8_t[KEY_SIZE];

    // load the account Id from persistent memory
    Serial.println(F("loading the account Id from persistent memory..."));
    if (loadAccount(accountId)) {
        Serial.println(F("loading the keys from persistent memory..."));
        if (loadKeys(publicKey, encryptedKey)) {
            Serial.println(F("everything loaded..."));
            return;  // everything loaded
        }
        Serial.println(F("no keys..."));
        erase(publicKey, KEY_SIZE);
        publicKey = 0;
        erase(encryptedKey, KEY_SIZE);
        encryptedKey = 0;
        return;  // account loaded
    }
    Serial.println(F("account does not exist..."));
    erase(accountId, AID_SIZE);
    accountId = 0;
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
    Serial.println(F("erasing transient data..."));
    Serial.println(F("erasing account Id..."));
    erase(accountId, AID_SIZE);
    accountId = 0;
    Serial.println(F("erasing public key..."));
    erase(publicKey, KEY_SIZE);
    publicKey = 0;
    Serial.println(F("erasing encrypted key..."));
    erase(encryptedKey, KEY_SIZE);
    encryptedKey = 0;
    Serial.println(F("erasing previous public key..."));
    erase(previousPublicKey, KEY_SIZE);
    previousPublicKey = 0;
    Serial.println(F("erasing previous encrypted key..."));
    erase(previousEncryptedKey, KEY_SIZE);
    previousEncryptedKey = 0;

    Serial.println(F("erasing persistent data..."));
    // erase persistent data
    for (size_t i = 0; i < EEPROM.length(); i++) {
        uint8_t current = EEPROM.read(i);
        // limited EEPROM life-time, only write to EEPROM if necessary
        if (current != 0) {
            Serial.print(F("writing zero to EEPROM["));
            Serial.print(i);
            Serial.println(F("]"));
            EEPROM.write(i, 0x00);
        }
    }
}


bool HSM::registerAccount(const uint8_t anAccountId[KEY_SIZE]) {
    if (accountId) {
        Serial.println(F("account is already registered..."));
        return false;  // already registered
    }
    accountId = new uint8_t[AID_SIZE];
    memcpy(accountId, anAccountId, AID_SIZE);
    Serial.println(F("saving the account Id in persistent memory..."));
    saveAccount(accountId);
    return true;
}


// NOTE: The returned message digest must be deleted by the calling program.
const uint8_t* HSM::digestMessage(
    const uint8_t anAccountId[AID_SIZE],
    const char* message
) {
    if (invalidAccount(anAccountId, accountId)) {
        Serial.println(F("invalid account Id..."));
        return 0;
    }
    SHA512 digester;
    size_t messageLength = strlen(message);
    uint8_t* digest = new uint8_t[DIG_SIZE];
    Serial.println(F("digesting the message..."));
    digester.update((const void*) message, messageLength);
    digester.finalize(digest, DIG_SIZE);
    return digest;
}


// INVARIANT: newSecretKey and secretKey will have been erased when this function returns.
// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(
    const uint8_t anAccountId[AID_SIZE],
    uint8_t newSecretKey[KEY_SIZE],
    uint8_t secretKey[KEY_SIZE]
) {
    if (invalidAccount(anAccountId, accountId)) {
        Serial.println(F("invalid account Id..."));
        return 0;
    }
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous keys
    if (previousPublicKey) {
        Serial.println(F("rolling back previous keys..."));
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
        Serial.println(F("handling existing keys..."));
        decryptKey(secretKey, encryptedKey, privateKey);

        // validate the private key
        Serial.println(F("validating the private key..."));
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
    Serial.println(F("generating the private key..."));
    Ed25519::generatePrivateKey(privateKey);
    Serial.println(F("generating the public key..."));
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println(F("encrypting and saving the keys..."));
    encryptKey(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);
    privateKey = 0;
    saveKeys(publicKey, encryptedKey);

    // return a copy of the public key
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);
    return copy;
}


// INVARIANT: secretKey will have been erased when this function returns.
// NOTE: The returned signature must be deleted by the calling program.
const uint8_t* HSM::signMessage(
    const uint8_t anAccountId[AID_SIZE],
    uint8_t secretKey[KEY_SIZE],
    const char* message
) {
    if (invalidAccount(anAccountId, accountId)) {
        Serial.println(F("invalid account Id..."));
        return 0;
    }
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous key state
    const uint8_t* currentPublicKey = previousPublicKey ? previousPublicKey : publicKey;
    const uint8_t* currentEncryptedKey = previousPublicKey ? previousEncryptedKey : encryptedKey;

    // decrypt the private key
    Serial.println(F("decrypting the key..."));
    decryptKey(secretKey, currentEncryptedKey, privateKey);

    // validate the private key
    Serial.println(F("validating the key..."));
    if (invalidKeyPair(currentPublicKey, privateKey)) {
        Serial.println(F("the key is invalid..."));
        // clean up and bail
        erase(privateKey, KEY_SIZE);
        privateKey = 0;
        return 0;  // TODO: analyze as possible side channel
    }

    // sign the message using the private key
    Serial.println(F("signing the message..."));
    uint8_t* signature = new uint8_t[SIG_SIZE];
    size_t messageLength = strlen(message);
    Ed25519::sign(signature, privateKey, currentPublicKey, (const void*) message, messageLength);

    // erase the private key
    Serial.println(F("erasing the private key..."));
    erase(privateKey, KEY_SIZE);
    privateKey = 0;

    // handle any previous key state
    Serial.println(F("handling any previous keys..."));
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
        Serial.println(F("invalid account Id..."));
        return false;
    }
    Serial.println(F("verifying the signature..."));
    size_t messageLength = strlen(message);
    aPublicKey = aPublicKey ? aPublicKey : publicKey;  // default to the HSM public key
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) message, messageLength);
    return isValid;
}


// INVARIANT: No trace of any keys will remain when this function returns.
bool HSM::eraseKeys(const uint8_t anAccountId[AID_SIZE]) {
    if (invalidAccount(anAccountId, accountId)) {
        Serial.println(F("invalid account Id..."));
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
    Serial.println(F("erasing the state of the keys from persistent memory..."));
    saveAccount(accountId);
    saveKeys(publicKey, encryptedKey);

    return true;
}

