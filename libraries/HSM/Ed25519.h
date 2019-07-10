#ifndef CRYPTO_ED25519_h
#define CRYPTO_ED25519_h

#include <inttypes.h>
#include <stddef.h>

class Ed25519 {
public:
    static void sign(uint8_t signature[64], const uint8_t privateKey[32],
                     const uint8_t publicKey[32], const void *message, size_t len);

    static bool verify(const uint8_t signature[64], const uint8_t publicKey[32],
                       const void *message, size_t len);

    static void generatePrivateKey(uint8_t privateKey[32]);

    static void derivePublicKey(uint8_t publicKey[32], const uint8_t privateKey[32]);

private:
    Ed25519();
    ~Ed25519();
};

#endif
