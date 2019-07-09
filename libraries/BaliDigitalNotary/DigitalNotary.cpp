/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#include <string.h>
#include "Ed25519.h"
#include "DigitalNotary.h"


// FREE FUNCTIONS

const size_t SIGNATURE_SIZE = 64;  // bytes

const char* BASE32 = "0123456789ABCDFGHJKLMNPQRSTVWXYZ";

/*
 * offset:    0        1        2        3        4        0
 * byte:  00000111|11222223|33334444|45555566|66677777|...
 * mask:   F8  07  C0 3E  01 F0  0F 80  7C 03  E0  1F   F8  07
 */
int encodeBytes(const uint8_t previous, const uint8_t current, const int byteIndex, char* base32, int charIndex) {
    uint8_t chunk;
    int offset = byteIndex % 5;
    switch (offset) {
        case 0:
            chunk = (current & 0xF8) >> 3;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 1:
            chunk = ((previous & 0x07) << 2) | ((current & 0xC0) >> 6);
            base32[charIndex++] = BASE32[chunk];
            chunk = (current & 0x3E) >> 1;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 2:
            chunk = ((previous & 0x01) << 4) | ((current & 0xF0) >> 4);
            base32[charIndex++] = BASE32[chunk];
            break;
        case 3:
            chunk = ((previous & 0x0F) << 1) | ((current & 0x80) >> 7);
            base32[charIndex++] = BASE32[chunk];
            chunk = (current & 0x7C) >> 2;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 4:
            chunk = ((previous & 0x03) << 3) | ((current & 0xE0) >> 5);
            base32[charIndex++] = BASE32[chunk];
            chunk = current & 0x1F;
            base32[charIndex++] = BASE32[chunk];
            break;
    }
    return charIndex;
}

/*
 * Same as normal, but pad with 0's in "next" byte
 * case:      0        1        2        3        4
 * byte:  xxxxx111|00xxxxx3|00004444|0xxxxx66|000xxxxx|...
 * mask:   F8  07  C0 3E  01 F0  0F 80  7C 03  E0  1F
 */
int encodeLast(const uint8_t last, const int byteIndex, char* base32, int charIndex) {
    uint8_t chunk;
    int offset = byteIndex % 5;
    switch (offset) {
        case 0:
            chunk = (last & 0x07) << 2;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 1:
            chunk = (last & 0x01) << 4;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 2:
            chunk = (last & 0x0F) << 1;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 3:
            chunk = (last & 0x03) << 3;
            base32[charIndex++] = BASE32[chunk];
            break;
        case 4:
            // nothing to do, was handled by previous call
            break;
    }
    return charIndex;
}

const char* encode(const uint8_t* bytes, size_t length) {
    uint8_t previousByte = 0x00;
    uint8_t currentByte = 0x00;
    uint8_t lastByte = bytes[length - 1];
    int size = 3 + (length * 8 / 5);  // account for a partial byte and the trailing null
    char* base32 = new char[size];
    memset(base32, 0x00, size);
    int charIndex = 0;
    for (int i = 0; i < length; i++) {
        previousByte = currentByte;
        currentByte = bytes[i];
        charIndex = encodeBytes(previousByte, currentByte, i, base32, charIndex);
    }
    charIndex = encodeLast(lastByte, length - 1, base32, charIndex);
    return base32;
}

/*
 * offset:    0        1        2        3        4        0
 * byte:  00000111|11222223|33334444|45555566|66677777|...
 * mask:   F8  07  C0 3E  01 F0  0F 80  7C 03  E0  1F   F8  07
 */
void decodeBytes(const uint8_t chunk, const int charIndex, uint8_t* bytes) {
    int byteIndex = charIndex * 5 / 8;  // integer division drops the remainder
    int offset = charIndex % 8;
    switch (offset) {
        case 0:
            bytes[byteIndex] |= chunk << 3;
            break;
        case 1:
            bytes[byteIndex++] |= chunk >> 2;
            bytes[byteIndex] |= chunk << 6;
            break;
        case 2:
            bytes[byteIndex] |= chunk << 1;
            break;
        case 3:
            bytes[byteIndex++] |= chunk >> 4;
            bytes[byteIndex] |= chunk << 4;
            break;
        case 4:
            bytes[byteIndex++] |= chunk >> 1;
            bytes[byteIndex] |= chunk << 7;
            break;
        case 5:
            bytes[byteIndex] |= chunk << 2;
            break;
        case 6:
            bytes[byteIndex++] |= chunk >> 3;
            bytes[byteIndex] |= chunk << 5;
            break;
        case 7:
            bytes[byteIndex] |= chunk;
            break;
    }
}

/*
 * Same as normal, but pad with 0's in "next" byte
 * case:      0        1        2        3        4
 * byte:  xxxxx111|00xxxxx3|00004444|0xxxxx66|00077777|...
 * mask:   F8  07  C0 3E  01 F0  0F 80  7C 03  E0  1F
 */
void decodeLast(const uint8_t chunk, const int charIndex, uint8_t* bytes) {
    int byteIndex = charIndex * 5 / 8;  // integer division drops the remainder
    int offset = charIndex % 8;
    switch (offset) {
        case 1:
            bytes[byteIndex] |= chunk >> 2;
            break;
        case 3:
            bytes[byteIndex] |= chunk >> 4;
            break;
        case 4:
            bytes[byteIndex] |= chunk >> 1;
            break;
        case 6:
            bytes[byteIndex] |= chunk >> 3;
            break;
        case 7:
            bytes[byteIndex] |= chunk;
            break;
    }
}

const uint8_t* decode(const char* base32) {
    size_t length = strlen(base32);
    int size = length * 5 / 8;  // integer division drops the remainder
    uint8_t* bytes = new uint8_t[size];
    memset(bytes, 0x00, size);
    char character;
    uint8_t chunk;
    for (int i = 0; i < length; i++) {
        character = base32[i];
        chunk = (uint8_t) (strchr(BASE32, character) - BASE32);  // index of character in base 32
        if (i < length - 1) {
            decodeBytes(chunk, i, bytes);
        } else {
            decodeLast(chunk, i, bytes);
        }
    }
    return bytes;
}


// PUBLIC METHODS

DigitalNotary::DigitalNotary() {
    generateKeyPair();
}

DigitalNotary::~DigitalNotary() {
    forgetKeyPair();
}

char* DigitalNotary::generateKeyPair() {
    Ed25519::generatePrivateKey(privateKey);
    Ed25519::derivePublicKey(publicKey, privateKey);
    // TODO: generate and notarize the public certificate
    return 0;
}

void DigitalNotary::forgetKeyPair() {
    for (int i = 0; i < PRIVATE_SIZE; i++) privateKey[i] = 0;
    for (int j = 0; j < PUBLIC_SIZE; j++) publicKey[j] = 0;
}

const char* DigitalNotary::notarizeMessage(const char* message) {
    size_t length = strlen(message);
    uint8_t signature[SIGNATURE_SIZE];
    uint8_t bytes[length];
    for (int i = 0; i < length; i++) {
        bytes[i] = (uint8_t) message[i];
    }
    Ed25519::sign(signature, privateKey, publicKey, message, length);
    const char* seal = encode(signature, SIGNATURE_SIZE);
    return seal;
}

bool DigitalNotary::sealIsValid(const char* message, const char* seal) {
    size_t length = strlen(message);
    const uint8_t* signature = decode(seal);
    bool isValid = Ed25519::verify(signature, publicKey, message, length);
    delete [] signature;
    return isValid;
}

