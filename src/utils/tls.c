#include "tls.h"
#include "logger.h"
#include <openssl/err.h>
#include <openssl/ssl.h>


void tls_init_library(void) {
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  ERR_load_BIO_strings();
  ERR_load_crypto_strings();
  LOG_INFO("OpenSSL library initialized");
}

SSL_CTX *tls_create_context(const char *cert_file, const char *key_file) {
  const SSL_METHOD *method;
  SSL_CTX *ctx;

  method = TLS_server_method();
  ctx = SSL_CTX_new(method);
  if (!ctx) {
    LOG_ERROR("Unable to create SSL context");
    ERR_print_errors_fp(stderr); // Todo: redirect to logger?
    return NULL;
  }

  // Set options
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION); // TLS 1.2+

  if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
    LOG_ERROR("Failed to load certificate file: %s", cert_file);
    ERR_print_errors_fp(stderr);
    SSL_CTX_free(ctx);
    return NULL;
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
    LOG_ERROR("Failed to load private key file: %s", key_file);
    ERR_print_errors_fp(stderr);
    SSL_CTX_free(ctx);
    return NULL;
  }

  if (!SSL_CTX_check_private_key(ctx)) {
    LOG_ERROR("Private key does not match the certificate public key");
    SSL_CTX_free(ctx);
    return NULL;
  }

  LOG_INFO("TLS context created with cert: %s", cert_file);
  return ctx;
}

void tls_cleanup(void) { EVP_cleanup(); }
