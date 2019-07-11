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
const uint8_t* publicKey;
uint8_t* secretKey;

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
    while (bluetooth.available()) {

        // signal an incoming request
        digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
        digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW

        // read the next message
        bluetooth.readline();
        const char* message = bluetooth.buffer;
        Serial.print("message: ");
        Serial.println(message);

        // notarize the message
        const uint8_t* signature = hsm->signMessage(secretKey, message);
        const char* encoded = Codex::encode(signature, 64);
        Serial.print("signature: ");
        Serial.println(encoded);

        // validate the signature
        bool isValid = hsm->validSignature(publicKey, message, signature);
        if (isValid) {
            Serial.println("The signature is valid.");
        } else {
            Serial.println("The signature is invalid.");
        }

        // clean up
        delete [] signature;
        delete [] encoded;

    }

}


void initConsole() {
    // initialize the LED
    pinMode(LED_BUILTIN, OUTPUT);

    // initialize the logging console
    while (!Serial);
    delay(500);
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

    Serial.println("Generating a new key pair...");
    secretKey = new uint8_t[32];
    memset(secretKey, 0x9D, 32);
    publicKey = hsm->generateKeys(secretKey);
    Serial.println("Done.");
    Serial.println("");
}


void initBluetooth() {
    Serial.println("Initializing the bluetooth module...");

    if ( !bluetooth.begin(VERBOSE_MODE) ) {
        Serial.println("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?");
    }
    Serial.println("OK!");

    if ( FACTORY_RESET_ENABLE ) {
        // Perform a factory reset to make sure everything is in a known state
        Serial.println("Performing a factory reset...");
        if ( ! bluetooth.factoryReset() ) {
            Serial.println("No factory reset occurred.");
        } else {
            Serial.println("Done.");
            Serial.println("");
        }
    }

    // Disable command echo from Bluefruit
    Serial.println("Disabling the command echo...");
    bluetooth.echo(false);
    Serial.println("Done.");
    Serial.println("");
    
    // Limit the range for connections for better security
    // Allowed values: -40, -20, -16, -12, -8, -4, 0, and 4
    Serial.println("Limit the range for bluetooth...");
    bluetooth.sendCommandCheckOK("AT+BLEPOWERLEVEL=-40");
    Serial.println("Done.");
    Serial.println("");
    
    // Print Bluefruit information
    Serial.println("Requesting Bluefruit info...");
    bluetooth.info();
    Serial.println("Done.");
    Serial.println("");
    
    Serial.println("Please use Adafruit Bluefruit LE app to connect in UART mode");
    Serial.println("Then Enter characters to send to Bluefruit");
    Serial.println();

    // Debug info is a little annoying after this point!
    Serial.println("Turning off verbose output...");
    bluetooth.verbose(false);
    Serial.println("Done.");
    Serial.println("");

    // Wait for connection
    while (! bluetooth.isConnected()) {
        delay(500);
    }

    // LED Activity command is only supported from 0.6.6
    if ( bluetooth.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) ) {
        // Change Mode LED Activity
        Serial.println("******************************");
        Serial.println("Change LED activity to " MODE_LED_BEHAVIOUR);
        bluetooth.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
        Serial.println("******************************");
    }

    Serial.println("Switching to DATA mode!");
    bluetooth.setMode(BLUEFRUIT_MODE_DATA);
}
