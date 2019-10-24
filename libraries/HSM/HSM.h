/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.        *
 *                                                                      *
 * This source code is for reference purposes only.  It is protected by *
 * US Patent 9,853,813 and any use of this source code will be deemed   *
 * an infringment of the patent.  Crater Dog Technologies(TM) retains   *
 * full ownership of this source code.                                  *
 ************************************************************************/
#ifndef HSM_H
#define HSM_H

#include <inttypes.h>
#include <stddef.h>

#define KEY_SIZE 32  // key size in bytes
#define DIG_SIZE 64  // digest size in bytes
#define SIG_SIZE 64  // digital signature size in bytes


// STATE MACHINE

enum State {
    Invalid = 0,
    NoKeyPairs = 1,
    OneKeyPair = 2,
    TwoKeyPairs = 3
};

enum RequestType {
    LoadBlock = 0,
    GenerateKeys = 1,
    RotateKeys = 2,
    EraseKeys = 3,
    DigestBytes = 4,
    SignBytes = 5,
    ValidSignature = 6
};


// CLASS DEFINITION

/**
 * This class implements a hardware security module (HSM) and is designed to run on a
 * hardened processor that is tamper resistant and shielded from EMF monitoring. The
 * HSM provides the public-private key cryptographic functions needed to digitally
 * sign and verify bytes.
 *
 * The functions are split into two groups, the first which do not require access to
 * the private key:
 *  * digestBytes(bytes, size) => digest
 *  * validSignature(bytes, size, signature, aPublicKey) => isValid?
 *
 * and the second group which do involve the private key which has been encrypted
 * using a secret key that is passed in from a mobile device:
 *  * generateKeys(secretKey) => publicKey
 *  * signBytes(secretKey, bytes, size) => signature
 *  * eraseKeys() => success?
 *
 * The private key is encrypted using the secret key as follows:
 *    secretKey XOR privateKey => encryptedKey
 *
 * The private key can then be decryped using the secret key as needed:
 *    secretKey XOR encryptedKey => privateKey
 *
 * Neither the secret key nor the private key are maintained in the HSM so the private
 * key is completely secure.
 *
 * The process for signing bytes and verifying the resulting signatures requires
 * several steps:
 *  1  const uint8_t* bytes;  // the bytes to be signed
 *  2. const uint8_t* secretKey;  // stored on a mobile device
 *  3. const uint8_t* signature = hsm->signBytes(secretKey, bytes, size);
 *  4. bool isValid = hsm->validSignature(bytes, size, signature, aPublicKey);
 *
 * If the public key corresponds to the private key that signed the bytes then the
 * signature is valid.
 *
 * The process for generating new keys requires several steps:
 *  1. const uint8_t* secretKey = a new random byte array containing KEY_SIZE bytes
 *  2. const uint8_t* publicKey = hsm->generateKeys(secretKey);
 *  3. const char* certificate = construct a new certificate containing the public key
 *  4. const uint8_t* signature = hsm->signBytes(secretKey, certificate);
 *  5. const char* certificate = append the signature to the certificate;
 *  6. publish the signed certificate to the cloud for others to download
 *
 * Note, this process can be repeated periodically to protect older keys. Anything
 * signed with the older keys can still be validated using the corresponding public
 * certificates available from the cloud, but the older keys will have been erased
 * so that no one else can use them.
 *
 * When regenerating keys, step two above has an extra argument that is passed:
 *  2. const uint8_t* publicKey = hsm->generateKeys(newSecretKey, existingSecretKey);
 *
 * This allows the HSM to decrypt and verify the existing private key before
 * replacing it with a new private key. It also saves off the existing encrypted
 * key so that the existing private key can be used to sign the new certificate
 * in step four above:
 *  4. const uint8_t* signature = hsm->signBytes(existingSecretKey, certificate);
 *
 * Having each new certificate signed with the previous private key allows the
 * certificates to be managed on a key chain where each certificate is signed by the
 * private key associated with the previous certificate. Only the first certificate
 * is signed using its own private key.
 */
class HSM final {

  public:
    /**
     * This constructor "rehydrates" a hardware security module (HSM) on power up.
     */
    HSM();

    /**
     * This destructor "dehydrates" a hardware security module (HSM) on power down. It
     * erases the keys from the processor memory and stops processing requests.
     */
   ~HSM();

    /**
     * This function is passed, from a mobile device, a new secret key. It generates a
     * new public-private key pair and uses the new secret key to encrypt the new
     * private key using the XOR operation to generate a new encrypted key. Then the
     * new public key and the new encrypted key are saved, and the new secret key and
     * new private key are erased from the HSM. The new public key is returned from
     * the function.
     *
     * It is the responsibilty of the calling program to 'delete []' the public key
     * once it has finished with it.
     */
    const uint8_t* generateKeys(uint8_t newSecretKey[KEY_SIZE]);

    /**
     * This function is passed, from a mobile device, an existing secret key and a
     * new secret key. It saves the existing public and encrypted keys and then
     * generates a new public-private key pair. It uses the new secret key to encrypt
     * the new private key using the XOR operation to generate a new encrypted key.
     * Then the new public key and the new encrypted key are saved, and the existing
     * and new secret keys and the new private key are erased from the HSM. The new
     * public key is returned from the function.
     *
     * It is the responsibilty of the calling program to 'delete []' the public key
     * once it has finished with it.
     */
    const uint8_t* rotateKeys(uint8_t existingSecretKey[KEY_SIZE],uint8_t newSecretKey[KEY_SIZE]);

    /**
     * This function erases from the processor memory all current and previous keys.
     * This function should be called when the mobile device associated with the HSM
     * has been lost or stolen. The function returns a value describing whether or
     * not the keys were successfully erased.
     */
    bool eraseKeys();

    /**
     * This function is passed, from a mobile device, some bytes. It generates, for the
     * bytes, a digest that can be used later to verify that the bytes have not changed.
     * No keys are used to generate the digest. The digest is returned from the function.
     * It is the responsibilty of the calling program to 'delete []' the digest once it
     * has finished with it.
     */
    const uint8_t* digestBytes(const uint8_t* bytes, const size_t size);

    /**
     * This function is passed, from a mobile device, a secret key and some bytes to
     * be digitally signed. The secret key is used to reconstruct the private key using
     * the encrypted key and verifying it with the public key. If the keys are valid,
     * the private key is used to digitally sign the bytes and the secret key and
     * the private key are erased from the HSM. The digital signature for the bytes
     * is returned from the function.
     *
     * If there is a previous encrypted key, that key is used one last time and then erased
     * from the HSM. This is a special case that occurs only when the public certificate
     * for a new key is being signed by the previous private key to prove it belongs
     * on the same key chain.
     *
     * It is the responsibilty of the calling program to 'delete []' the signature
     * once it has finished with it.
     */
    const uint8_t* signBytes(uint8_t secretKey[KEY_SIZE], const uint8_t* bytes, const size_t size);

    /**
     * This function is passed, from a mobile device, some bytes, and a digital signature
     * that may belong to the bytes, and a public key that will be used to validate
     * the signature. The function checks to see whether or not the digital signature was
     * created for the bytes using the private key associated with the specified public
     * key. Note, the specified public key may or may not be the same public key that is
     * associated with the private key maintained by the HSM. The function returns a value
     * describing whether or not the digital signature is valid.
     */
    bool validSignature(
        const uint8_t aPublicKey[KEY_SIZE],
        const uint8_t signature[SIG_SIZE],
        const uint8_t* bytes,
        const size_t size
    );

  private:
    static const size_t BUFFER_SIZE = 4 * KEY_SIZE + 1;

    /**
     * This function checks to see if the specified request is allowed in the current
     * state.
     */
    bool validRequest(RequestType request);

    /**
     * This function transitions the HSM to the next state based on the specified
     * request.
     */
    void transitionState(RequestType request);

    State currentState = Invalid;

    /**
     * This function stores any persisted key state to the flash memory based file
     * system.
     */
    void storeState();

    /**
     * This function loads any persisted key state from the flash memory based file
     * system.
     */
    void loadState();

    uint8_t buffer[BUFFER_SIZE] = { 0 };
    uint8_t* publicKey = 0;
    uint8_t* encryptedKey = 0;
    uint8_t* previousPublicKey = 0;
    uint8_t* previousEncryptedKey = 0;
    bool hasButton = false;

};

#endif
