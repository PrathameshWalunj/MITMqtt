#pragma once

#include <string>

// force include OpenSSL headers because Boost.Asio SSL requires them
#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace mitmqtt {
namespace utils {

// Certificate management for TLS/SSL connections
class CertificateManager {
public:
  CertificateManager();
  ~CertificateManager();

  
  bool loadCACertificate(const std::string &caFile);
  bool loadServerCertificate(const std::string &certFile,
                             const std::string &keyFile);
  bool loadClientCertificate(const std::string &certFile,
                             const std::string &keyFile);

  // Get SSL context
  SSL_CTX *getServerContext();
  SSL_CTX *getClientContext();

  // Generate self-signed certificate
  bool generateSelfSignedCertificate(const std::string &certFile,
                                     const std::string &keyFile);

private:
  SSL_CTX *serverCtx_;
  SSL_CTX *clientCtx_;

  void initializeOpenSSL();
  void cleanupOpenSSL();
};

} 
} 
