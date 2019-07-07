#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <Adafruit_BluefruitLE_UART.h>
#include <DigitalNotary.h>
#include <string.h>
#if SOFTWARE_SERIAL_AVAILABLE
    #include <SoftwareSerial.h>
#endif


// Arduino Configuration
#include "BluefruitConfig.h"
#define FACTORY_RESET_ENABLE        1
#define MINIMUM_FIRMWARE_VERSION    "0.8.0"
#define MODE_LED_BEHAVIOUR          "DISABLE"
#define BLE_POWER_LEVEL             -10


// Create the bluefruit hardware SPI, using SCK/MOSI/MISO hardware SPI pins,
// and then user selected CS/IRQ/RST
Adafruit_BluefruitLE_SPI bluetooth(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


DigitalNotary* notary;

/*
 * This function configures the HW an the BLE module. It is called
 * automatically on startup.
 */
void setup(void) {

    initConsole();

    initNotary();
    
    initBluetooth();

}


/*
 * This function polls the BLE (bluetooth) hardware for new requests. It
 * is called automatically after setup().
 */
void loop(void) {
    char* message;
    const char* seal;
    
    // Check for incoming characters from Bluefruit
    while (bluetooth.available()) {

        // signal an incoming request
        digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
        digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW

        // read the next message
        bluetooth.readline();
        message = bluetooth.buffer;
        Serial.println(strlen(message));

        // echo the message to the log
        Serial.println(message);

        // notarize the message
        seal = notary->notarizeMessage(message);
        // echo the notary seal to the log
        Serial.println(seal);
        delete [] seal;

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


void initNotary() {
    Serial.println("Generating a new key pair...");
    notary = new DigitalNotary();
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
        /* Perform a factory reset to make sure everything is in a known state */
        Serial.println("Performing a factory reset...");
        if ( ! bluetooth.factoryReset() ) {
            Serial.println("No factory reset occurred.");
        } else {
            Serial.println("Done.");
            Serial.println("");
        }
    }

    /* Disable command echo from Bluefruit */
    Serial.println("Disabling the command echo...");
    bluetooth.echo(false);
    Serial.println("Done.");
    Serial.println("");
    
    /* Limit the range for connections for better security */
    Serial.println("Limit the range for bluetooth...");
    bluetooth.sendCommandCheckOK("AT+BLEPOWERLEVEL=" BLE_POWER_LEVEL);
    Serial.println("Done.");
    Serial.println("");
    
    /* Print Bluefruit information */
    Serial.println("Requesting Bluefruit info...");
    bluetooth.info();
    Serial.println("Done.");
    Serial.println("");
    
    Serial.println("Please use Adafruit Bluefruit LE app to connect in UART mode");
    Serial.println("Then Enter characters to send to Bluefruit");
    Serial.println();

    // debug info is a little annoying after this point!
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
