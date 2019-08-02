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
