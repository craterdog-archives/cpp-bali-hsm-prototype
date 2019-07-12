#ifndef FORMATTER_H
#define FORMATTER_H

#include <inttypes.h>
#include <stddef.h>


class Formatter final {
public:
    static const char* formatCitation(
        const char *timestamp,
        const char *tag,
        const char *version,
        const char *digest
    );

    static const char* formatCertificate(
        const char *timestamp,
        const char *accountId,
        const char *publicKey,
        const char *tag,
        const char *version,
        const char *previous
    );

    static const char* formatDocument(
        const char *component,
        const char *timestamp,
        const char *certificate,
        const char *signature
    );

    static const char* indentComponent(const char* component);

private:
    Formatter();
   ~Formatter();
};

#endif
