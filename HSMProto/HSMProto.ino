#include <bluefruit.h>
#include <Codex.h>
#include <HSM.h>


// Forward Declarations
uint8_t readRequest(void);
void writeResult(bool result);
void writeResult(uint8_t* result, size_t length);
void testHSM(void);


// Bluetooth Services
BLEDis  bledis;  // device information service
BLEUart bleuart; // UART communication service


/*
 * This structure is used to reference the section of the request buffer that
 * contains a specific argument that was passed as part of the request. Each
 * request has the following byte format:
 *   Request (1 byte) [0..255]
 *   Number of Arguments (1 byte) [0..255]
 *   Length of Argument 1 (2 bytes) [0..65535]
 *   Argument 1 ([0..65535] bytes)
 *   Length of Argument 2 (2 bytes) [0..65535]
 *   Argument 2 ([0..65535] bytes)
 *      ...
 *   Length of Argument N (2 bytes) [0..65535]
 *   Argument N ([0..65535] bytes)
 *
 * If the entire request is only a single byte long then the number of arguments
 * is assumed to be zero.
 */
struct Argument {
    uint8_t* pointer;
    size_t length;
};


/*
 * The request buffer is used to hold all of the information associated with
 * a request that is received from a paired mobile device. For efficiency, the
 * arguments are referenced inline in the buffer rather than being copied into
 * their own memory.
 */
const int BUFFER_SIZE = 5000;
uint8_t buffer[BUFFER_SIZE];
Argument* arguments = 0;
size_t numberOfArguments = 0;


/*
 * The hardware security module (HSM) encapsulates and protects the private key
 * and implements all the required public-private cryptographic functions.
 */
HSM* hsm;


/*
 * This function configures the adafruit feather. It is called automatically
 * on startup.
 */
void setup(void) {
    initConsole();
    initHSM();
    initBluetooth();
    suspendLoop();  // empty anyway
}


/*
 * This function is called repeatedly once the adafruit feather has been setup.
 */
void loop(void) {
    // Everything is handled by event callbacks
}


/*
 * This function initializes the serial console if one exists.
 */
void initConsole() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("Wearable Identity Console");
    Serial.println("-------------------------");
    Serial.println("");
}


/*
 * This function initializes the hardware security module (HSM).
 */
void initHSM() {
    Serial.println("Loading the state of the HSM...");
    hsm = new HSM();
    Serial.println("Done.");
    Serial.println("");
}


/*
 * This function initializes the low energy bluetooth processor.
 */
void initBluetooth() {
    Serial.println("Initializing the bluetooth module...");

    // Setup the BLE LED to be enabled on CONNECT.
    Bluefruit.autoConnLed(true);

    // Config the peripheral connection
    // Note: Maximum bandwith requires more SRAM.
    Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);

    Bluefruit.begin();

    // Limit the range for connections for better security
    // Allowed values: -40, -20, -16, -12, -8, -4, 0, and 4
    Bluefruit.setTxPower(-20);

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
    bleuart.setRxCallback(requestCallback);
    //bleuart.setNotifyCallback(responseCallback);

    // Configure and start advertising for the device
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    //Bluefruit.Advertising.addTxPower();              // uses the current BLE power level
    Bluefruit.Advertising.addService(bleuart);
    Bluefruit.Advertising.addName();                 // uses the device name specified above
    Bluefruit.Advertising.restartOnDisconnect(true); // auto advertising when disconnected
    Bluefruit.Advertising.setInterval(32, 244);      // fast mode and slow mode (in units of 0.625 ms)
    Bluefruit.Advertising.setFastTimeout(30);        // timeout in seconds for fast mode, then slow mode
    Bluefruit.Advertising.start(0);                  // 0 = Don't stop advertising after N seconds  

    Serial.println("Done.");
    Serial.println("");
}


/*
 * This callback function is invoked each time a connection to the device occurs.
 */
void connectCallback(uint16_t connectionHandle) {
    BLEConnection* connection = Bluefruit.Connection(connectionHandle);
    char peerName[64] = { 0 };
    connection->getPeerName(peerName, 64);
    Serial.print("Connected to ");
    Serial.println(peerName);
    Serial.println("");
}


/*
 * This callback function is invoked each time a request is received by the BLE UART.
 */
void requestCallback(uint16_t connectionHandle) {

    // read the next request from the mobile device
    uint8_t request = readRequest();
    Serial.print("Request: ");

    switch (request) {
        // testHSM (self test)
        case 0: {
            Serial.println("Test HSM");
            testHSM();
            Serial.println("Test Complete.");
            Serial.println("");
            break;
        }

        // digestMessage
        case 1: {
            Serial.println("Digest Message");
            boolean success = false;
            const char* message = (const char*) arguments[0].pointer;
            if (arguments[0].length == strlen(message) + 1) {
                const uint8_t* digest = hsm->digestMessage(message);
                if (digest) {
                    success = true;
                    writeResult(digest, DIG_SIZE);
                    delete [] digest;
                }
            }
            if (!success) writeResult(false);
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        // generateKeys
        case 2: {
            Serial.println("(Re)Generate Keys");
            boolean success = false;
            if (numberOfArguments > 0 && numberOfArguments < 3 && arguments[0].length == KEY_SIZE) {
                const uint8_t* publicKey;
                uint8_t* newSecretKey = arguments[0].pointer;
                if (numberOfArguments == 2 && arguments[1].length == KEY_SIZE) {
                    uint8_t* existingSecretKey = arguments[1].pointer;
                    publicKey = hsm->generateKeys(newSecretKey, existingSecretKey);
                    memset(existingSecretKey, 0x00, KEY_SIZE);
                } else {
                    publicKey = hsm->generateKeys(newSecretKey);
                }
                if (publicKey) {
                    success = true;
                    Serial.print("Public Key: ");
                    Serial.println(Codex::encode(publicKey, KEY_SIZE));
                    writeResult(publicKey, KEY_SIZE);
                    delete [] publicKey;
                }
                memset(newSecretKey, 0x00, KEY_SIZE);
            }
            if (!success) writeResult(false);
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        // signMessage
        case 3: {
            Serial.println("Sign Message");
            boolean success = false;
            uint8_t* secretKey = arguments[0].pointer;
            if (arguments[0].length == KEY_SIZE) {
                const char* message = (const char*) arguments[1].pointer;
                if (arguments[1].length == strlen(message) + 1) {
                    const uint8_t* signature = hsm->signMessage(secretKey, message);
                    if (signature) {
                        success = true;
                        Serial.print("Signatue: ");
                        Serial.println(Codex::encode(signature, SIG_SIZE));
                        writeResult(signature, SIG_SIZE);
                        delete [] signature;
                    }
                }
            }
            memset(secretKey, 0x00, KEY_SIZE);
            if (!success) writeResult(false);
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        // validSignature
        case 4: {
            Serial.println("Valid Signature?");
            boolean success = false;
            const char* message = (const char*) arguments[0].pointer;
            if (arguments[0].length == strlen(message) + 1) {
                uint8_t* signature = arguments[1].pointer;
                if (arguments[1].length == SIG_SIZE) {
                    if (numberOfArguments == 3) {
                        uint8_t* aPublicKey = arguments[2].pointer;
                        if (arguments[2].length == KEY_SIZE) {
                            success = hsm->validSignature(message, signature, aPublicKey);
                        }
                    } else {
                        success = hsm->validSignature(message, signature);
                    }
                }
            }
            if (!success) writeResult(false);
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        // eraseKeys
        case 5: {
            Serial.println("Erase Keys");
            boolean success = false;
            success = hsm->eraseKeys();
            writeResult(success);
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        // invalid
        default: {
            Serial.print(request);
            Serial.println(" - Invalid request, try again...");
            Serial.println("");
            break;
        }
    }

}


/*
 * This callback function is invoked each time a connection to the device is lost.
 */
void disconnectCallback(uint16_t connectionHandle, uint8_t reason) {
    Serial.print("Disconnected from mobile device - reason code: 0x");
    Serial.println(reason, HEX);
    Serial.println("See: ~/Library/Arduino15/packages/adafruit/hardware/nrf52/0.11.1/cores/nRF5/nordic/softdevice/s140_nrf52_6.1.1_API/include/ble_hci.h");
    Serial.println("");
}


/*
 * This function generates a new byte array containing the specified number of
 * random bytes.
 */
uint8_t* randomBytes(size_t length) {
    uint8_t* bytes = new uint8_t[length];
    for (size_t i = 0; i < length; i++) {
        bytes[i] = (uint8_t) random(256);
    }
    return bytes;
}


/*
 * This function reads the next request from the BLE UART and its arguments from the request
 * buffer and indexes the arguments to make them easy to access.
 */
uint8_t readRequest() {
    memset(buffer, 0x00, BUFFER_SIZE);
    size_t byteCount = bleuart.read(buffer, BUFFER_SIZE);
    Serial.print("Number of bytes read: ");
    Serial.println(byteCount);
    if (byteCount == 0 || byteCount == BUFFER_SIZE) {
        // invalid request
        return 0;
    }
    size_t index = 0;
    uint8_t request = buffer[index++];
    numberOfArguments = buffer[index++];
    Serial.print("Number of arguments: ");
    Serial.println(numberOfArguments);
    if (arguments) delete [] arguments;
    arguments = new Argument[numberOfArguments];
    for (size_t i = 0; i < numberOfArguments; i++) {
        uint16_t numberOfBytes = buffer[index++] << 8 | buffer[index++];
        arguments[i].pointer = buffer + index;
        arguments[i].length = numberOfBytes;
        index += numberOfBytes;
    }
    if (index != byteCount) return 0;  // invalid request format
    return request;
}


/*
 * This function writes the result of a request as a boolean value to the BLE UART.
 */
void writeResult(bool result) {
    bleuart.write((uint8_t) result);
    bleuart.flush();
}


/*
 * This function writes the result of a request as a byte array value to the BLE UART.
 */
void writeResult(const uint8_t* result, size_t length) {
    bleuart.write(result, length);
    bleuart.flush();
}


/*
 * This function causes the HSM to run a self-test that executes each of its functions
 * at least once.
 */
void testHSM() {
    Serial.println("Generating a message digest...");
    const char* message = "This is a test of ButtonUp™.";
    const uint8_t* digest = hsm->digestMessage(message);
    if (digest) {
        const char* encoded = Codex::encode(digest, DIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
        delete [] digest;
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Generating an initial key pair...");
    uint8_t* secretKey = randomBytes(KEY_SIZE);
    const uint8_t* publicKey = hsm->generateKeys(secretKey);
    if (publicKey) {
        const char* encoded = Codex::encode(publicKey, KEY_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Signing the message...");
    const uint8_t* signature = hsm->signMessage(secretKey, message);
    if (signature) {
        const char* encoded = Codex::encode(signature, SIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Validating the signature...");
    if (hsm->validSignature(message, signature, publicKey)) {
        Serial.println("Is Valid.");
    } else {
        Serial.println("Is Invalid.");
    }
    delete [] signature;
    Serial.println("");

    Serial.println("Generating a new key pair...");
    uint8_t* newSecretKey = randomBytes(KEY_SIZE);
    const uint8_t* newPublicKey = hsm->generateKeys(newSecretKey, secretKey);
    if (newPublicKey) {
        const char* encoded = Codex::encode(newPublicKey, KEY_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Signing the certificate...");
    signature = hsm->signMessage(secretKey, (const char*) newPublicKey);
    if (signature) {
        const char* encoded = Codex::encode(signature, SIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Validating the signature...");
    if (hsm->validSignature((const char*) newPublicKey, signature, publicKey)) {
        Serial.println("Is Valid.");
    } else {
        Serial.println("Is Invalid.");
    }
    delete [] signature;
    Serial.println("");

    Serial.println("Signing the message...");
    delete [] signature;
    signature = hsm->signMessage(newSecretKey, message);
    if (signature) {
        const char* encoded = Codex::encode(signature, SIG_SIZE);
        Serial.println(encoded);
        delete [] encoded;
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Validating the signature...");
    if (hsm->validSignature(message, signature, newPublicKey)) {
        Serial.println("Is Valid.");
    } else {
        Serial.println("Is Invalid.");
    }
    delete [] signature;
    Serial.println("");

    Serial.println("Erasing the keys...");
    if (hsm->eraseKeys()) {
        Serial.println("Succeeded.");
    } else {
        Serial.println("Failed.");
    }
    Serial.println("");

    Serial.println("Cleaning up...");
    delete [] secretKey;
    delete [] newSecretKey;
    delete [] publicKey;
    delete [] newPublicKey;
}
