/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#include <string.h>
#include <Arduino.h>
#include <SHA512.h>
#include <Ed25519.h>
#include <Codex.h>
#include "HSM.h"


// PRIVATE FREE FUNCTIONS

/*
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


/*
 * This function erases the specified data array and deletes its allocated
 * memory.
 * NOTE: the data argument is passed by reference so that it can be reset
 * to zero.
 */
void erase(uint8_t* &data, const size_t size) {
    if (data) {
        memset(data, 0x00, size);
        delete [] data;
        data = 0;
    }
}


/*
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
}


HSM::~HSM() {
    eraseKeys();
}


// NOTE: The returned digest must be deleted by the calling program.
const uint8_t* HSM::digestBytes(const uint8_t* bytes, const size_t size) {
    SHA512 digester;
    uint8_t* digest = new uint8_t[DIG_SIZE];
    digester.update((const void*) bytes, size);
    digester.finalize(digest, DIG_SIZE);
    return digest;
}


// There are three possible states that the HSM can be in when this is
// called:
//  1) No keys yet exist, it is being called for the first time.
//  2) A key pair exists and it is being called to rotate the keys.
//  3) The previous key pair is stored and for some reason the subsequent
//     call to signBytes() never occurred so we must roll back the keys.
// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(uint8_t newSecretKey[KEY_SIZE], uint8_t existingSecretKey[KEY_SIZE]) {
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous keys (case 3 above)
    if (previousPublicKey) {
        Serial.println("A previous key pair already exists, rolling it back to the current key pair...");
        // roll-back the previous regeneration attempt
        memcpy(publicKey, previousPublicKey, KEY_SIZE);
        memcpy(encryptedKey, previousEncryptedKey, KEY_SIZE);
        erase(previousPublicKey, KEY_SIZE);
        erase(previousEncryptedKey, KEY_SIZE);
        // now we are back to case 2 above
    }

    // handle existing keys (case 2 above)
    if (existingSecretKey) {
        Serial.println("Extracting the existing private key...");
        XOR(existingSecretKey, encryptedKey, privateKey);

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            Serial.println("An invalid secret key was passed by the mobile device.");
            // clean up and bail
            erase(privateKey, KEY_SIZE);
            return 0;  // TODO: analyze as possible side channel
        }

        // save copies of the previous public and encrypted keys
        Serial.println("Saving the previous key pair...");
        previousPublicKey = new uint8_t[KEY_SIZE];
        memcpy(previousPublicKey, publicKey, KEY_SIZE);
        erase(publicKey, KEY_SIZE);
        previousEncryptedKey = new uint8_t[KEY_SIZE];
        memcpy(previousEncryptedKey, encryptedKey, KEY_SIZE);
        erase(encryptedKey, KEY_SIZE);
        // now we are in case 1 above
    }

    // generate a new key pair (case 1 above)
    Serial.println("Generating a new key pair...");
    publicKey = new uint8_t[KEY_SIZE];
    encryptedKey = new uint8_t[KEY_SIZE];
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);
    return copy;
}


// NOTE: The returned digital signature must be deleted by the calling program.
const uint8_t* HSM::signBytes(uint8_t secretKey[KEY_SIZE], const uint8_t* bytes, const size_t size) {
    if (publicKey == 0) {
        Serial.println("No key has been generated yet.");
        return 0;
    }
    uint8_t* privateKey = new uint8_t[KEY_SIZE];
    uint8_t* signature = new uint8_t[SIG_SIZE];

    if (previousPublicKey) {

        // decrypt the private key
        XOR(secretKey, previousEncryptedKey, privateKey);

        // validate the private key
        if (invalidKeyPair(previousPublicKey, privateKey)) {
            Serial.println("An invalid previous secret key was passed by the mobile device.");
            erase(privateKey, KEY_SIZE);
            return 0;  // TODO: analyze as possible side channel
        }

        // sign the bytes using the private key
        Serial.println("Signing using the previous private key...");
        Ed25519::sign(signature, privateKey, previousPublicKey, (const void*) bytes, size);

        // erase the private key
        erase(privateKey, KEY_SIZE);

        erase(previousPublicKey, KEY_SIZE);
        erase(previousEncryptedKey, KEY_SIZE);

    } else {

        // decrypt the private key
        XOR(secretKey, encryptedKey, privateKey);

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            Serial.println("An invalid secret key was passed by the mobile device.");
            erase(privateKey, KEY_SIZE);
            return 0;  // TODO: analyze as possible side channel
        }

        // sign the bytes using the private key
        Serial.println("Signing using the current private key...");
        Ed25519::sign(signature, privateKey, publicKey, (const void*) bytes, size);

        // erase the private key
        erase(privateKey, KEY_SIZE);

    }

    // return the digital signature
    return signature;
}


// NOTE: the specified public key need not be the same public key that is associated
// with the hardware security module (HSM). It should be the key associated with the
// private key that supposedly signed the bytes.
bool HSM::validSignature(
    const uint8_t* bytes,
    const size_t size,
    const uint8_t signature[SIG_SIZE],
    const uint8_t aPublicKey[KEY_SIZE]
) {
    if (aPublicKey == 0) {
        Serial.println("Validating the signature using the local public key...");
        aPublicKey = publicKey;
    }
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) bytes, size);
    return isValid;
}


bool HSM::eraseKeys() {
    erase(publicKey, KEY_SIZE);
    erase(encryptedKey, KEY_SIZE);
    erase(previousPublicKey, KEY_SIZE);
    erase(previousEncryptedKey, KEY_SIZE);
    return true;
}

