![Logo](https://raw.githubusercontent.com/craterdog-bali/bali-project-documentation/master/images/CraterDogLogo.png)

### Arduino Wearable Identity Prototype
This project provides a C++ implementation of a _hardware security module_ (HSM) that can be uploaded to an [Adafruit BLE nRF52 Feather Board](https://www.adafruit.com/product/3406). The code implements the peripheral side of the security protocol defined in the [Bali Digital Notary™](https://github.com/craterdog-bali/js-bali-digital-notary) project. Specifically, it can be used with the [HSM proxy module](https://github.com/craterdog-bali/js-bali-digital-notary/blob/master/src/v1/HSM.js) running on nodeJS.

### Getting Started
To get started with this project you should do the following:
 1. Purchase an [Adafruit BLE nRF52 Feather Board](https://www.adafruit.com/product/3406) from Adafruit. Include in the order a rechargeable battery like the [Lithium Ion Polymer 3.7v 350mAh](https://www.adafruit.com/product/2750) which plugs into the board.
 1. Download and install, on your Mac or PC, the [Arduino IDE](https://www.arduino.cc/en/Main/Software).
 1. Configure the IDE by following [these instructions](https://learn.adafruit.com/adafruit-arduino-ide-setup/arduino-1-dot-6-x-ide).
 1. Clone this project on your computer by doing the following:
     ```
     git clone https://github.com/derknorton/wearable-hsm-prototype.git
     ```
 1. Open the Arduino IDE and load the _sketch_ that is in this folder of the project using the `File/Open...` menu:
     ```
     ./wearable-hsm-prototype/HSMProto/HSMProto.ino
     ```
 1. Connect your feather board to your computer using a micro USB cable.
 1. Select the board type (Adafruit Bluefruit Feather nRF52832) from the `Tools/Board` menu.
 1. Select the serial port as something like "cu.SLAB_USBtoUART" from the `Tools/Port` menu.
 1. The nRF52832 processor has an outdated bootloader. Select the "Bootloader DFU for the nRF52832" from the `Tools/Programmer` menu.
 1. Upload the new bootloader by selecting `Tools/Burn Bootloader` and then wait for it to finish.
 1. Then select the "Arduino as ISP" programmer from the `Tools/Programmer` menu.
 1. Click on the "check mark" button to compile the sketch, and then on the "right arrow" button to upload it to your board.
 1. When the code is done uploading, click on the "magnifying glass" button in the upper right corner to bring up the Serial Monitor. You should see it output something like the following:
     ```
     09:56:11.300 -> The ButtonUp™ Console
     09:56:12.110 -> ---------------------
     09:56:12.110 ->
     09:56:12.110 -> Initializing the hardware security module (HSM)...
     09:56:12.110 -> Loading the state of the HSM...
     09:56:12.145 -> Reading the state file...
     09:56:12.145 -> STATE: 000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
     09:56:12.145 -> No keys have been created.
     09:56:12.145 -> Done.
     09:56:12.145 ->
     09:56:12.145 -> Initializing the bluetooth module...
     09:56:12.425 -> Done.
     09:56:12.425 ->
     ```

### State Machine
The hardware security module (HSM) implemented by the feather board prototype defines a simple state machine consisting of the three possible state in which the HSM may be at any given time as well as the transitions between states caused by each type of request.

![State Machine](https://github.com/derknorton/wearable-hsm-prototype/blob/master/docs/images/StateMachine.png)

#### States
The three states include the following:
 * _No Key Pairs_ - There are currently no key pairs stored on the HSM.
 * _One Key Pair_ - A key pair has been generated and is ready to use.
 * _Two Key Pairs_ - The HSM is in the middle of rotating the current key pair.

#### Request Types
The six possible request types include the following:
 * _Generate Keys_ - Generate a new key pair and return the new public key.
 * _Rotate Keys_ - Save the existing key pair, generate a new pair and return the new public key.
 * _Erase Keys_ - Erase all key pairs from the hardware security module (HSM).
 * _Digest Bytes_ - Generate and return a SHA512 digest of an array of bytes.
 * _Sign Bytes_ - Digitally sign an array of bytes using the private key and return the digital signature.
 * _Signature Valid?_ - Return whether or not a digital signature can be validated using a public key.

#### Key Rotation
Special attention should be paid to the concept of rotating the key pairs. Each key pair is good for signing about 100 byte arrays before it has "leaked" enough information to make calculating the private key a remote possibility. So in general, it is best to rotate the keys about every 100 signing requests. The process of rotating the keys takes several steps:
 1. Save the existing key pair.
 1. Generate a new key pair and return the new public key to the mobile device.
 1. The mobile device embeds the new public key in a public certificate that contains a citation to the public certificate for the previous key pair.
 1. The HSM digitally signs the new public certificate using the **previous** private key.
 1. The mobile device publishes the new public certificate to the cloud.

This process ensures that each public certificate can be validated using the previous public certificate in the chain. It adds some complexity by requiring a third state in the state machine (Two Key Pairs), but is well worth the added security and auditablility.

### Binary Protocol
The feather board prototype implements a binary request protocol that runs on top of the bluetooth low energy (BLE) transport protocol. The binary protocol supports a small set of request types. Each request has the following form:
```
[ T ][ N ][  s1  ][     arg 1     ][  s2  ][     arg 2     ]...[  sN  ][     arg N     ]
```

The byte fields are as follows:
 * _T_: request type (1 byte)
 * _N_: the number of arguments (1 byte)
 * _s1_: the size of first argument (2 bytes)
 * _arg 1_: the first argument (s1 bytes)
 * _s2_: the size of second argument (2 bytes)
 * _arg 2_: the second argument (s2 bytes)
 *   ⋮
 * _sN_: the size of Nth argument (2 bytes)
 * _arg N_: the Nth argument (sN bytes)

#### Additional Blocks
A request can consist of a maximum of 20,000 bytes. The first two bytes make up the _header_ which defines the type of request and how many arguments are included with it. The rest of the bytes define the _arguments_ that are passed with the request.

If the total size `S` of the request exceeds 512 bytes, the request must be broken up into an additional `K` 512 byte blocks that are each sent separately before sending the actual request block:
```
[0][K][((S - 2) modulo 510) + 2 bytes]
[0][J][510 bytes]
[0][I][510 bytes]
  ⋮
[0][2][510 bytes]
[0][1][510 bytes]
[T][N][510 bytes]
```

Each additional block has a request type of `0` with the second byte in the header being the block number. The blocks are sent in **reverse** order, the last part of the total request is sent first and the first part of the total request is sent last. The request is then assembled in order by the feather board:
```
[request][block 1][block 2]...[block I][block J][block K]
```

The reasoning behind this ordering is that the feather board need not care about the content of the additional blocks until the request itself has been received.

#### Generate Keys
This request type tells the feather board to generate a new public-private key pair. It is only valid if no keys yet exist and it returns the new public key. The request has the form:
```
[0x01][0x01][0x0020][32 bytes]
```
 * **Request Type**: `0x01`
 * **Number of Arguments**: `0x01`
 * **Size of Argument 1**: `0x0020`
 * **Argument 1**: `[a new random secret key of 32 bytes]`

The response from this request type has one of the following forms:
 * **Successful**:`[the new public key of 32 bytes]`
 * **Failed**:`0xFF`

#### Rotate Keys
This request type tells the feather board to save a copy of the existing public-private key pair and then generate a new public-private key pair. It is only valid exactly one public-private key pair exists and it returns the new public key. The request has the form:
```
[0x02][0x02][0x0020][32 bytes][0x0020][32 bytes]
```
 * **Request Type**: `0x02`
 * **Number of Arguments**: `0x02`
 * **Size of Argument 1**: `0x0020`
 * **Argument 1**: `[the existing secret key of 32 bytes]`
 * **Size of Argument 2**: `0x0020`
 * **Argument 2**: `[a new random secret key of 32 bytes]`

The response from this request type has one of the following forms:
 * **Successful**:`[the new public key of 32 bytes]`
 * **Failed**:`0xFF`

#### Erase Keys
This request type tells the feather board to erase from its memory all public-private key pairs. It is valid when the board is in any state. The request has the form:
```
[0x03][0x00]
```
 * **Request Type**: `0x03`
 * **Number of Arguments**: `0x00`

The response from this request type has one of the following forms:
 * **Successful**:`0x00` (`false`) or `0x01` (`true`)
 * **Failed**:`0xFF`

#### Digest Bytes
This request type tells the feather board to generate a SHA512 digest of a byte array. It is not valid when two public-private key pairs exist, and it returns the digest. The request has the form:
```
[0x04][0x01][0xnnnn][N bytes]
```
 * **Request Type**: `0x04`
 * **Number of Arguments**: `0x01`
 * **Size of Argument 1**: `0xnnnn`
 * **Argument 1**: `[a byte array of N bytes]`

The response from this request type has one of the following forms:
 * **Successful**:`[the digest of 64 bytes]`
 * **Failed**:`0xFF`

#### Sign Bytes
This request type tells the feather board to digitally sign a byte array using the current, or if one exists, previous private key. It is only valid when at least one public-private key pair exists, and it returns the digital signature. The request has the form:
```
[0x05][0x02][0x0020][32 bytes][0xnnnn][N bytes]
```
 * **Request Type**: `0x05`
 * **Number of Arguments**: `0x02`
 * **Size of Argument 1**: `0x0020`
 * **Argument 1**: `[the previous or current secret key of 32 bytes]`
 * **Size of Argument 2**: `0xnnnn`
 * **Argument 2**: `[a byte array of N bytes]`

The response from this request type has one of the following forms:
 * **Successful**:`[the digital signature of 64 bytes]`
 * **Failed**:`0xFF`

#### Signature Valid?
This request type tells the feather board to determine whether or not a digital signature and a byte array can be validated using a public key. It is not valid when two public-private key pairs exist. The request has the form:
```
[0x06][0x03][0x0020][32 bytes][0x0040][64 bytes][0xnnnn][N bytes]
```
 * **Request Type**: `0x06`
 * **Number of Arguments**: `0x03`
 * **Size of Argument 3**: `0x0020`
 * **Argument 1**: `[a public key of 32 bytes]`
 * **Size of Argument 2**: `0x0040`
 * **Argument 2**: `[a digital signature of 64 bytes]`
 * **Size of Argument 1**: `0xnnnn`
 * **Argument 3**: `[a byte array of N bytes]`

The response from this request type has one of the following forms:
 * **Successful**:`0x00` (`false`) or `0x01` (`true`)
 * **Failed**:`0xFF`

