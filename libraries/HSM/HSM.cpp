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
void loadState(
    uint8_t accountId[AID_SIZE],
    uint8_t publicKey[KEY_SIZE],
    uint8_t encryptedKey[KEY_SIZE]
) {
    size_t index = 0;

    // load the account Id
    for (size_t i = 0; i < AID_SIZE; i++) {
        accountId[i] = EEPROM.read(index++);
    }

    // load the public key
    for (size_t i = 0; i < KEY_SIZE; i++) {
        publicKey[i] = EEPROM.read(index++);
    }

    // load the encrypted key
    for (size_t i = 0; i < KEY_SIZE; i++) {
        encryptedKey[i] = EEPROM.read(index++);
    }
}


/**
 * This function saves the current state of the hardware security module (HSM)
 * to the EEPROM drive.
 */
void saveState(
    const uint8_t accountId[AID_SIZE],
    const uint8_t publicKey[KEY_SIZE] = 0,
    const uint8_t encryptedKey[KEY_SIZE] = 0
) {
    size_t index = 0;
    if (accountId) {

        // save the account Id
        for (size_t i = 0; i < AID_SIZE; i++) {
            EEPROM.write(index++, accountId[i]);
        }

        if (publicKey) {

            // save the public key
            for (size_t i = 0; i < KEY_SIZE; i++) {
                EEPROM.write(index++, publicKey[i]);
            }

            // save the encrypted key
            for (size_t i = 0; i < KEY_SIZE; i++) {
                EEPROM.write(index++, encryptedKey[i]);
            }
        }
    }
}


/**
 * This function erases each byte in the specified data array and deletes the
 * array.
 */
void erase(uint8_t* data, size_t size = KEY_SIZE) {
    memset(data, 0x00, size);
    delete [] data;
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
 * This function uses a simple one-time-pad algorithm (XOR) to encrypt a private
 * key using a secret key and then erases both the secret key and private key.
 */
void encryptKey(
    uint8_t secretKey[KEY_SIZE],
    uint8_t privateKey[KEY_SIZE],
    uint8_t encryptedKey[KEY_SIZE]
) {
    XOR(secretKey, privateKey, encryptedKey);
    memset(secretKey, 0x00, KEY_SIZE);
    memset(privateKey, 0x00, KEY_SIZE);
}


/**
 * This function decrypts a private key that was encrypted using a secret key and
 * a simple one-time-pad algorithm (XOR) and then erases the secret key.
 */
void decryptKey(
    uint8_t secretKey[KEY_SIZE],
    const uint8_t encryptedKey[KEY_SIZE],
    uint8_t privateKey[KEY_SIZE]
) {
    XOR(secretKey, encryptedKey, privateKey);
    memset(secretKey, 0x00, KEY_SIZE);
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


// PUBLIC METHODS

HSM::HSM() {
    // allocate space for state
    accountId = new uint8_t[AID_SIZE];
    publicKey = new uint8_t[KEY_SIZE];
    encryptedKey = new uint8_t[KEY_SIZE];

    // load the state from persistent memory
    loadState(accountId, publicKey, encryptedKey);

    // check to see if an accountId exists
    for (size_t i = 0; i < AID_SIZE; i++) {
        if (accountId[i] != 0x00) return;  // accountId exists
    }

    // if no accountId, delete the state
    erase(accountId, AID_SIZE);
    erase(publicKey, KEY_SIZE);
    erase(encryptedKey, KEY_SIZE);
}


HSM::~HSM() {
    if (accountId) {
        erase(accountId, AID_SIZE);
    }
    if (publicKey) {
        erase(publicKey);
        erase(encryptedKey, KEY_SIZE);
    }
    if (previousPublicKey) {
        erase(previousPublicKey, KEY_SIZE);
        erase(previousEncryptedKey, KEY_SIZE);
    }
}


bool HSM::registerAccount(const uint8_t anAccountId[KEY_SIZE]) {
    if (accountId) return false;  // already registered
    accountId = new uint8_t[AID_SIZE];
    memcpy(accountId, anAccountId, AID_SIZE);
    saveState(accountId);
    return true;
}


// NOTE: The returned message digest must be deleted by the calling program.
const uint8_t* HSM::digestMessage(
    const uint8_t anAccountId[AID_SIZE],
    const char* message
) {
    if (invalidAccount(anAccountId, accountId)) return 0;
    SHA512 digester;
    size_t messageLength = strlen(message);
    uint8_t* digest = new uint8_t[DIG_SIZE];
    digester.update((const void*) message, messageLength);
    digester.finalize(digest, DIG_SIZE);
    return digest;
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
    if (invalidAccount(anAccountId, accountId)) return false;
    size_t messageLength = strlen(message);
    aPublicKey = aPublicKey ? aPublicKey : publicKey;  // default to the HSM public key
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) message, messageLength);
    return isValid;
}


// INVARIANT: newSecretKey and secretKey will have been erased when this function returns.
// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(
    const uint8_t anAccountId[AID_SIZE],
    uint8_t newSecretKey[KEY_SIZE],
    uint8_t secretKey[KEY_SIZE]
) {
    if (invalidAccount(anAccountId, accountId)) return 0;
    uint8_t privateKey[KEY_SIZE];

    // handle any previous keys
    if (previousPublicKey) {
        // roll-back the previous regeneration attempt
        memcpy(publicKey, previousPublicKey, KEY_SIZE);
        memcpy(encryptedKey, previousEncryptedKey, KEY_SIZE);
        saveState(accountId, publicKey, encryptedKey);
        erase(previousPublicKey, KEY_SIZE);
        previousPublicKey = 0;
        erase(previousEncryptedKey, KEY_SIZE);
        previousEncryptedKey = 0;
    }

    // handle existing keys
    if (secretKey) {
        decryptKey(secretKey, encryptedKey, privateKey);  // erases secretKey

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            // clean up and bail
            memset(newSecretKey, 0x00, KEY_SIZE);
            memset(privateKey, 0x00, KEY_SIZE);
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
    encryptKey(newSecretKey, privateKey, encryptedKey);  // erases newSecretKey and privateKey
    saveState(accountId, publicKey, encryptedKey);

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
    if (invalidAccount(anAccountId, accountId)) return 0;
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous key state
    const uint8_t* currentPublicKey = previousPublicKey ? previousPublicKey : publicKey;
    const uint8_t* currentEncryptedKey = previousPublicKey ? previousEncryptedKey : encryptedKey;

    // decrypt the private key
    decryptKey(secretKey, currentEncryptedKey, privateKey);  // erases secretKey

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


// INVARIANT: No trace of any keys will remain when this function returns.
void HSM::eraseKeys(const uint8_t anAccountId[AID_SIZE]) {
    if (invalidAccount(anAccountId, accountId)) return;

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
    saveState(accountId, publicKey, encryptedKey);
}

