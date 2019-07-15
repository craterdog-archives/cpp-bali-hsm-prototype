#include <Arduino.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <Adafruit_BluefruitLE_UART.h>
#include "Config.h"
#include <Codex.h>
#include <Formatter.h>
#include <HSM.h>

// Create the bluefruit hardware SPI, using SCK/MOSI/MISO hardware SPI pins,
// and then user selected CS/IRQ/RST
Adafruit_BluefruitLE_SPI bluetooth(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


// Forward declarations
HSM* hsm;

int32_t readRequest(void);
uint8_t** readArguments(void);
void deleteArguments(uint8_t**);
void writeResult(uint8_t* result, size_t length = 0);
void eraseKey(uint8_t* key);
void testHSM(void);


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
        int32_t request = readRequest();
        Serial.print("Request: ");
        uint8_t** arguments = readArguments();

        switch (request) {
            // registerAccount
            case 1: {
                Serial.println("Register Account");
                const uint8_t* anAccountId = arguments[0];
                boolean success = hsm->registerAccount(anAccountId);
                writeResult(success);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println("");
                deleteArguments(arguments);
                break;
            }

            // digestMessage
            case 2: {
                Serial.println("Digest Message");
                const uint8_t* anAccountId = arguments[0];
                const char* message = (const char*) arguments[1];
                const uint8_t* digest = hsm->digestMessage(anAccountId, message);
                writeResult(digest, sizeof digest);
                Serial.println(digest ? "Succeeded" : "Failed");
                Serial.println("");
                deleteArguments(arguments);
                delete [] digest;
                break;
            }

            // generateKeys
            case 3: {
                Serial.println("(Re)Generate Keys");
                const uint8_t* anAccountId = arguments[0];
                uint8_t* newSecretKey = arguments[1];
                uint8_t* secretKey = (sizeof arguments == 3) ? arguments[2] : 0;
                const uint8_t* publicKey = hsm->generateKeys(anAccountId, newSecretKey, secretKey);
                eraseKey(newSecretKey);
                newSecretKey = 0;
                eraseKey(secretKey);
                secretKey = 0;
                writeResult(publicKey, sizeof publicKey);
                Serial.println(publicKey ? "Succeeded" : "Failed");
                Serial.println("");
                deleteArguments(arguments);
                delete [] publicKey;
                break;
            }
 
            // signMessage
            case 4: {
                Serial.println("Sign Message");
                const uint8_t* anAccountId = arguments[0];
                uint8_t* secretKey = arguments[1];
                const char* message = (const char*) arguments[2];
                const uint8_t* signature = hsm->signMessage(anAccountId, secretKey, message);
                eraseKey(secretKey);
                secretKey = 0;
                writeResult(signature, sizeof signature);
                Serial.println(signature ? "Succeeded" : "Failed");
                Serial.println("");
                deleteArguments(arguments);
                delete [] signature;
                break;
            }
 
            // validSignature
            case 5: {
                Serial.println("Valid Signature?");
                const uint8_t* anAccountId = arguments[0];
                const char* message = (const char*) arguments[1];
                const uint8_t* signature = arguments[2];
                const uint8_t* aPublicKey = (sizeof arguments == 4) ? arguments[3] : 0;
                bool isValid = hsm->validSignature(anAccountId, message, signature, aPublicKey);
                writeResult((const uint8_t*) isValid);
                Serial.println(isValid ? "Succeeded" : "Failed");
                Serial.println("");
                deleteArguments(arguments);
                break;
            }
 
            // eraseKeys
            case 6: {
                Serial.println("Erase Keys");
                const uint8_t* anAccountId = arguments[0];
                bool isValid = hsm->eraseKeys(anAccountId);
                writeResult((const uint8_t*) isValid);
                Serial.println(isValid ? "Succeeded" : "Failed");
                Serial.println("");
                deleteArguments(arguments);
                break;
            }

            // testHSM
            case 42: {
                Serial.println("Test HSM");
                testHSM(arguments);
                deleteArguments(arguments);
                break;
            }

            // invalid
            default: {
                Serial.println("Invalid, try again...");
                Serial.println("");
                break;
            }
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


uint8_t* randomBytes(size_t length) {
    uint8_t* bytes = new uint8_t[length];
    for (size_t i = 0; i < length; i++) {
        bytes[i] = (uint8_t) random(256);
    }
    return bytes;
}


int32_t readRequest(void) {
    int32_t request = bluetooth.readline_parseInt();
    if ((request < 1 || request > 6) && request != 42) return 0;
    return request;
}


uint8_t** readArguments(void) {
    int32_t count = bluetooth.readline_parseInt();
    uint8_t** arguments = new uint8_t*[count];
    for (size_t i = 0; i < count; i++) {
        int32_t length = bluetooth.readline_parseInt();
        uint8_t* argument = new uint8_t[length];
        for (size_t j = 0; j < length; j++) {
            size_t timeout = 5;  // seconds
            while (!bluetooth.available()) {
                if (timeout--) {
                    delay(1);  // wait a second
                } else {
                    // timed out so clean up and bail
                    for (size_t k = 0; k < i; k++) {
                        delete [] arguments[k];  // the arguments read in thus far
                    }
                    delete [] argument;  // the argument in progress
                    delete [] arguments;  // the array of arguments
                    return 0;
                }
            }
            argument[j] = bluetooth.read();
        }
        arguments[i] = argument;
    }
    return arguments;
}


void deleteArguments(uint8_t** arguments) {
    size_t count = sizeof arguments;
    for (size_t i = 0; i < count; i++) {
        uint8_t* argument = arguments[i];
        delete [] argument;
    }
    delete [] arguments;
}


void writeResult(bool result) {
    bluetooth.write((int) result);
}


void writeResult(const uint8_t* result, size_t length) {
    for (size_t i = 0; i < length; i++) {
        bluetooth.write(result[i]);
    }
}


void eraseKey(uint8_t* key) {
    if (key) {
        memset(key, 0x00, KEY_SIZE);
        delete [] key;
    }
}


void testHSM(uint8_t** arguments) {
    Serial.println("Resetting the HSM...");
    hsm->resetHSM();
    Serial.println("Done.");
    Serial.println("");

    Serial.println("Registering a new account...");
    const uint8_t* accountId = randomBytes(AID_SIZE);
    bool success = hsm->registerAccount(accountId);
    Serial.println(success ? Codex::encode(accountId, AID_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Generating a message diagest...");
    const char* message = "This is a test of ButtonUp™.";
    const uint8_t* digest = hsm->digestMessage(accountId, message);
    Serial.println(digest ? Codex::encode(digest, DIG_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Generating an initial key pair...");
    uint8_t* secretKey = randomBytes(KEY_SIZE);
    const uint8_t* publicKey = hsm->generateKeys(accountId, secretKey);
    Serial.println(publicKey ? Codex::encode(publicKey, KEY_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Signing the message...");
    const uint8_t* signature = hsm->signMessage(accountId, secretKey, message);
    Serial.println(signature ? Codex::encode(signature, SIG_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Validating the signature...");
    bool isValid = hsm->validSignature(accountId, message, signature, publicKey);
    Serial.println(isValid ? "Is Valid." : "Is Invalid.");
    Serial.println("");

    Serial.println("Generating a new key pair...");
    uint8_t* newSecretKey = randomBytes(KEY_SIZE);
    const uint8_t* newPublicKey = hsm->generateKeys(accountId, newSecretKey, secretKey);
    Serial.println(newPublicKey ? Codex::encode(newPublicKey, KEY_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Signing the certificate...");
    delete [] signature;
    signature = hsm->signMessage(accountId, secretKey, (const char*) newPublicKey);
    Serial.println(signature ? Codex::encode(signature, SIG_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Validating the signature...");
    isValid = hsm->validSignature(accountId, (const char*) newPublicKey, signature, publicKey);
    Serial.println(isValid ? "Is Valid." : "Is Invalid.");
    Serial.println("");

    Serial.println("Signing the message...");
    delete [] signature;
    signature = hsm->signMessage(accountId, newSecretKey, message);
    Serial.println(signature ? Codex::encode(signature, SIG_SIZE) : "Failed.");
    Serial.println("");

    Serial.println("Validating the signature...");
    isValid = hsm->validSignature(accountId, message, signature, newPublicKey);
    Serial.println(isValid ? "Is Valid." : "Is Invalid.");
    Serial.println("");

    Serial.println("Erasing the keys...");
    hsm->eraseKeys(accountId);
    Serial.println("Succeeded.");
    Serial.println("");

    delete [] accountId;
    delete [] digest;
    delete [] secretKey;
    delete [] newSecretKey;
    delete [] publicKey;
    delete [] newPublicKey;
    delete [] signature;
}
