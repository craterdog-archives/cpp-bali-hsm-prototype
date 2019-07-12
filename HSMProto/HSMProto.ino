#include <Arduino.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <Adafruit_BluefruitLE_UART.h>
#include "Config.h"
#include <Codex.h>
#include <HSM.h>

// Create the bluefruit hardware SPI, using SCK/MOSI/MISO hardware SPI pins,
// and then user selected CS/IRQ/RST
Adafruit_BluefruitLE_SPI bluetooth(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


HSM* hsm;

// These are just here for testing
uint8_t* randomBytes(size_t length);
const char* lastMessage = strdup("");
uint8_t* lastSecretKey = randomBytes(32);
const uint8_t* lastPublicKey;
const uint8_t* lastSignature;


/*
 * This function configures the HW an the BLE module. It is called
 * automatically on startup.
 */
void setup(void) {
    initConsole();
    initHSM();
    initBluetooth();
}


/*
 * This function polls the BLE (bluetooth) hardware for new requests. It
 * is called automatically after setup().
 */
void loop(void) {
    
    // Check for incoming characters from Bluefruit
    if (bluetooth.available()) {

        // signal an incoming request
        digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
        digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW

        // read the next request
        bluetooth.readline();
        const char* request = bluetooth.buffer;
        Serial.print("request: ");
        Serial.println(request);

        switch (request[0]) {
            // digestMessage
            case 'd': {
                // read the message
                Serial.println("digestMessage: enter the message");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* message = bluetooth.buffer;
                message = getMessage(message);  // just for testing
                Serial.print("message: ");
                Serial.println(message);

                // digest the message
                const uint8_t* digest = hsm->digestMessage(message);
                const char* encodedDigest = Codex::encode(digest, 64);
                Serial.print("digest: ");
                Serial.println(encodedDigest);
                Serial.println("");
                delete [] digest;
                delete [] encodedDigest;
                break;
            }

            // validSignature
            case 'v': {
                // read the public key
                Serial.println("validSignature: enter a public key");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* encodedPublicKey = bluetooth.buffer;
                Serial.print("a public key: ");
                Serial.println(encodedPublicKey);
                const uint8_t* aPublicKey = getPublicKey(encodedPublicKey);

                // read the message
                Serial.println("validSignature: enter the message");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* message = bluetooth.buffer;
                message = getMessage(message);  // just for testing
                Serial.print("message: ");
                Serial.println(message);

                // read the signature
                Serial.println("validSignature: enter the signature");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* encodedSignature = bluetooth.buffer;
                Serial.print("signature: ");
                Serial.println(encodedSignature);
                const uint8_t* signature = getSignature(encodedSignature);

                // validate the signature
                bool isValid = hsm->validSignature(aPublicKey, message, signature);
                Serial.print("is valid: ");
                Serial.println(isValid);
                Serial.println("");
                break;
            }
 
            // generateKeys
            case 'g': {
                // read the secret key
                Serial.println("generateKeys: enter the secret key");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* encodedSecretKey = bluetooth.buffer;
                Serial.print("secret key: ");
                Serial.println(encodedSecretKey);
                uint8_t* secretKey = getSecretKey(encodedSecretKey);

                // generate the new keys
                const uint8_t* publicKey = hsm->generateKeys(secretKey);
                delete [] lastPublicKey;
                lastPublicKey = publicKey;
                const char* encodedPublicKey = Codex::encode(publicKey, 32);
                Serial.print("publicKey: ");
                Serial.println(encodedPublicKey);
                Serial.println("");
                delete [] encodedPublicKey;

                // sign the new public key
                const uint8_t* signature = hsm->signMessage(secretKey, encodedPublicKey);
                delete [] lastSignature;
                lastSignature = signature;
                const char* encodedSignature = Codex::encode(signature, 64);
                Serial.print("signature: ");
                Serial.println(encodedSignature);
                Serial.println("");
                delete [] encodedSignature;
                break;
            }
 
            // signMessage
            case 's': {
                // read the secret key
                Serial.println("signMessage: enter the secret key");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* encodedSecretKey = bluetooth.buffer;
                Serial.print("secret key: ");
                Serial.println(encodedSecretKey);
                uint8_t* secretKey = getSecretKey(encodedSecretKey);

                // read the message
                Serial.println("signMessage: enter the message");
                while (!bluetooth.available()) delay(500);
                bluetooth.readline();
                const char* message = bluetooth.buffer;
                message = getMessage(message);  // just for testing
                Serial.print("message: ");
                Serial.println(message);

                // sign the message
                const uint8_t* signature = hsm->signMessage(secretKey, message);
                delete [] lastSignature;
                lastSignature = signature;
                const char* encodedSignature = Codex::encode(signature, 64);
                Serial.print("signature: ");
                Serial.println(encodedSignature);
                Serial.println("");
                delete [] encodedSignature;
                break;
            }
 
            // eraseKeys
            case 'e': {
                Serial.println("eraseKeys");
                hsm->eraseKeys();
                break;
            }

            default: Serial.println("Invalid command, try again...");
        }

    }

}


void initConsole() {
    // initialize the LED
    pinMode(LED_BUILTIN, OUTPUT);

    // initialize the logging console
    while (!Serial) delay(500);
    Serial.begin(115200);

    // logging console header
    Serial.println("Wearable Identity Console");
    Serial.println("-------------------------");
    Serial.println("");
}


void initHSM() {
    Serial.println("Loading the state of the HSM...");
    hsm = new HSM();
    Serial.println("Done.");
    Serial.println("");
}


uint8_t* randomBytes(size_t length) {
    uint8_t* bytes = new uint8_t[length];
    for (size_t i = 0; i < length; i++) {
        bytes[i] = (uint8_t) random(256);
    }
    return bytes;
}


void initBluetooth() {
    Serial.println("Initializing the bluetooth module...");

    // Set the bluetooth logging to verbose
    if (!bluetooth.begin(VERBOSE_MODE)) {
        Serial.println("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?");
    }

    // Perform a factory reset to make sure everything is in a known state
    if (FACTORY_RESET_ENABLE && !bluetooth.factoryReset()) {
        Serial.println("Unable to perform a factory reset.");
    }

    // Disable the command echo for bluetooth
    bluetooth.echo(false);
    
    // Limit the range for connections for better security
    // Allowed values: -40, -20, -16, -12, -8, -4, 0, and 4
    bluetooth.sendCommandCheckOK("AT+BLEPOWERLEVEL=-40");
    
    // Print Bluefruit information
    bluetooth.info();
    
    // Debug info is a little annoying after this point!
    bluetooth.verbose(false);

    Serial.println("Done.");
    Serial.println("");

    Serial.println("Waiting for a connection...");
    // Wait for connection
    while (!bluetooth.isConnected()) delay(500);

    // Set LED activity mode
    bluetooth.sendCommandCheckOK("AT+HWModeLED=DISABLE");

    // Switch to data mode
    bluetooth.setMode(BLUEFRUIT_MODE_DATA);
    
    Serial.println("Connected.");
    Serial.println("");
}


// The following code is just here for testing and should be removed once a real mobile
// client has been developed.

const char* getMessage(const char* message) {
    if (strcmp(message, "") == 0) {
        return lastMessage;
    } else {
        delete [] lastMessage;
        lastMessage = strdup(message);
    }
}


uint8_t* getSecretKey(const char* encodedSecretKey) {
    if (strcmp(encodedSecretKey, "") == 0) {
        return lastSecretKey;
    } else {
        delete [] lastSecretKey;
        lastSecretKey = Codex::decode(encodedSecretKey);
    }
}


const uint8_t* getPublicKey(const char* encodedPublicKey) {
    if (strcmp(encodedPublicKey, "") == 0) {
        return lastPublicKey;
    } else {
        delete [] lastPublicKey;
        lastPublicKey = Codex::decode(encodedPublicKey);
    }
}


const uint8_t* getSignature(const char* encodedSignature) {
    if (strcmp(encodedSignature, "") == 0) {
        return lastSignature;
    } else {
        delete [] lastSignature;
        lastSignature = Codex::decode(encodedSignature);
    }
}

// End of test code.

