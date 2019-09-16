/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
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

const State nextState[4][7] = {
   // invalid     generateKeys   rotateKeys   eraseKeys  digestBytes  signBytes  validBytes
    { invalid,     invalid,     invalid,     invalid,    invalid,    invalid,    invalid },    // invalid
    { invalid,     oneKeyPair,  invalid,     noKeyPairs, noKeyPairs, invalid,    noKeyPairs }, // noKeyPairs
    { invalid,     invalid,     twoKeyPairs, noKeyPairs, oneKeyPair, oneKeyPair, oneKeyPair }, // oneKeyPair
    { invalid,     invalid,     invalid,     noKeyPairs, invalid,    oneKeyPair, invalid }     // twoKeyPairs
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
        File file(InternalFS);
        file.open(STATE_FILENAME, FILE_O_READ);
        file.read(buffer, BUFFER_SIZE);
        file.close();
        char* encoded;
        currentState = (State) (buffer[0] + 1);
        Serial.print("The current state is: ");
        Serial.println(currentState);
        switch (currentState) {
            case noKeyPairs:
                Serial.println("No keys have been created.");
                break;
            case oneKeyPair:
                Serial.println("Loading the current keys...");
                publicKey = new uint8_t[KEY_SIZE];
                memcpy(publicKey, buffer + 1, KEY_SIZE);
                encryptedKey = new uint8_t[KEY_SIZE];
                memcpy(encryptedKey, buffer + 1 + KEY_SIZE, KEY_SIZE);
                encoded = Codex::encode(encryptedKey, KEY_SIZE);
                Serial.print("Encrypted Key: ");
                Serial.println(encoded);
                delete [] encoded;
                break;
            case twoKeyPairs:
                Serial.println("Loading the current keys...");
                publicKey = new uint8_t[KEY_SIZE];
                memcpy(publicKey, buffer + 1, KEY_SIZE);
                encryptedKey = new uint8_t[KEY_SIZE];
                memcpy(encryptedKey, buffer + 1 + KEY_SIZE, KEY_SIZE);
                encoded = Codex::encode(encryptedKey, KEY_SIZE);
                Serial.print("Encrypted Key: ");
                Serial.println(encoded);
                delete [] encoded;
                Serial.println("Loading the previous keys...");
                previousPublicKey = new uint8_t[KEY_SIZE];
                memcpy(previousPublicKey, buffer + 1 + 2 * KEY_SIZE, KEY_SIZE);
                previousEncryptedKey = new uint8_t[KEY_SIZE];
                memcpy(previousEncryptedKey, buffer + 1 + 3 * KEY_SIZE, KEY_SIZE);
                encoded = Codex::encode(previousEncryptedKey, KEY_SIZE);
                Serial.print("Previous Encrypted Key: ");
                Serial.println(encoded);
                delete [] encoded;
                break;
        }
    } else {
        Serial.println("Initializing the state file...");
        memset(buffer, 0x00, BUFFER_SIZE);
        InternalFS.remove(STATE_FILENAME);
        File file(InternalFS);
        file.open(STATE_FILENAME, FILE_O_WRITE);
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
        const char* encoded = Codex::encode(encryptedKey, KEY_SIZE);
        Serial.print("Encrypted Key: ");
        Serial.println(encoded);
        delete [] encoded;
    }
    if (previousPublicKey) {
        Serial.println("Saving the previous keys...");
        buffer[0]++;
        memcpy(buffer + 1 + 2 * KEY_SIZE, previousPublicKey, KEY_SIZE);
        memcpy(buffer + 1 + 3 * KEY_SIZE, previousEncryptedKey, KEY_SIZE);
        const char* encoded = Codex::encode(previousEncryptedKey, KEY_SIZE);
        Serial.print("Previous Encrypted Key: ");
        Serial.println(encoded);
        delete [] encoded;
    }
    InternalFS.remove(STATE_FILENAME);
    File file(InternalFS);
    file.open(STATE_FILENAME, FILE_O_WRITE);
    file.write(buffer, BUFFER_SIZE);
    file.flush();
    file.close();
    Serial.println("Done.");
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::generateKeys(uint8_t newSecretKey[KEY_SIZE]) {
    // validate request type
    if (nextState[currentState][1] == 0) {
        Serial.print("The HSM is in an invalid state for this operation: ");
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
    encryptedKey = new uint8_t[KEY_SIZE];
    uint8_t* privateKey = new uint8_t[KEY_SIZE];
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);
    storeState();
    digitalWrite(LED, LOW);

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);

    // update current state
    currentState = nextState[currentState][1];

    return copy;
}


// NOTE: The returned public key must be deleted by the calling program.
const uint8_t* HSM::rotateKeys(uint8_t existingSecretKey[KEY_SIZE], uint8_t newSecretKey[KEY_SIZE]) {
    // validate request type
    if (nextState[currentState][2] == 0) {
        Serial.print("The HSM is in an invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

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
    XOR(existingSecretKey, encryptedKey, privateKey);

    // validate the private key
    if (invalidKeyPair(publicKey, privateKey)) {
        Serial.println("An invalid existing secret key was passed by the mobile device.");
        // clean up and bail
        erase(privateKey, KEY_SIZE);
        digitalWrite(LED, LOW);
        return 0;
    }

    // save copies of the previous public and encrypted keys
    Serial.println("Saving the previous key pair...");
    previousPublicKey = new uint8_t[KEY_SIZE];
    memcpy(previousPublicKey, publicKey, KEY_SIZE);
    previousEncryptedKey = new uint8_t[KEY_SIZE];
    memcpy(previousEncryptedKey, encryptedKey, KEY_SIZE);

    // generate a new key pair
    Serial.println("Generating a new key pair...");
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);

    // encrypt and save the private key
    Serial.println("Hiding the new private key...");
    XOR(newSecretKey, privateKey, encryptedKey);
    erase(privateKey, KEY_SIZE);
    storeState();
    digitalWrite(LED, LOW);

    // return a copy of the public key
    Serial.println("Returning the new public key...");
    uint8_t* copy = new uint8_t[KEY_SIZE];
    memcpy(copy, publicKey, KEY_SIZE);

    // update current state
    currentState = nextState[currentState][2];

    return copy;
}


bool HSM::eraseKeys() {
    // validate request type
    if (nextState[currentState][3] == 0) {
        Serial.print("The HSM is in an invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    Serial.println("Erasing the keys...");
    erase(publicKey, KEY_SIZE);
    erase(encryptedKey, KEY_SIZE);
    erase(previousPublicKey, KEY_SIZE);
    erase(previousEncryptedKey, KEY_SIZE);

    Serial.println("Erasing the state file...");
    memset(buffer, 0x00, BUFFER_SIZE);
    InternalFS.remove(STATE_FILENAME);
    File file(InternalFS);
    file.open(STATE_FILENAME, FILE_O_WRITE);
    file.write(buffer, BUFFER_SIZE);
    file.flush();
    file.close();

    // update current state
    currentState = nextState[currentState][3];

    return true;
}


// NOTE: The returned digest must be deleted by the calling program.
const uint8_t* HSM::digestBytes(const uint8_t* bytes, const size_t size) {
    // validate request type
    if (nextState[currentState][4] == 0) {
        Serial.print("The HSM is in an invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    // generate the digital digest
    SHA512 digester;
    uint8_t* digest = new uint8_t[DIG_SIZE];
    digester.update((const void*) bytes, size);
    digester.finalize(digest, DIG_SIZE);

    // update current state
    currentState = nextState[currentState][4];

    return digest;
}


// NOTE: The returned digital signature must be deleted by the calling program.
const uint8_t* HSM::signBytes(uint8_t secretKey[KEY_SIZE], const uint8_t* bytes, const size_t size) {
    // validate request type
    if (nextState[currentState][5] == 0) {
        Serial.print("The HSM is in an invalid state for this operation: ");
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
        XOR(secretKey, previousEncryptedKey, privateKey);

        // validate the private key
        if (invalidKeyPair(previousPublicKey, privateKey)) {
            Serial.println("An invalid previous secret key was passed by the mobile device.");
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
        erase(previousEncryptedKey, KEY_SIZE);
        storeState();

    } else {

        // decrypt the private key
        XOR(secretKey, encryptedKey, privateKey);

        // validate the private key
        if (invalidKeyPair(publicKey, privateKey)) {
            Serial.println("An invalid secret key was passed by the mobile device.");
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
    digitalWrite(LED, LOW);

    // update current state
    currentState = nextState[currentState][5];

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
    if (nextState[currentState][6] == 0) {
        Serial.print("The HSM is in an invalid state for this operation: ");
        Serial.println(currentState);
        return 0;
    }

    bool isValid = Ed25519::verify(signature, aPublicKey, (const void*) bytes, size);

    // update current state
    currentState = nextState[currentState][6];

    return isValid;
}

