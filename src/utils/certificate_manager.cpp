#include "certificate_manager.hpp"
#include <iostream>

#ifdef MITMQTT_HAS_SSL
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#endif

namespace mitmqtt {
namespace utils {

CertificateManager::CertificateManager()
    : serverCtx_(nullptr), clientCtx_(nullptr) {
  initializeOpenSSL();
}

CertificateManager::~CertificateManager() {
#ifdef MITMQTT_HAS_SSL
  if (serverCtx_) {
    SSL_CTX_free(serverCtx_);
  }
  if (clientCtx_) {
    SSL_CTX_free(clientCtx_);
  }
#endif
  cleanupOpenSSL();
}

void CertificateManager::initializeOpenSSL() {
#ifdef MITMQTT_HAS_SSL
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
#endif
}

void CertificateManager::cleanupOpenSSL() {
#ifdef MITMQTT_HAS_SSL
  EVP_cleanup();
#endif
}

bool CertificateManager::loadCACertificate(const std::string &caFile) {
#ifdef MITMQTT_HAS_SSL
  // Create client context if it doesn't exist
  if (!clientCtx_) {
    clientCtx_ = SSL_CTX_new(TLS_client_method());
    if (!clientCtx_) {
      std::cerr << "Failed to create SSL client context" << std::endl;
      return false;
    }
  }

  // Load CA certificate
  if (SSL_CTX_load_verify_locations(clientCtx_, caFile.c_str(), nullptr) != 1) {
    std::cerr << "Failed to load CA certificate: " << caFile << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  std::cout << "Loaded CA certificate: " << caFile << std::endl;
  return true;
#else
  (void)caFile;
  std::cerr << "SSL support not compiled in" << std::endl;
  return false;
#endif
}

bool CertificateManager::loadServerCertificate(const std::string &certFile,
                                               const std::string &keyFile) {
#ifdef MITMQTT_HAS_SSL
  // Create server context if it doesn't exist
  if (!serverCtx_) {
    serverCtx_ = SSL_CTX_new(TLS_server_method());
    if (!serverCtx_) {
      std::cerr << "Failed to create SSL server context" << std::endl;
      return false;
    }
  }

  // Load certificate
  if (SSL_CTX_use_certificate_file(serverCtx_, certFile.c_str(),
                                   SSL_FILETYPE_PEM) != 1) {
    std::cerr << "Failed to load server certificate: " << certFile << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  // Load private key
  if (SSL_CTX_use_PrivateKey_file(serverCtx_, keyFile.c_str(),
                                  SSL_FILETYPE_PEM) != 1) {
    std::cerr << "Failed to load server private key: " << keyFile << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  // Verify private key
  if (SSL_CTX_check_private_key(serverCtx_) != 1) {
    std::cerr << "Server private key does not match certificate" << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  std::cout << "Loaded server certificate and key" << std::endl;
  return true;
#else
  (void)certFile;
  (void)keyFile;
  std::cerr << "SSL support not compiled in" << std::endl;
  return false;
#endif
}

bool CertificateManager::loadClientCertificate(const std::string &certFile,
                                               const std::string &keyFile) {
#ifdef MITMQTT_HAS_SSL
  // Create client context if it doesn't exist
  if (!clientCtx_) {
    clientCtx_ = SSL_CTX_new(TLS_client_method());
    if (!clientCtx_) {
      std::cerr << "Failed to create SSL client context" << std::endl;
      return false;
    }
  }

  // Load certificate
  if (SSL_CTX_use_certificate_file(clientCtx_, certFile.c_str(),
                                   SSL_FILETYPE_PEM) != 1) {
    std::cerr << "Failed to load client certificate: " << certFile << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  // Load private key
  if (SSL_CTX_use_PrivateKey_file(clientCtx_, keyFile.c_str(),
                                  SSL_FILETYPE_PEM) != 1) {
    std::cerr << "Failed to load client private key: " << keyFile << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  // Verify private key
  if (SSL_CTX_check_private_key(clientCtx_) != 1) {
    std::cerr << "Client private key does not match certificate" << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  std::cout << "Loaded client certificate and key" << std::endl;
  return true;
#else
  (void)certFile;
  (void)keyFile;
  std::cerr << "SSL support not compiled in" << std::endl;
  return false;
#endif
}

SSL_CTX *CertificateManager::getServerContext() { return serverCtx_; }

SSL_CTX *CertificateManager::getClientContext() { return clientCtx_; }

bool CertificateManager::generateSelfSignedCertificate(
    const std::string &certFile, const std::string &keyFile) {
#ifdef MITMQTT_HAS_SSL
  std::cout << "Generating self-signed CA certificate..." << std::endl;

  // Generate RSA key pair using EVP_PKEY_Q_keygen (OpenSSL 3.x)
  EVP_PKEY *pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", (size_t)2048);
  if (!pkey) {
    std::cerr << "Failed to generate RSA key pair" << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }
  std::cout << "RSA key pair generated successfully" << std::endl;

  // Create X509 certificate
  X509 *x509 = X509_new();
  if (!x509) {
    std::cerr << "Failed to create X509 certificate" << std::endl;
    EVP_PKEY_free(pkey);
    return false;
  }

  // Set certificate version (v3)
  X509_set_version(x509, 2);

  // Set serial number
  ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

  // Set validity period (10 years)
  X509_gmtime_adj(X509_get_notBefore(x509), 0);
  X509_gmtime_adj(X509_get_notAfter(x509), 10L * 365L * 24L * 60L * 60L);

  // Set public key
  X509_set_pubkey(x509, pkey);

  // Set subject name (CA info)
  X509_NAME *name = X509_get_subject_name(x509);
  X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"US", -1,
                             -1, 0);
  X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC,
                             (unsigned char *)"Security", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC,
                             (unsigned char *)"MITMqtt Proxy", -1, -1, 0);
  X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                             (unsigned char *)"MITMqtt CA", -1, -1, 0);

  // Self-signed: issuer = subject
  X509_set_issuer_name(x509, name);

  // Add CA constraint extension
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, x509, x509, nullptr, nullptr, 0);

  X509_EXTENSION *ext =
      X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints,
                          const_cast<char *>("critical,CA:TRUE"));
  if (ext) {
    X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);
  }

  // Add key usage extension
  ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_key_usage,
                            const_cast<char *>("critical,keyCertSign,cRLSign"));
  if (ext) {
    X509_add_ext(x509, ext, -1);
    X509_EXTENSION_free(ext);
  }

  // Sign the certificate with its own private key (self-signed)
  if (!X509_sign(x509, pkey, EVP_sha256())) {
    std::cerr << "Failed to sign certificate" << std::endl;
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return false;
  }
  std::cout << "Certificate signed successfully" << std::endl;

  // Write private key to file using BIO (Windows compatible)
  BIO *keyBio = BIO_new_file(keyFile.c_str(), "wb");
  if (!keyBio) {
    std::cerr << "Failed to open key file for writing: " << keyFile
              << std::endl;
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return false;
  }
  if (!PEM_write_bio_PrivateKey(keyBio, pkey, nullptr, nullptr, 0, nullptr,
                                nullptr)) {
    std::cerr << "Failed to write private key" << std::endl;
    BIO_free(keyBio);
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return false;
  }
  BIO_free(keyBio);
  std::cout << "Private key saved to: " << keyFile << std::endl;

  // Write certificate to file using BIO (Windows compatible)
  BIO *certBio = BIO_new_file(certFile.c_str(), "wb");
  if (!certBio) {
    std::cerr << "Failed to open cert file for writing: " << certFile
              << std::endl;
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return false;
  }
  if (!PEM_write_bio_X509(certBio, x509)) {
    std::cerr << "Failed to write certificate" << std::endl;
    BIO_free(certBio);
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return false;
  }
  BIO_free(certBio);
  std::cout << "Certificate saved to: " << certFile << std::endl;

  // Cleanup
  X509_free(x509);
  EVP_PKEY_free(pkey);

  std::cout << "Self-signed CA certificate generated successfully!"
            << std::endl;
  std::cout
      << "Add this CA certificate to your devices to enable TLS interception."
      << std::endl;

  return true;
#else
  (void)certFile;
  (void)keyFile;
  std::cerr << "SSL support not compiled in" << std::endl;
  return false;
#endif
}

} 
} 
