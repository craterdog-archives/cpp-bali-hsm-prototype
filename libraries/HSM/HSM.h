/************************************************************************
 * Copyright (c) Crater Dog Technologies(TM).  All Rights Reserved.     *
 ************************************************************************/
#ifndef HSM_H
#define HSM_H

#include <inttypes.h>
#include <stddef.h>

/**
 * This class implements a hardware security module (HSM) and is designed to run on a
 * hardened processor that is tamper resistant and shielded from EMF monitoring. The
 * HSM provides the public-private key cryptographic functions needed to digitally
 * sign and verify messages.
 *
 * The functions are split into two groups, the first which do not require access to
 * the private key:
 *  * digestMessage(message) => digest
 *  * validSignature(aPublicKey, message, signature) => boolean
 *
 * and the second group which do involve the private key which has been encrypted
 * using a secret key that is passed in from a mobile device:
 *  * generateKeys(secretKey) => publicKey
 *  * signMessage(secretKey, message => signature
 *  * eraseKeys()
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
 * The process for signing messages and verifying the resulting signatures requires
 * several steps:
 *  1  const char* message;  // the message to be signed
 *  2. const uint8_t* secretKey;  // stored on a mobile device
 *  3. const uint8_t* signature = hsm->signMessage(secretKey, message);
 *  4. bool isValid = hsm->validSignature(aPublicKey, message, signature);
 *
 * If the public key corresponds to the private key that signed the message then the
 * signature is valid.
 *
 * The process for generating new keys requires several steps:
 *  1. const uint8_t* secretKey = a new random byte array containing 32 bytes
 *  2. const uint8_t* publicKey = hsm->generateKeys(secretKey);
 *  3. const char* certificate = construct a new certificate containing the public key
 *  4. const uint8_t* signature = hsm->signMessage(secretKey, certificate);
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
 *  4. const uint8_t* signature = hsm->signMessage(existingSecretKey, certificate);
 *
 * Having each new certificate signed with the previous private key allows the
 * certificates to be managed on a key chain where each certificate is signed by the
 * private key associated with the previous certificate. Only the first certificate
 * is signed using its own private key.
 */
class HSM final {

  public:
    /**
     * This constructor "rehydrates" a hardware security module (HSM) on power up. It
     * loads the keys from the EEPROM drive and begins processing requests.
     */
    HSM();

    /**
     * This destructor "dehydrates" a hardware security module (HSM) on power down. It
     * erases the keys from the processor memory and stops processing requests.
     */
   ~HSM();

    /**
     * This function is passed, from a mobile device, a message. It generates, for the
     * message, a digest that can be used later to verify that the content of the message
     * has not changed. No keys are used to generate the digest. The message digest is
     * returned from the function. It is the responsibilty of the calling program to
     * 'delete []' the digest once it has finished with it.
     */
    const uint8_t* digestMessage(const char* message);

    /**
     * This function is passed, from a mobile device, a public key, a message, and a
     * digital signature that may or may not belong to the message. It checks to see
     * whether or not the digital signature was created for the message using the
     * private key associated with the specified public key. Note, the specified public
     * key may or may not be the same public key that is associated with the private key
     * maintained by the HSM. The function returns whether or not the digital signature
     * is valid.
     */
    bool validSignature(const uint8_t aPublicKey[32], const char* message, const uint8_t signature[64]);

    /**
     * This function is passed, from a mobile device, a new secret key. It generates a
     * new public-private key pair and uses the new secret key to encrypt the new
     * private key using the XOR operation to generate a new encrypted key. Then the
     * new public key and the new encrypted key are saved, and the new secret key and
     * new private key are erased from the HSM. The new public key is returned from
     * the function.
     *
     * If an optional existing secret key is passed as well, it is used to reconstruct
     * the existing private key using the existing encrypted key and verifying it with
     * the existing public key. If the keys are valid, the existing encrypted key and
     * existing public key are saved as oldX and oldP,and the new versions of the keys
     * are generated as described above.
     *
     * It is the responsibilty of the calling program to 'delete []' the public key
     * once it has finished with it.
     */
    const uint8_t* generateKeys(uint8_t newSecretKey[32], uint8_t secretKey[32] = 0);

    /**
     * This function is passed, from a mobile device, a secret key and a message to be
     * digitally signed. The secret key is used to reconstruct the private key using
     * the encrypted key and verifying it with the public key. If the keys are valid,
     * the private key is used to digitally sign the message and the secret key and
     * the private key are erased from the HSM. The digital signature for the message
     * is returned from the function.
     *
     * If there is an old encrypted key, that key is used one last time and then erased
     * from the HSM. This is a special case that occurs only when the public certificate
     * for a new key is being signed by the previous private key to prove it belongs
     * on the same key chain.
     *
     * It is the responsibilty of the calling program to 'delete []' the signature
     * once it has finished with it.
     */
    const uint8_t* signMessage(uint8_t secretKey[32], const char* message);

    /**
     * This function erases from the processor memory all current and past keys and
     * erases from the EEPROM drive the current keys. This function should be called
     * when the mobile device associated with the HSM has been lost or stolen.
     */
    void eraseKeys();

  private:
    bool transitioning = false;
    uint8_t publicKey[32];
    uint8_t encryptedKey[32];
    uint8_t oldPublicKey[32];
    uint8_t oldEncryptedKey[32];

};

#endif
