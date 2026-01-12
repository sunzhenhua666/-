#ifndef TLS_H
#define TLS_H

#include <openssl/err.h>
#include <openssl/ssl.h>


// Initialize OpenSSL Library
void tls_init_library(void);

// Create SSL Context (Server Mode)
// Returns NULL on failure
SSL_CTX *tls_create_context(const char *cert_file, const char *key_file);

// Cleanup Library
void tls_cleanup(void);

#endif // TLS_H
