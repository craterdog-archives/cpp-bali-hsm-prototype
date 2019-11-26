/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This source code is for reference purposes only.  It is protected by *
 * US Patent 9,853,813 and any use of this source code will be deemed   *
 * an infringement of the patent.  Crater Dog Technologies(TM) retains  *
 * full ownership of this source code.  If you are interested in        *
 * experimenting with, or licensing the technology, please contact us   *
 * at craterdog@gmail.com                                               *
 ************************************************************************/
#include <string.h>
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <SHA512.h>
#include <Ed25519.h>
#include "HSM.h"
#include "Codex.h"

#define STATE_DIRECTORY    "/cdt"
#define STATE_FILENAME    "/cdt/state"
using namespace Adafruit_LittleFS_Namespace;


// CONSTANTS

const int LED = 17;  // pin number of the LED
const int BUTTON = 5;  // pin number of the push button (other pin is ground)
const int WAIT_MILLISECONDS = 50;
const int MAX_WAIT_MILLISECONDS = 5 /*seconds*/ * 1000;

// STATE MACHINE

const State nextState[4][7] = {
   // LoadBlock   GenerateKeys   RotateKeys   EraseKeys  DigestBytes  SignBytes  ValidSignature
    { Invalid,     Invalid,     Invalid,     Invalid,    Invalid,    Invalid,    Invalid },    // Invalid
    { Invalid,     OneKeyPair,  Invalid,     NoKeyPairs, NoKeyPairs, Invalid,    NoKeyPairs }, // NoKeyPairs
    { Invalid,     Invalid,     TwoKeyPairs, NoKeyPairs, OneKeyPair, OneKeyPair, OneKeyPair }, // OneKeyPair
    { Invalid,     Invalid,     Invalid,     NoKeyPairs, Invalid,    OneKeyPair, Invalid }     // TwoKeyPairs
};


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
 * This function erases the specified data array and deletes its allocated memory.
 * NOTE: the data argument is passed by reference so that it can be reset to zero.
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
 * Invalid.
 */
bool InvalidKeyPair(
    const uint8_t publicKey[KEY_SIZE],
    const uint8_t privateKey[KEY_SIZE]
) {
    uint8_t signature[SIG_SIZE];
    Ed25519::sign(signature, privateKey, publicKey, (const void*) privateKey, KEY_SIZE);
    return !Ed25519::verify(signature, publicKey, (const void*) privateKey, KEY_SIZE);
}


bool rejected() {
    int milliseconds = 0;
    while (milliseconds < MAX_WAIT_MILLISECONDS) {
        delay(WAIT_MILLISECONDS);
        milliseconds += WAIT_MILLISECONDS;
        int buttonState = digitalRead(BUTTON);
        if (buttonState == LOW) {
            return false;  // approved
        }
    }
    return true;  // rejected
}


// PUBLIC MEMBER FUNCTIONS

HSM::HSM() {
    Serial.println("Checking for a button...");
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    pinMode(BUTTON, INPUT_PULLUP);
    if (rejected()) {
        Serial.println("The button is disabled.");
        pinMode(BUTTON, OUTPUT);
        this->hasButton = false;
    } else {
        Serial.println("The button is enabled.");
        this->hasButton = true;
    }
    Serial.println("Loading the state of the HSM...");
    InternalFS.begin();
    loadState();
    digitalWrite(LED, LOW);
}


HSM::~HSM() {
    Serial.println("Erasing all transient keys from the HSM...");
    erase(publicKey, KEY_SIZE);
    erase(wearableKey, KEY_SIZE);
    erase(previousPublicKey, KEY_SIZE);
    erase(previousWearableKey, KEY_SIZE);
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(uint8_t newMobileKey[KEY_SIZE]) {
    // validate request type
    if (validRequest(GenerateKeys)) {
        Serial.print("The HSM is in an Invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    // get user approval (if button enabled)
    digitalWrite(LED, HIGH);
    if (hasButton && rejected()) {
        Serial.println("The request was rejected by the user.");
        digitalWrite(LED, LOW);
        return 0;
    }

    // generate a new key pair
    Serial.println("Generating a new key pair...");
    publicKey = new uint8_t[KEY_SIZE];
    wearableKey = new uint8_t[KEY_SIZE];
    uint8_t* privateKey = new uint8_t[KEY_SIZE];
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newMobileKey, privateKey, wearableKey);
    erase(privateKey, KEY_SIZE);

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);

    // update current state
    transitionState(GenerateKeys);
    storeState();
    digitalWrite(LED, LOW);

    return copy;
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::rotateKeys(uint8_t existingMobileKey[KEY_SIZE], uint8_t newMobileKey[KEY_SIZE]) {
    // validate request type
    if (validRequest(RotateKeys)) {
        Serial.print("The HSM is in an Invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    // get user approval (if button enabled)
    digitalWrite(LED, HIGH);
    if (hasButton && rejected()) {
        Serial.println("The request was rejected by the user.");
        digitalWrite(LED, LOW);
        return 0;
    }

    // handle existing keys
    Serial.println("Extracting the existing private key...");
    uint8_t* privateKey = new uint8_t[KEY_SIZE];
    XOR(existingMobileKey, wearableKey, privateKey);

    // validate the private key
    if (InvalidKeyPair(publicKey, privateKey)) {
        Serial.println("An Invalid existing mobile key was passed by the mobile device.");
        // clean up and bail
        erase(privateKey, KEY_SIZE);
        digitalWrite(LED, LOW);
        return 0;
    }

    // save copies of the previous public and wearable keys
    Serial.println("Saving the previous key pair...");
    previousPublicKey = new uint8_t[KEY_SIZE];
    memcpy(previousPublicKey, publicKey, KEY_SIZE);
    previousWearableKey = new uint8_t[KEY_SIZE];
    memcpy(previousWearableKey, wearableKey, KEY_SIZE);

    // generate a new key pair
    Serial.println("Generating a new key pair...");
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newMobileKey, privateKey, wearableKey);
    erase(privateKey, KEY_SIZE);

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);

    // update current state
    transitionState(RotateKeys);
    storeState();
    digitalWrite(LED, LOW);

    return copy;
}


bool HSM::eraseKeys() {
    // validate request type
    if (validRequest(EraseKeys)) {
        Serial.print("The HSM is in an Invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    Serial.println("Erasing the keys...");
    erase(publicKey, KEY_SIZE);
    erase(wearableKey, KEY_SIZE);
    erase(previousPublicKey, KEY_SIZE);
    erase(previousWearableKey, KEY_SIZE);

    // update current state
    transitionState(EraseKeys);
    storeState();

    return true;
}


// NOTE: The returned digest must be deleted by the calling program.
const uint8_t* HSM::digestBytes(const uint8_t* bytes, const size_t size) {
    // validate request type
    if (validRequest(DigestBytes)) {
        Serial.print("The HSM is in an Invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    // generate the digital digest
    SHA512 digester;
    uint8_t* digest = new uint8_t[DIG_SIZE];
    digester.update((const void*) bytes, size);
    digester.finalize(digest, DIG_SIZE);

    // update current state
    transitionState(DigestBytes);
    storeState();

    return digest;
}


// NOTE: The returned digital signature must be deleted by the calling program.
const uint8_t* HSM::signBytes(uint8_t mobileKey[KEY_SIZE], const uint8_t* bytes, const size_t size) {
    // validate request type
    if (validRequest(SignBytes)) {
        Serial.print("The HSM is in an Invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    // get user approval (if button enabled)
    digitalWrite(LED, HIGH);
    if (hasButton && rejected()) {
        Serial.println("The request was rejected by the user.");
        digitalWrite(LED, LOW);
        return 0;
    }

    uint8_t* privateKey = new uint8_t[KEY_SIZE];
    uint8_t* signature = new uint8_t[SIG_SIZE];
    if (previousPublicKey) {

        // decrypt the private key
        XOR(mobileKey, previousWearableKey, privateKey);

        // validate the private key
        if (InvalidKeyPair(previousPublicKey, privateKey)) {
            Serial.println("An Invalid previous mobile key was passed by the mobile device.");
            erase(privateKey, KEY_SIZE);
            digitalWrite(LED, LOW);
            return 0;
        }

        // sign the bytes using the private key
        Serial.println("Signing using the previous private key...");
        Ed25519::sign(signature, privateKey, previousPublicKey, (const void*) bytes, size);

        // erase the private key
        erase(privateKey, KEY_SIZE);

        // erase the previous keys
        erase(previousPublicKey, KEY_SIZE);
        erase(previousWearableKey, KEY_SIZE);

    } else {

        // decrypt the private key
        XOR(mobileKey, wearableKey, privateKey);

        // validate the private key
        if (InvalidKeyPair(publicKey, privateKey)) {
            Serial.println("An Invalid mobile key was passed by the mobile device.");
            erase(privateKey, KEY_SIZE);
            digitalWrite(LED, LOW);
            return 0;
        }

        // sign the bytes using the private key
        Serial.println("Signing using the current private key...");
        Ed25519::sign(signature, privateKey, publicKey, (const void*) bytes, size);

        // erase the private key
        erase(privateKey, KEY_SIZE);

    }

    // update current state
    transitionState(SignBytes);
    storeState();
    digitalWrite(LED, LOW);

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
    // validate request type
    if (validRequest(ValidSignature)) {
        Serial.print("The HSM is in an Invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) bytes, size);

    // update current state
    transitionState(ValidSignature);
    storeState();

    return isValid;
}


bool HSM::validRequest(RequestType request) {
    return nextState[currentState][request] == Invalid;
}


void HSM::transitionState(RequestType request) {
    currentState = nextState[currentState][request];
}


void HSM::loadState() {
    if (!InternalFS.exists(STATE_DIRECTORY)) {
        Serial.println("Creating the state directory...");
        InternalFS.mkdir(STATE_DIRECTORY);
    }
    if (!InternalFS.exists(STATE_FILENAME)) {
        Serial.println("Initializing the state file...");
        memset(buffer, 0x00, BUFFER_SIZE);
        File file(InternalFS);
        file.open(STATE_FILENAME, FILE_O_WRITE);
        file.write(buffer, BUFFER_SIZE);
        file.flush();
        file.close();
    } else {
        Serial.println("Reading the state file...");
        File file(InternalFS);
        file.open(STATE_FILENAME, FILE_O_READ);
        file.read(buffer, BUFFER_SIZE);
        file.close();
    }
    currentState = (State) (buffer[0] + 1);
    Serial.print("The current state is: ");
    Serial.println(currentState);
    if (currentState > 1) {
        Serial.println("Loading the current keys...");
        publicKey = new uint8_t[KEY_SIZE];
        memcpy(publicKey, buffer + 1, KEY_SIZE);
        wearableKey = new uint8_t[KEY_SIZE];
        memcpy(wearableKey, buffer + 1 + KEY_SIZE, KEY_SIZE);
    }
    if (currentState > 2) {
        Serial.println("Loading the previous keys...");
        previousPublicKey = new uint8_t[KEY_SIZE];
        memcpy(previousPublicKey, buffer + 1 + 2 * KEY_SIZE, KEY_SIZE);
        previousWearableKey = new uint8_t[KEY_SIZE];
        memcpy(previousWearableKey, buffer + 1 + 3 * KEY_SIZE, KEY_SIZE);
    }
}


void HSM::storeState() {
    Serial.println("Writing the state file...");
    memset(buffer, 0x00, BUFFER_SIZE);
    if (publicKey) {
        Serial.println("Saving the current keys...");
        buffer[0]++;
        memcpy(buffer + 1, publicKey, KEY_SIZE);
        memcpy(buffer + 1 + KEY_SIZE, wearableKey, KEY_SIZE);
    }
    if (previousPublicKey) {
        Serial.println("Saving the previous keys...");
        buffer[0]++;
        memcpy(buffer + 1 + 2 * KEY_SIZE, previousPublicKey, KEY_SIZE);
        memcpy(buffer + 1 + 3 * KEY_SIZE, previousWearableKey, KEY_SIZE);
    }
    InternalFS.remove(STATE_FILENAME);
    File file(InternalFS);
    file.open(STATE_FILENAME, FILE_O_WRITE);
    file.write(buffer, BUFFER_SIZE);
    file.flush();
    file.close();
    Serial.println("Done.");
}

