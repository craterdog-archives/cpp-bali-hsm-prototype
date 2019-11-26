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
#include <bluefruit.h>
#include <Codex.h>
#include <HSM.h>


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
    Serial.println("The ArmorD™ v1.2 Console");
    Serial.println("------------------------");
    Serial.println("");
}


/*
 * The hardware security module (HSM) encapsulates and protects the private key
 * and implements all the required public-private cryptographic functions.
 */
HSM* hsm;


/*
 * This function initializes the hardware security module (HSM).
 */
void initHSM() {
    Serial.println("Initializing the hardware security module (HSM)...");
    hsm = new HSM();
    Serial.println("Done.");
    Serial.println("");
}


// Bluetooth Services
const uint16_t BLOCK_SIZE = 510;  // max MTU size minus first two bytes
BLEDis  bledis;  // device information service
BLEUart bleuart(BLOCK_SIZE + 3); // UART communication service (FIFO size)


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
    Bluefruit.setTxPower(-40);

    // The name will be displayed in the mobile app
    Bluefruit.setName("ArmorD");

    // Add callbacks for important events
    Bluefruit.Periph.setConnectCallback(connectCallback);
    Bluefruit.Periph.setDisconnectCallback(disconnectCallback);

    // Configure and start the Device Information Service
    bledis.setManufacturer("Crater Dog Technologies");
    bledis.setModel("v1");
    bledis.begin();
  
    // Configure and start UART Communication Service
    bleuart.begin();
    bleuart.setRxCallback(requestCallback);
    bleuart.setNotifyCallback(notifyCallback);

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


char* REASON[] = {
    "Success",
    "Unknown BLE Command",
    "Unknown Connection Identifier",
    "Hardware Failure",
    "Page Timeout",
    "Authentication Failure",
    "Pin or Key missing",
    "Memory Capacity Exceeded",
    "Connection Timeout",
    "Connection Limit Exceeded",
    "Synchronous Connection Limit To A Device Exceeded",
    "ACL Connection Already Exists",
    "Command Disallowed",
    "Connection Rejected due to Limited Resources",
    "Connection Rejected Due To Security Reasons",
    "Connection Rejected due to Unacceptable BD_ADDR",
    "Connection Accept Timeout Exceeded",
    "Unsupported Feature or Parameter Value",
    "Invalid BLE Command Parameters",
    "Remote User Terminated Connection",
    "Remote Device Terminated Connection due to low resources",
    "Remote Device Terminated Connection due to power off",
    "Local Host Terminated Connection",
    "Repeated Attempts",
    "Pairing Not Allowed",
    "Unknown LMP PDU",
    "Unsupported Remote Feature",
    "SCO Offset Rejected",
    "SCO Interval Rejected",
    "SCO Air Mode Rejected",
    "Invalid LMP Parameters",
    "Unspecified Error",
    "Unsupported LMP Parameter Value",
    "Role Change Not Allowed",
    "LMP Response Timeout",
    "LMP Error Transaction Collision/LL Procedure Collision",
    "LMP PDU Not Allowed",
    "Encryption Mode Not Acceptable",
    "Link Key Can Not be Changed",
    "Requested QoS Not Supported",
    "Instant Passed",
    "Pairing with Unit Key Unsupported",
    "Different Transaction Collision",
    "Reserved",
    "QoS Unacceptable Parameter",
    "QoS Rejected",
    "Channel Classification Not Supported",
    "Insufficient Security",
    "Parameter Out Of Mandatory Range",
    "Reserved",
    "Role Switch Pending",
    "Reserved",
    "Reserved Slot Violation",
    "Role Switch Failed",
    "Extended Inquiry Response Too Large",
    "Secure Simple Pairing Not Supported By Host",
    "Host Busy - Pairing",
    "Connection Rejected due to No Suitable Channel Found",
    "Controller Busy",
    "Connection Interval Unacceptable",
    "Directed Advertisement Timeout",
    "Connection Terminated due to MIC Failure",
    "Connection Failed to be Established"
};

/*
 * This callback function is invoked each time a connection to the device is lost.
 * See: ~/Library/Arduino15/packages/adafruit/hardware/nrf52\/*\/cores/nRF5/nordic/softdevice/s140_nrf52_*_API/include/ble_hci.h");
 */
void disconnectCallback(uint16_t connectionHandle, uint8_t reason) {
    Serial.print("Disconnected from mobile device - reason code: 0x");
    Serial.println(reason, HEX);
    Serial.println(REASON[reason]);
    Serial.println("");
}


/*
 * The request buffer is used to hold all of the information associated with one
 * or more requests that are received from a paired mobile device. For efficiency,
 * the arguments are referenced inline in the buffer rather than being copied into
 * their own memory.
 */
const int BUFFER_SIZE = 10200;  // 20 blocks * 510 bytes/block
uint8_t buffer[BUFFER_SIZE];    // a multiple of the block size


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
Argument* arguments = 0;


// Forward Declarations
RequestType readRequest();
void writeResult(bool result);
void writeResult(uint8_t* result, size_t length);
void writeError();


/*
 * This callback function is invoked each time a request is received by the BLE UART.
 */
void requestCallback(uint16_t connectionHandle) {
    // read the next request from the mobile device
    RequestType requestType = readRequest();
    if (requestType == BadRequest) {
        Serial.println("Unable to read the request.");
        writeError();
        return;
    }

    // process the request
    Serial.print("Request: ");
    switch (requestType) {
        case LoadBlock: {
            Serial.println("Load Block");
            writeResult(true);
            Serial.println("Succeeded");
            Serial.println("");
            break;
        }

        case GenerateKeys: {
            Serial.println("Generate Keys");
            bool success = false;
            if (arguments[0].length == KEY_SIZE) {
                uint8_t* newMobileKey = arguments[0].pointer;
                const uint8_t* publicKey = hsm->generateKeys(newMobileKey);
                memset(newMobileKey, 0x00, KEY_SIZE);
                if (publicKey) {
                    success = true;
                    const char* encoded = Codex::encode(publicKey, KEY_SIZE);
                    Serial.print("Public Key: ");
                    Serial.println(encoded);
                    delete [] encoded;
                    writeResult(publicKey, KEY_SIZE);
                    delete [] publicKey;
                }
            }
            if (!success) writeError();
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        case RotateKeys: {
            Serial.println("Rotate Keys");
            bool success = false;
            if (arguments[0].length == KEY_SIZE && arguments[1].length == KEY_SIZE) {
                uint8_t* existingMobileKey = arguments[0].pointer;
                uint8_t* newMobileKey = arguments[1].pointer;
                const uint8_t* publicKey = hsm->rotateKeys(existingMobileKey, newMobileKey);
                memset(existingMobileKey, 0x00, KEY_SIZE);
                memset(newMobileKey, 0x00, KEY_SIZE);
                if (publicKey) {
                    success = true;
                    const char* encoded = Codex::encode(publicKey, KEY_SIZE);
                    Serial.print("Public Key: ");
                    Serial.println(encoded);
                    delete [] encoded;
                    writeResult(publicKey, KEY_SIZE);
                    delete [] publicKey;
                }
            }
            if (!success) writeError();
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        case EraseKeys: {
            Serial.println("Erase Keys");
            bool success = false;
            if (hsm->eraseKeys()) {
                success = true;
                writeResult(success);
            }
            if (!success) writeError();
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        case DigestBytes: {
            Serial.println("Digest Bytes");
            bool success = false;
            const uint8_t* bytes = arguments[0].pointer;
            const size_t size = arguments[0].length;
            const uint8_t* digest = hsm->digestBytes(bytes, size);
            if (digest) {
                const char* encoded = Codex::encode(digest, DIG_SIZE);
                Serial.print("Bytes Digest: ");
                Serial.println(encoded);
                delete [] encoded;
                success = true;
                writeResult(digest, DIG_SIZE);
                delete [] digest;
            }
            if (!success) writeError();
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        case SignBytes: {
            Serial.println("Sign Bytes");
            bool success = false;
            if (arguments[0].length == KEY_SIZE) {
                uint8_t* mobileKey = arguments[0].pointer;
                const uint8_t* bytes = arguments[1].pointer;
                const size_t size = arguments[1].length;
                const uint8_t* signature = hsm->signBytes(mobileKey, bytes, size);
                memset(mobileKey, 0x00, KEY_SIZE);
                if (signature) {
                    success = true;
                    const char* encoded = Codex::encode(signature, SIG_SIZE);
                    Serial.print("Signature: ");
                    Serial.println(encoded);
                    delete [] encoded;
                    writeResult(signature, SIG_SIZE);
                    delete [] signature;
                }
            }
            if (!success) writeError();
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        case ValidSignature: {
            Serial.println("Valid Signature?");
            bool success = false;
            if (arguments[0].length == KEY_SIZE && arguments[1].length == SIG_SIZE) {
                uint8_t* aPublicKey = arguments[0].pointer;
                uint8_t* signature = arguments[1].pointer;
                const uint8_t* bytes = arguments[2].pointer;
                const size_t size = arguments[2].length;
                success = hsm->validSignature(aPublicKey, signature, bytes, size);
                writeResult(success);
            }
            if (!success) writeError();
            Serial.println(success ? "Succeeded" : "Failed");
            Serial.println("");
            break;
        }

        default: {
            Serial.print("Invalid request type (");
            Serial.print(requestType);
            Serial.println("), try again...");
            Serial.println("");
            writeError();
            break;
        }
    }

}


/*
 * This callback function is invoked each time notification is enabled or disabled.
 */
void notifyCallback(uint16_t connectionHandle, bool enabled) {
    /*
    Serial.print("Notification was ");
    Serial.print(enabled ? "enabled" : "disabled");
    Serial.println(".");
    */
}


/*
 * This function reads the next request from the BLE UART and its arguments from the request
 * buffer and indexes the arguments to make them easy to access. Each request has one of the
 * following formats:
 *
 * One Block of a Large Request (greater than 512 bytes)
 * [    1 byte    ][      1 byte       ][     510 bytes      ]
 * [     0xFF     ][ the block number  ][ one block of bytes ]
 *
 * The First Block of a Complete Request
 * [    1 byte    ][      1 byte       ][  510 bytes   ]
 * [ request type ][number of arguments][block of bytes]
 *
 * The buffer containing the full request has the form:
 * [      2 bytes       ][ size of argument 1 ][      2 bytes       ][ size of argument 2 ]...
 * [ size of argument 1 ][  argument 1 bytes  ][ size of argument 2 ][  argument 2 bytes  ]...
 * 
 */
RequestType readRequest() {
    Serial.println("Attempting to read...");

    // Read in the request information
    RequestType requestType = (RequestType) bleuart.read();
    if (requestType == LoadBlock) {
        // It's an extended sized request so load in one block of it
        uint8_t blockNumber = bleuart.read();
        if ((blockNumber + 1) * BLOCK_SIZE > BUFFER_SIZE) {
            Serial.print("The request contains too many blocks: ");
            Serial.println(blockNumber + 1);  // zero based block numbering
            return BadRequest;
        }
        Serial.print("Block number: ");
        Serial.println(blockNumber);
        size_t bytesRead = bleuart.read(buffer + blockNumber * BLOCK_SIZE, BLOCK_SIZE);
        Serial.print("Bytes read: ");
        Serial.println(bytesRead);
    } else {
        // It's a full request so parse it
        uint8_t numberOfArguments = bleuart.read();
        Serial.print("Number of arguments: ");
        Serial.println(numberOfArguments);
        size_t bytesRead = bleuart.read(buffer, BLOCK_SIZE);
        Serial.print("Bytes read: ");
        Serial.println(bytesRead);
        size_t index = 0;
        if (arguments) delete [] arguments;
        arguments = new Argument[numberOfArguments];
        for (size_t i = 0; i < numberOfArguments; i++) {
            uint16_t numberOfBytes = buffer[index++] << 8 | buffer[index++];
            arguments[i].pointer = buffer + index;
            arguments[i].length = numberOfBytes;
            index += numberOfBytes;
            if (index > BUFFER_SIZE) {
                Serial.print("The request was corrupted.");
                return BadRequest;
            }
        }
    }
    return requestType;
}


/*
 * This function writes the result of a request as a boolean value to the BLE UART.
 */
void writeResult(bool result) {
    bleuart.write(result ? 0x01 : 0x00);
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
 * This function writes the error result of a request to the BLE UART.
 */
void writeError() {
    bleuart.write(0xFF);
    bleuart.flush();
}

