/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#include <string.h>
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <SHA512.h>
#include <Ed25519.h>
#include <Codex.h>
#include "HSM.h"

#define STATE_DIRECTORY    "/cdt"
#define STATE_FILENAME    "/cdt/state"
using namespace Adafruit_LittleFS_Namespace;


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
    Serial.println("Loading the state of the HSM...");
    InternalFS.begin();
    loadState();
}


HSM::~HSM() {
    Serial.println("Erasing all transient keys from the HSM...");
    erase(publicKey, KEY_SIZE);
    erase(encryptedKey, KEY_SIZE);
    erase(previousPublicKey, KEY_SIZE);
    erase(previousEncryptedKey, KEY_SIZE);
}


void HSM::loadState() {
    if (!InternalFS.exists(STATE_DIRECTORY)) {
        Serial.println("Creating the state directory...");
        InternalFS.mkdir(STATE_DIRECTORY);
    }
    if (InternalFS.exists(STATE_FILENAME)) {
        Serial.println("Reading the state file...");
        File file(STATE_FILENAME, FILE_O_READ, InternalFS);
        file.read(buffer, BUFFER_SIZE);
        file.close();
        Serial.print("STATE: ");
        Serial.println(Codex::encode(buffer, BUFFER_SIZE));
        switch (buffer[0]) {
            case 0:
                Serial.println("No keys have been created.");
                break;
            case 1:
                Serial.println("Loading the current keys...");
                publicKey = new uint8_t[KEY_SIZE];
                memcpy(publicKey, buffer + 1, KEY_SIZE);
                encryptedKey = new uint8_t[KEY_SIZE];
                memcpy(encryptedKey, buffer + 1 + KEY_SIZE, KEY_SIZE);
                break;
            case 2:
                Serial.println("Loading the current keys...");
                publicKey = new uint8_t[KEY_SIZE];
                memcpy(publicKey, buffer + 1, KEY_SIZE);
                encryptedKey = new uint8_t[KEY_SIZE];
                memcpy(encryptedKey, buffer + 1 + KEY_SIZE, KEY_SIZE);
                Serial.println("Loading the previous keys...");
                previousPublicKey = new uint8_t[KEY_SIZE];
                memcpy(previousPublicKey, buffer + 1 + 2 * KEY_SIZE, KEY_SIZE);
                previousEncryptedKey = new uint8_t[KEY_SIZE];
                memcpy(previousEncryptedKey, buffer + 1 + 3 * KEY_SIZE, KEY_SIZE);
                break;
        }
    } else {
        Serial.println("Initializing the state file...");
        memset(buffer, 0x00, BUFFER_SIZE);
        InternalFS.remove(STATE_FILENAME);
        File file(STATE_FILENAME, FILE_O_WRITE, InternalFS);
        file.write(buffer, BUFFER_SIZE);
        file.flush();
        file.close();
    }
}


void HSM::storeState() {
    Serial.println("Writing the state file...");
    memset(buffer, 0x00, BUFFER_SIZE);
    if (publicKey) {
        Serial.println("Saving the current keys...");
        buffer[0]++;
        memcpy(buffer + 1, publicKey, KEY_SIZE);
        memcpy(buffer + 1 + KEY_SIZE, encryptedKey, KEY_SIZE);
    }
    if (previousPublicKey) {
        Serial.println("Saving the previous keys...");
        buffer[0]++;
        memcpy(buffer + 1 + 2 * KEY_SIZE, previousPublicKey, KEY_SIZE);
        memcpy(buffer + 1 + 3 * KEY_SIZE, previousEncryptedKey, KEY_SIZE);
    }
    InternalFS.remove(STATE_FILENAME);
    File file(STATE_FILENAME, FILE_O_WRITE, InternalFS);
    file.write(buffer, BUFFER_SIZE);
    file.flush();
    file.close();
    Serial.print("STATE: ");
    Serial.println(Codex::encode(buffer, BUFFER_SIZE));
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(uint8_t newSecretKey[KEY_SIZE]) {
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous keys
    if (previousPublicKey) {
        Serial.println("A previous key pair already exists, rolling it back to the current key pair...");
        // roll-back the previous regeneration attempt
        memcpy(publicKey, previousPublicKey, KEY_SIZE);
        memcpy(encryptedKey, previousEncryptedKey, KEY_SIZE);
        erase(previousPublicKey, KEY_SIZE);
        erase(previousEncryptedKey, KEY_SIZE);
        storeState();
    }

    // generate a new key pair
    Serial.println("Generating a new key pair...");
    publicKey = new uint8_t[KEY_SIZE];
    encryptedKey = new uint8_t[KEY_SIZE];
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);
    storeState();

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);
    return copy;
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::rotateKeys(uint8_t existingSecretKey[KEY_SIZE], uint8_t newSecretKey[KEY_SIZE]) {
    uint8_t* privateKey = new uint8_t[KEY_SIZE];

    // handle any previous keys
    if (previousPublicKey) {
        Serial.println("A previous key pair already exists, rolling it back to the current key pair...");
        // roll-back the previous regeneration attempt
        memcpy(publicKey, previousPublicKey, KEY_SIZE);
        memcpy(encryptedKey, previousEncryptedKey, KEY_SIZE);
        erase(previousPublicKey, KEY_SIZE);
        erase(previousEncryptedKey, KEY_SIZE);
        storeState();
    }

    // handle existing keys
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

    // generate a new key pair
    Serial.println("Generating a new key pair...");
    publicKey = new uint8_t[KEY_SIZE];
    encryptedKey = new uint8_t[KEY_SIZE];
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);
    storeState();

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);
    return copy;
}


bool HSM::eraseKeys() {
    Serial.println("Erasing the keys...");
    erase(publicKey, KEY_SIZE);
    erase(encryptedKey, KEY_SIZE);
    erase(previousPublicKey, KEY_SIZE);
    erase(previousEncryptedKey, KEY_SIZE);

    Serial.println("Erasing the state file...");
    memset(buffer, 0x00, BUFFER_SIZE);
    InternalFS.remove(STATE_FILENAME);
    File file(STATE_FILENAME, FILE_O_WRITE, InternalFS);
    file.write(buffer, BUFFER_SIZE);
    file.flush();
    file.close();

    return true;
}


// NOTE: The returned digest must be deleted by the calling program.
const uint8_t* HSM::digestBytes(const uint8_t* bytes, const size_t size) {
    SHA512 digester;
    uint8_t* digest = new uint8_t[DIG_SIZE];
    digester.update((const void*) bytes, size);
    digester.finalize(digest, DIG_SIZE);
    return digest;
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

        // erase the previous keys
        erase(previousPublicKey, KEY_SIZE);
        erase(previousEncryptedKey, KEY_SIZE);
        storeState();

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
    const uint8_t aPublicKey[KEY_SIZE],
    const uint8_t signature[SIG_SIZE],
    const uint8_t* bytes,
    const size_t size
) {
    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) bytes, size);
    return isValid;
}

