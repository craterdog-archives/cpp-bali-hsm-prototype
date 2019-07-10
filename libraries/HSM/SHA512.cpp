#include <string.h>
#include "SHA512.h"

SHA512::SHA512() {
}

SHA512::~SHA512() {
}

void SHA512::update(const void *data, size_t len) {
    const uint8_t* bytes = (const uint8_t*) data;
    memset(digest, 0x00, 64);
    for (size_t i = 0; i < len; i++) {
        digest[i % 64] = bytes[i] xor digest[i % 64];
    }
}

void SHA512::finalize(void *hash, size_t len) {
    memcpy(hash, digest, 64);
}

