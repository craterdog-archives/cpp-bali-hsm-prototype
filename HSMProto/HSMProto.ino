#include <bluefruit.h>
//#include "Config.h"  //not used
#include <Codex.h>
#include <HSM.h>

BLEDis  bledis;  // device information service
BLEUart bleuart; // UART communication service


// Forward declarations
uint8_t readRequest(void);
void writeResult(bool result);
void writeResult(uint8_t* result, size_t length);
void testHSM(void);

struct Argument {
    uint8_t* pointer;
    size_t length;
};

const int BUFFER_SIZE = 2000;
uint8_t buffer[BUFFER_SIZE];
Argument* arguments = 0;

HSM* hsm;


/*
 * This function configures the adafruit feather. It is called automatically
 * on startup.
 */
void setup(void) {
    initConsole();
    initHSM();
    initBluetooth();
}


/*
 * This function is called repeatedly once the adafruit feather has been setup.
 */
void loop(void) {
    // Everything is handled by event callbacks
}


void initConsole() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println(F("Wearable Identity Console"));
    Serial.println(F("-------------------------"));
    Serial.println(F(""));
}


void initHSM() {
    Serial.println(F("Loading the state of the HSM..."));
    hsm = new HSM();
    Serial.println(F("Done."));
    Serial.println(F(""));
}


void initBluetooth() {
    Serial.println(F("Initializing the bluetooth module..."));

    // Setup the BLE LED to be enabled on CONNECT.
    Bluefruit.autoConnLed(true);

    // Config the peripheral connection
    // Note: Maximum bandwith requires more SRAM.
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

    Bluefruit.begin();

    // Limit the range for connections for better security
    // Allowed values: -40, -20, -16, -12, -8, -4, 0, and 4
    //Bluefruit.setTxPower(-40);
    Bluefruit.setTxPower(-4);

    // The name will be displayed in the mobile app
    Bluefruit.setName("ButtonUp™");

    // Add callbacks for important events
    Bluefruit.Periph.setConnectCallback(connectCallback);
    Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

    // Configure and start the Device Information Service
    bledis.setManufacturer("Adafruit Industries");
    bledis.setModel("Bluefruit Feather52");
    bledis.begin();
  
    // Configure and start UART Communication Service
    bleuart.begin();
    bleuart.setRxCallback(readCallback);

    // Configure and start advertising for the device
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();              // uses the current BLE power level
    Bluefruit.Advertising.addService(bleuart);
    Bluefruit.ScanResponse.addName();                // uses the device name specified above
    Bluefruit.Advertising.restartOnDisconnect(true); // auto advertising when disconnected
    Bluefruit.Advertising.setInterval(32, 244);      // fast mode and slow mode (in units of 0.625 ms)
    Bluefruit.Advertising.setFastTimeout(30);        // timeout in seconds for fast mode, then slow mode
    Bluefruit.Advertising.start(0);                  // 0 = Don't stop advertising after N seconds  

    Serial.println(F("Done."));
    Serial.println(F(""));
}


/**
 * This function is invoked each time a connection to the device occurs.
 */
void connectCallback(uint16_t connectionHandle) {
    BLEConnection* connection = Bluefruit.Connection(connectionHandle);
    char peerName[32] = { 0 };
    connection->getPeerName(peerName, sizeof(peerName));
    Serial.print("Connected to ");
    Serial.println(peerName);
    Serial.println("");
}


/**
 * This function is invoked each time data can be read from the UART.
 */
void readCallback(uint16_t connectionHandle) {

    // Check for incoming request from mobile device
    if (bleuart.available()) {

        // read the next request
        uint8_t request = readRequest();
        Serial.print(F("Request: "));

        switch (request) {
            // registerAccount
            case 1: {
                Serial.println(F("Register Account"));
                boolean success = false;
                const uint8_t* anAccountId = arguments[0].pointer;
                if (arguments[0].length == AID_SIZE) {
                    success = hsm->registerAccount(anAccountId);
                }
                writeResult(success);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println(F(""));
                break;
            }

            // digestMessage
            case 2: {
                Serial.println(F("Digest Message"));
                boolean success = false;
                const uint8_t* anAccountId = arguments[0].pointer;
                if (arguments[0].length == AID_SIZE) {
                    const char* message = (const char*) arguments[1].pointer;
                    if (arguments[1].length == strlen(message) + 1) {
                        const uint8_t* digest = hsm->digestMessage(anAccountId, message);
                        if (digest) {
                            success = true;
                            writeResult(digest, sizeof digest);
                            delete [] digest;
                        }
                    }
                }
                if (!success) writeResult(false);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println(F(""));
                break;
            }

            // generateKeys
            case 3: {
                Serial.println(F("(Re)Generate Keys"));
                boolean success = false;
                const uint8_t* anAccountId = arguments[0].pointer;
                if (arguments[0].length == AID_SIZE) {
                    uint8_t* newSecretKey = arguments[1].pointer;
                    if (arguments[1].length == KEY_SIZE) {
                        const uint8_t* publicKey;
                        if (sizeof arguments == 3) {
                            uint8_t* secretKey = arguments[2].pointer;
                            if (arguments[2].length == KEY_SIZE) {
                                publicKey = hsm->generateKeys(anAccountId, newSecretKey, secretKey);
                            }
                            memset(secretKey, 0x00, KEY_SIZE);
                        } else {
                            publicKey = hsm->generateKeys(anAccountId, newSecretKey);
                        }
                        if (publicKey) {
                            success = true;
                            writeResult(publicKey, sizeof publicKey);
                            delete [] publicKey;
                        }
                    }
                    memset(newSecretKey, 0x00, KEY_SIZE);
                }
                if (!success) writeResult(false);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println(F(""));
                break;
            }
 
            // signMessage
            case 4: {
                Serial.println(F("Sign Message"));
                boolean success = false;
                const uint8_t* anAccountId = arguments[0].pointer;
                if (arguments[0].length == AID_SIZE) {
                    uint8_t* secretKey = arguments[1].pointer;
                    if (arguments[1].length == KEY_SIZE) {
                        const char* message = (const char*) arguments[2].pointer;
                        if (arguments[2].length == strlen(message) + 1) {
                            const uint8_t* signature = hsm->signMessage(anAccountId, secretKey, message);
                            if (signature) {
                                success = true;
                                writeResult(signature, sizeof signature);
                                delete [] signature;
                            }
                        }
                    }
                    memset(secretKey, 0x00, KEY_SIZE);
                }
                if (!success) writeResult(false);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println(F(""));
                break;
            }
 
            // validSignature
            case 5: {
                Serial.println(F("Valid Signature?"));
                boolean success = false;
                const uint8_t* anAccountId = arguments[0].pointer;
                if (arguments[0].length == AID_SIZE) {
                    const char* message = (const char*) arguments[1].pointer;
                    if (arguments[1].length == strlen(message) + 1) {
                        uint8_t* signature = arguments[2].pointer;
                        if (arguments[2].length == SIG_SIZE) {
                            if (sizeof arguments == 4) {
                                uint8_t* aPublicKey = arguments[3].pointer;
                                if (arguments[3].length == KEY_SIZE) {
                                    success = hsm->validSignature(anAccountId, message, signature, aPublicKey);
                                }
                            } else {
                                success = hsm->validSignature(anAccountId, message, signature);
                            }
                        }
                    }
                }
                if (!success) writeResult(false);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println(F(""));
                break;
            }
 
            // eraseKeys
            case 6: {
                Serial.println(F("Erase Keys"));
                boolean success = false;
                const uint8_t* anAccountId = arguments[0].pointer;
                if (arguments[0].length == AID_SIZE) {
                    success = hsm->eraseKeys(anAccountId);
                }
                writeResult(success);
                Serial.println(success ? "Succeeded" : "Failed");
                Serial.println(F(""));
                break;
            }

            // testHSM
            case 42: {
                Serial.println(F("Test HSM"));
                testHSM();
                break;
            }

            // invalid
            default: {
                Serial.print(request);
                Serial.println(F(" - Invalid request, try again..."));
                Serial.println(F(""));
                break;
            }
        }

    }

}


/**
 * This function is invoked each time a connection to the device is lost.
 */
void disconnectCallback(uint16_t connectionHandle, uint8_t reason) {
    Serial.print("Disconnected from mobile device - reason code: 0x");
    Serial.println(reason, HEX);
    Serial.println("See: ~/Library/Arduino15/packages/adafruit/hardware/nrf52/0.11.1/cores/nRF5/nordic/softdevice/s140_nrf52_6.1.1_API/include/ble_hci.h");
    Serial.println("");
}


uint8_t* randomBytes(size_t length) {
    uint8_t* bytes = new uint8_t[length];
    for (size_t i = 0; i < length; i++) {
        bytes[i] = (uint8_t) random(256);
    }
    return bytes;
}


uint8_t readRequest() {
    size_t count = bleuart.read(buffer, BUFFER_SIZE);
    if (count == 0 || count == BUFFER_SIZE) {
        // invalid request
        return 0;
    }
    size_t index = 0;
    uint8_t request = buffer[index++];
    if (request == 42) count = 1;
    uint8_t numberOfArguments = (count > 1) ? buffer[index++] : 0;
    if (arguments) delete [] arguments;
    arguments = new Argument[numberOfArguments];
    for (size_t i = 0; i < numberOfArguments; i++) {
        uint16_t numberOfBytes = buffer[index++] << 8 | buffer[index++];
        arguments[i].pointer = buffer + index;
        arguments[i].length = numberOfBytes;
        index += numberOfBytes;
    }
    if (index != count) return 0;  // invalid request format
    return request;
}


void writeResult(bool result) {
    bleuart.write((uint8_t) result);
}


void writeResult(const uint8_t* result, size_t length) {
    bleuart.write(result, length);
}


void testHSM() {
    Serial.println(F("Resetting the HSM..."));
    hsm->resetHSM();
    Serial.println(F("Done."));
    Serial.println(F(""));

    Serial.println(F("Registering a new account..."));
    const uint8_t* accountId = randomBytes(AID_SIZE);
    if (hsm->registerAccount(accountId)) {
        const char* encoded = Codex::encode(accountId, AID_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Generating a message digest..."));
    const char* message = "This is a test of ButtonUp™.";
    const uint8_t* digest = hsm->digestMessage(accountId, message);
    if (digest) {
        const char* encoded = Codex::encode(digest, DIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
        delete [] digest;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Generating an initial key pair..."));
    uint8_t* secretKey = randomBytes(KEY_SIZE);
    const uint8_t* publicKey = hsm->generateKeys(accountId, secretKey);
    if (publicKey) {
        const char* encoded = Codex::encode(publicKey, KEY_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Signing the message..."));
    const uint8_t* signature = hsm->signMessage(accountId, secretKey, message);
    if (signature) {
        const char* encoded = Codex::encode(signature, SIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Validating the signature..."));
    if (hsm->validSignature(accountId, message, signature, publicKey)) {
        Serial.println(F("Is Valid."));
    } else {
        Serial.println(F("Is Invalid."));
    }
    delete [] signature;
    Serial.println(F(""));

    Serial.println(F("Generating a new key pair..."));
    uint8_t* newSecretKey = randomBytes(KEY_SIZE);
    const uint8_t* newPublicKey = hsm->generateKeys(accountId, newSecretKey, secretKey);
    if (newPublicKey) {
        const char* encoded = Codex::encode(newPublicKey, KEY_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Signing the certificate..."));
    signature = hsm->signMessage(accountId, secretKey, (const char*) newPublicKey);
    if (signature) {
        const char* encoded = Codex::encode(signature, SIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Validating the signature..."));
    if (hsm->validSignature(accountId, (const char*) newPublicKey, signature, publicKey)) {
        Serial.println(F("Is Valid."));
    } else {
        Serial.println(F("Is Invalid."));
    }
    delete [] signature;
    Serial.println(F(""));

    Serial.println(F("Signing the message..."));
    delete [] signature;
    signature = hsm->signMessage(accountId, newSecretKey, message);
    if (signature) {
        const char* encoded = Codex::encode(signature, SIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    Serial.println(F("Validating the signature..."));
    if (hsm->validSignature(accountId, message, signature, newPublicKey)) {
        Serial.println(F("Is Valid."));
    } else {
        Serial.println(F("Is Invalid."));
    }
    delete [] signature;
    Serial.println(F(""));

    Serial.println(F("Erasing the keys..."));
    if (hsm->eraseKeys(accountId)) {
        Serial.println(F("Succeeded."));
    } else {
        Serial.println(F("Failed."));
    }
    Serial.println(F(""));

    delete [] accountId;
    delete [] secretKey;
    delete [] newSecretKey;
    delete [] publicKey;
    delete [] newPublicKey;
}
