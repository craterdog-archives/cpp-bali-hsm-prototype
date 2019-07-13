#include "Formatter.h"
#include <string.h>


// CITATION TEMPLATES

const char* CITATION_BEFORE_TIMESTAMP =
    "[\n"
    "    $protocol: v1\n"
    "    $timestamp: ";
const char* CITATION_BEFORE_TAG =
    "\n"
    "    $tag: ";
const char* CITATION_BEFORE_VERSION =
    "\n"
    "    $version: ";
const char* CITATION_BEFORE_DIGEST =
    "\n"
    "    $digest: ";
const char* CITATION_AFTER_DIGEST =
    "\n"
    "](\n"
    "    $type: /bali/notary/Citation/v1\n"
    ")";


// CERTIFICATE TEMPLATES

const char* CERTIFICATE_BEFORE_TIMESTAMP =
    "[\n"
    "    $protocol: v1\n"
    "    $timestamp: ";
const char* CERTIFICATE_BEFORE_ACCOUNT_ID =
    "\n"
    "    $accountId: ";
const char* CERTIFICATE_BEFORE_PUBLIC_KEY =
    "\n"
    "    $publicKey: ";
const char* CERTIFICATE_BEFORE_TAG =
    "\n"
    "](\n"
    "    $type: /bali/notary/Certificate/v1\n"
    "    $tag: ";
const char* CERTIFICATE_BEFORE_VERSION =
    "\n"
    "    $version: ";
const char* CERTIFICATE_BEFORE_PREVIOUS =
    "\n"
    "    $permissions: /bali/permissions/public/v1\n"
    "    $previous: ";
const char* CERTIFICATE_AFTER_PREVIOUS =
    "\n"
    ")";


// DOCUMENT TEMPLATES

const char* DOCUMENT_BEFORE_COMPONENT =
    "[\n"
    "    $component: ";
const char* DOCUMENT_BEFORE_TIMESTAMP =
    "\n"
    "    $protocol: v1\n"
    "    $timestamp: ";
const char* DOCUMENT_BEFORE_CERTIFICATE =
    "\n"
    "    $certificate: ";
const char* DOCUMENT_BEFORE_SIGNATURE =
    "\n"
    "    $signature: ";
const char* DOCUMENT_AFTER_SIGNATURE =
    "\n"
    "](\n"
    "    $type: /bali/notary/Document/v1\n"
    ")";


// PUBLIC METHODS

const char* Formatter::formatCitation(
    const char *timestamp,
    const char *tag,
    const char *version,
    const char *digest
) {
    // calculate the total length of the new citation including the terminating null
    size_t length =
        strlen(CITATION_BEFORE_TIMESTAMP) + strlen(timestamp) +
        strlen(CITATION_BEFORE_TAG) + strlen(tag) +
        strlen(CITATION_BEFORE_VERSION) + strlen(version) +
        strlen(CITATION_BEFORE_DIGEST) + strlen(digest) +
        strlen(CITATION_AFTER_DIGEST) + 1;  // include terminating null

    // allocate space for the result
    char *citation = new char[length];

    // prepend the timestamp part
    char *pointer = citation;
    strcpy(pointer, CITATION_BEFORE_TIMESTAMP);
    pointer += strlen(CITATION_BEFORE_TIMESTAMP);
    strcpy(pointer, timestamp);
    pointer += strlen(timestamp);

    // append the tag part
    strcpy(pointer, CITATION_BEFORE_TAG);
    pointer += strlen(CITATION_BEFORE_TAG);
    strcpy(pointer, tag);
    pointer += strlen(tag);

    // append the version part
    strcpy(pointer, CITATION_BEFORE_VERSION);
    pointer += strlen(CITATION_BEFORE_VERSION);
    strcpy(pointer, version);
    pointer += strlen(version);

    // append the digest part
    strcpy(pointer, CITATION_BEFORE_DIGEST);
    pointer += strlen(CITATION_BEFORE_DIGEST);
    strcpy(pointer, digest);
    pointer += strlen(digest);

    // append the rest of the citation
    strcpy(pointer, CITATION_AFTER_DIGEST);
    return citation;
}


const char* Formatter::formatCertificate(
    const char *timestamp,
    const char *accountId,
    const char *publicKey,
    const char *tag,
    const char *version,
    const char *previous
) {
    // calculate the total length of the new certificate including the terminating null
    size_t length =
        strlen(CERTIFICATE_BEFORE_TIMESTAMP) + strlen(timestamp) +
        strlen(CERTIFICATE_BEFORE_ACCOUNT_ID) + strlen(accountId) +
        strlen(CERTIFICATE_BEFORE_PUBLIC_KEY) + strlen(publicKey) +
        strlen(CERTIFICATE_BEFORE_TAG) + strlen(tag) +
        strlen(CERTIFICATE_BEFORE_VERSION) + strlen(version) +
        strlen(CERTIFICATE_BEFORE_PREVIOUS) + strlen(previous) +
        strlen(CERTIFICATE_AFTER_PREVIOUS) + 1;  // include terminating null

    // allocate space for the result
    char *certificate = new char[length];

    // prepend the timestamp part
    char *pointer = certificate;
    strcpy(pointer, CERTIFICATE_BEFORE_TIMESTAMP);
    pointer += strlen(CERTIFICATE_BEFORE_TIMESTAMP);
    strcpy(pointer, timestamp);
    pointer += strlen(timestamp);

    // append the account Id part
    strcpy(pointer, CERTIFICATE_BEFORE_ACCOUNT_ID);
    pointer += strlen(CERTIFICATE_BEFORE_ACCOUNT_ID);
    strcpy(pointer, accountId);
    pointer += strlen(accountId);

    // append the public key part
    strcpy(pointer, CERTIFICATE_BEFORE_PUBLIC_KEY);
    pointer += strlen(CERTIFICATE_BEFORE_PUBLIC_KEY);
    strcpy(pointer, publicKey);
    pointer += strlen(publicKey);

    // append the tag part
    strcpy(pointer, CERTIFICATE_BEFORE_TAG);
    pointer += strlen(CERTIFICATE_BEFORE_TAG);
    strcpy(pointer, tag);
    pointer += strlen(tag);

    // append the version part
    strcpy(pointer, CERTIFICATE_BEFORE_VERSION);
    pointer += strlen(CERTIFICATE_BEFORE_VERSION);
    strcpy(pointer, version);
    pointer += strlen(version);

    // append the previous version part
    strcpy(pointer, CERTIFICATE_BEFORE_PREVIOUS);
    pointer += strlen(CERTIFICATE_BEFORE_PREVIOUS);
    strcpy(pointer, previous);
    pointer += strlen(previous);

    // append the rest of the certificate
    strcpy(pointer, CERTIFICATE_AFTER_PREVIOUS);
    return certificate;
}


const char* Formatter::formatDocument(
    const char *component,
    const char *timestamp,
    const char *certificate,
    const char *signature
) {
    // calculate the total length of the new document including the terminating null
    size_t length =
        strlen(DOCUMENT_BEFORE_COMPONENT) + strlen(component) +
        strlen(DOCUMENT_BEFORE_TIMESTAMP) + strlen(timestamp) +
        strlen(DOCUMENT_BEFORE_CERTIFICATE) + strlen(certificate) +
        strlen(DOCUMENT_BEFORE_SIGNATURE) + strlen(signature) +
        strlen(DOCUMENT_AFTER_SIGNATURE) + 1;

    // allocate space for the result
    char *document = new char[length];

    // prepend the component part
    char *pointer = document;
    strcpy(pointer, DOCUMENT_BEFORE_COMPONENT);
    pointer += strlen(DOCUMENT_BEFORE_COMPONENT);
    strcpy(pointer, component);
    pointer += strlen(component);

    // append the timestamp part
    strcpy(pointer, DOCUMENT_BEFORE_TIMESTAMP);
    pointer += strlen(DOCUMENT_BEFORE_TIMESTAMP);
    strcpy(pointer, timestamp);
    pointer += strlen(timestamp);

    // append the certificate part
    strcpy(pointer, DOCUMENT_BEFORE_CERTIFICATE);
    pointer += strlen(DOCUMENT_BEFORE_CERTIFICATE);
    strcpy(pointer, certificate);
    pointer += strlen(certificate);

    // append the signature part
    strcpy(pointer, DOCUMENT_BEFORE_SIGNATURE);
    pointer += strlen(DOCUMENT_BEFORE_SIGNATURE);
    strcpy(pointer, signature);
    pointer += strlen(signature);

    // append the rest of the document
    strcpy(pointer, DOCUMENT_AFTER_SIGNATURE);

    return document;
}


const char* Formatter::indentComponent(const char *component) {
    size_t length = strlen(component);

    // count the number of lines
    size_t count = 1;
    for (size_t i = 0; i < length; i++) {
        if (component[i] == '\n') count++;
    }

    // allocate space for the result include the terminating null
    char *indented = new char[length + 4 * count + 1];

    // indent each new line
    size_t index = 0;
    for (size_t j = 0; j < length; j++) {
        indented[index++] = component[j];
        if (component[j] == '\n') {
            indented[index++] = ' ';
            indented[index++] = ' ';
            indented[index++] = ' ';
            indented[index++] = ' ';
        }
    }

    // add the terminating null
    indented[index] = '\0';

    return indented;
}

