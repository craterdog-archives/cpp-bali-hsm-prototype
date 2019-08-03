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
 1. Click on the "check mark" button to compile the sketch, and then on the "right arrow" button to upload it to your board.
 1. Finally, click on the "magnifying glass" button in the upper right corner to bring up the Serial Monitor. You should see it output something like the following:
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

The three states include the following:
 * *No Key Pairs* - There are currently no key pairs stored on the HSM.
 * *One Key Pair* - A key pair has been generated and is ready to use.
 * *Two Key Pairs* - The HSM is in the middle of rotating the current key pair.

The six possible request types include the following:
 * *GenerateKeys* - Generate a new key pair and return the new public key.
 * *RotateKeys* - Save the existing key pair, generate a new pair and return the new public key.
 * *EraseKeys* - Erase all key pairs from the hardware security module (HSM).
 * *DigestBytes* - Generate and return a SHA512 digest of an array of bytes.
 * *SignBytes* - Digitally sign an array of bytes using the private key and return the digital signature.
 * *SignatureValid* - Return whether or not a digital signature can be validated using a public key.

### Binary Protocol
The feather board prototype implements a binary request protocol that runs on top of the bluetooth low energy (BLE) transport protocol. The binary protocol supports a small set of request types. Each request has the following form:
```
[ T ][ N ][  s1  ][   arg 1   ][  s2  ][   arg 2   ]...[  sN  ][   arg N   ]
```

The byte fields are as follows:
 * *T*: request type (1 byte)
 * *N*: the number of arguments (1 byte)
 * *s1*: the size of first argument (2 bytes)
 * *arg 1*: the first argument (s1 bytes)
 * *s2*: the size of second argument (2 bytes)
 * *arg 2*: the second argument (s2 bytes)
 *   ⋮
 * *sN*: the size of Nth argument (2 bytes)
 * *arg N*: the Nth argument (sN bytes)

A request can currently consist of a maximum of 4096 bytes. The first two bytes make up the _header_ which defines the type of request and how many arguments are included with it. The rest of the bytes define the _arguments_ that are passed with the request.

If the total size of the request `S` exceeds 512 bytes, the request must be broken up into an additional `K` 512 byte blocks that are each sent separately before sending the actual request block:
```
[0][K][((S - 2) modulo 510) + 2 bytes]
[0][J][510 bytes]
[0][I][510 bytes]
  ⋮
[0][2][510 bytes]
[0][1][510 bytes]
[T][N][510 bytes]
```

Each additional block has a request type of zero with the second byte in the header being the block number. The blocks are sent in *reverse* order, the last part of the total request is sent first and the first part of the total request is sent last. The request is then assembled in order by the feather board:
```
[request][block 1][block 2]...[block I][block J][block K]
```

#### Generate Keys
This request type tells the feather board to generate a new set of keys. It assumes no keys yet exist and returns the new public key. The request has the form:
 * request type: 0x02
 * number of arguments: 0x01
 * size of argument 1: 0x20
 * argument 1: a new random secret key of 32 bytes

The response has the form:
 * a public key of 32 bytes

#### Regenerate Keys
This request type tells the feather board to regenerate the set of keys. It assumes a set of keys already exist and returns the new public key. The request has the form:
 * request type: 0x02
 * number of arguments: 0x02
 * size of argument 1: 0x20
 * argument 1: a new random secret key of 32 bytes
 * size of argument 2: 0x20
 * argument 2: the existing random secret key of 32 bytes

The response has the form:
 * a public key of 32 bytes

#### Sign Bytes
This request type tells the feather board to use the private key to digitally sign the specified bytes. The request has the form:
 * request type: 0x03
 * number of arguments: 0x02
 * size of argument 1: 0x20
 * argument 1: the existing secret key of 32 bytes
 * size of argument 2: 0xNNNN
 * argument 2: the N bytes to be digitally signed

The response has the form:
 * a digital signature of 64 bytes

