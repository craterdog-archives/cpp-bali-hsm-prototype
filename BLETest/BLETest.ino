#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <Adafruit_BluefruitLE_UART.h>
#include <DigitalNotary.h>
#if SOFTWARE_SERIAL_AVAILABLE
    #include <SoftwareSerial.h>
#endif


// Arduino Configuration
#include "BluefruitConfig.h"
#define FACTORY_RESET_ENABLE        1
#define MINIMUM_FIRMWARE_VERSION    "0.8.0"
#define MODE_LED_BEHAVIOUR          "DISABLE"
#define BLE_POWER_LEVEL             -20


// Create the bluefruit hardware SPI, using SCK/MOSI/MISO hardware SPI pins,
// and then user selected CS/IRQ/RST
Adafruit_BluefruitLE_SPI bluetooth(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);


/*
 * This function configures the HW an the BLE module. It is called
 * automatically on startup.
 */
void setup(void) {

    initConsole();

    initBluetooth();

    Serial.println("Please use Adafruit Bluefruit LE app to connect in UART mode");
    Serial.println("Then Enter characters to send to Bluefruit");
    Serial.println();

    // debug info is a little annoying after this point!
    bluetooth.verbose(false);

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

        // read the next line of data
        bluetooth.readline();

        // echo the line of data to the log
        Serial.println(bluetooth.buffer);
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


void initBluetooth() {
    Serial.println("Initializing the bluetooth module...");

    if ( !bluetooth.begin(VERBOSE_MODE) ) {
        Serial.println("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?");
    }
    Serial.println("OK!");

    if ( FACTORY_RESET_ENABLE ) {
        /* Perform a factory reset to make sure everything is in a known state */
        Serial.println("Performing a factory reset: ");
        if ( ! bluetooth.factoryReset() ) {
            Serial.println("No factory reset");
        }
    }

    /* Disable command echo from Bluefruit */
    bluetooth.echo(false);
    
    /* Limit the range for connections for better security */
    bluetooth.sendCommandCheckOK("AT+BLEPOWERLEVEL=" BLE_POWER_LEVEL);
    
    Serial.println("Requesting Bluefruit info:");
    /* Print Bluefruit information */
    bluetooth.info();
}
