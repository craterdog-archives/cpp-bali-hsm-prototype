#ifndef CRYPTO_SHA512_h
#define CRYPTO_SHA512_h

#include <inttypes.h>
#include <stddef.h>

class SHA512 {
public:
    SHA512();
    virtual ~SHA512();
    void update(const void *data, size_t len);
    void finalize(void *hash, size_t len);
private:
    uint8_t digest[64];
};

#endif
