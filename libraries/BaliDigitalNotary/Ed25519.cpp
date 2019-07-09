#include <string.h>
#include "Ed25519.h"

void Ed25519::sign(uint8_t signature[64], const uint8_t privateKey[32],
                   const uint8_t publicKey[32], const void *message, size_t len) {
    for (size_t i = 0; i < 64; i++) {
        signature[i] = (((const uint8_t *) message)[i % len]) ^ publicKey[i % 32];
    }
}

bool Ed25519::verify(const uint8_t signature[64], const uint8_t publicKey[32],
                     const void *message, size_t len) {
    uint8_t candidate[64];
    Ed25519::sign(candidate, publicKey, publicKey, message, len);
    bool isValid = (memcmp(candidate, signature, 64) == 0);
    return isValid;
}

void Ed25519::generatePrivateKey(uint8_t privateKey[32]) {
    memset(privateKey, 0x55, 32);
}

void Ed25519::derivePublicKey(uint8_t publicKey[32], const uint8_t privateKey[32]) {
    memset(publicKey, 0xAA, 32);
}
