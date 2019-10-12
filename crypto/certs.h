// Copyright (C) 2019 by Jakub Wojciech

// This file is part of Lelo Remote Music Player.

// Lelo Remote Music Player is free software: you can redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.

// Lelo Remote Music Player is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with Lelo Remote Music Player. If not, see
// <https://www.gnu.org/licenses/>.

#ifndef LRM_CERTS_H_
#define LRM_CERTS_H_

#include <cstring>
#include <memory>
#include <string_view>
#include <sstream>
#include <unordered_map>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "filesystem.h"
#include "Util.h"

#define int_error(msg) handle_error(__FILE__, __LINE__, (msg))

namespace lrm::crypto::certs {
namespace {
static constexpr int KEY_BITS = 2048;
static constexpr int KEY_EXPONENT = RSA_F4;
static const EVP_CIPHER* KEY_PEM_CIPHER = EVP_aes_192_gcm();
static const EVP_MD* KEY_SIGN_DIGEST = EVP_sha256();

int print_errors_cb(const char* str, size_t len, void* arg) {
  std::stringstream* ss = static_cast<std::stringstream*>(arg);
  ss->write(str, len);

  return 0;
}

void handle_error(std::string_view file, int line, std::string_view msg) {
  std::stringstream ss;
  ss << msg << '\n';

  ERR_print_errors_cb(&print_errors_cb, static_cast<void*>(&ss));

  throw std::runtime_error(ss.str());
}

std::basic_string<unsigned char> str_to_uc(std::string_view str) {
  std::basic_string<unsigned char> result(str.size(), ' ');
  std::memcpy(result.data(), str.data(), str.size());

  return result;
}
}

class RsaKeyPair {
 public:
  RsaKeyPair() : pkey_(EVP_PKEY_new(), &EVP_PKEY_free) {
    if (not pkey_) int_error("Failed to create EVP_PKEY object");
    RSA* rsa = RSA_new();
    if (not rsa) int_error("Failed to create RSA object");
    BIGNUM* exponent = BN_new();
    if (not exponent) int_error("Failed to create BIGNUM object");

    if (not BN_dec2bn(&exponent, std::to_string(KEY_EXPONENT).c_str()))
      int_error("Error converting int to BIGNUM");
    if (not RSA_generate_key_ex(rsa, KEY_BITS, exponent, nullptr))
      int_error("Error generating RSA key pair");
    BN_free(exponent);
    if (not EVP_PKEY_assign_RSA(pkey_.get(), rsa))
      int_error("Error assigning RSA to PKEY");
  }

  /// Construct RsaKeyPair from PEM file storing RSA private key
  explicit RsaKeyPair(std::string_view filename, std::string password = "")
      : pkey_(nullptr, &EVP_PKEY_free) {
    FILE* fp = nullptr;
    if ((not Util::file_exists(filename)) or
        (not (fp = std::fopen(filename.data(), "r")))) {
      std::stringstream ss;
      ss << __PRETTY_FUNCTION__ << "Error reading private key file: "
         << filename;
      throw std::invalid_argument(ss.str());
    }

    void* pass_p = static_cast<void*>(password.data());
    EVP_PKEY* pkey = PEM_read_PrivateKey(
        fp, nullptr, nullptr, password.empty() ? nullptr : pass_p);
    std::fclose(fp);
    if (not pkey) int_error("Error reading private key file");

    pkey_.reset(pkey);
  }
  RsaKeyPair(const RsaKeyPair&) = delete;
  RsaKeyPair(RsaKeyPair&&) = delete;
  RsaKeyPair& operator=(const RsaKeyPair&) = delete;
  RsaKeyPair& operator=(RsaKeyPair&&) = delete;

  EVP_PKEY* Get() {
    return pkey_.get();
  }

  void SerializePrivateKey(std::string_view filename,
                           std::string_view password = "") const {
    FILE* fp = std::fopen(filename.data(), "w");
    if (not fp) {
      throw std::runtime_error(
          std::string("Error opening file '")
          + filename.data() + "': " + std::strerror(errno));
    }

    const EVP_CIPHER* enc = password.empty() ? nullptr : KEY_PEM_CIPHER;

    std::basic_string<unsigned char> kstr;
    if (not password.empty()) {
      auto kstr = str_to_uc(password);
    }

    int result = PEM_write_PrivateKey(fp, pkey_.get(), enc,
                                      kstr.empty() ? nullptr : kstr.data(),
                                      password.size(), nullptr, nullptr);
    std::fclose(fp);

    if (not result) int_error("Error writing private key to file");
  }

  void SerializePublicKey(std::string_view filename) const {
    FILE* fp = std::fopen(filename.data(), "w");
    if (not fp) {
      throw std::runtime_error(
          std::string("Error opening file '")
          + filename.data() + "': " + std::strerror(errno));
    }

    int result = PEM_write_PUBKEY(fp, pkey_.get());
    std::fclose(fp);
    if (not result) int_error("Error writing public key to file");
  }

 private:
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_;
};

class RsaCertificate {
 public:
  using CertNameMap =
      std::unordered_map<std::string, std::string>;

  RsaCertificate(RsaKeyPair& key_pair, const CertNameMap name_entries,
                 unsigned int expiration_days = 365)
      : cert_(X509_new(), &X509_free) {
    if (not cert_) int_error("Failed to create X509 object");
    if (not X509_set_version(cert_.get(), 2L))
      int_error("Error setting certificate version");

    if (not X509_gmtime_adj(X509_get_notBefore(cert_.get()), 0))
      int_error("Error setting beginning time of the certificate");
    if (not X509_gmtime_adj(X509_get_notAfter(cert_.get()),
                            60 * 60 * 24 * expiration_days))
      int_error("Error setting ending time of the certificate");

    if (not X509_set_pubkey(cert_.get(), key_pair.Get()))
      int_error("Error setting public key of the certificate");

    X509_NAME* name = X509_NAME_new();
    if (not name) int_error("Failed to create X509_NAME object");

    for(const auto& [key, value] : name_entries) {
      auto value_uc = str_to_uc(value);
      if (not X509_NAME_add_entry_by_txt(
              name, key.c_str(), MBSTRING_ASC,
              value_uc.c_str(), value_uc.size(), -1, 0)) {
        X509_NAME_free(name);
        int_error("Error adding entry '" + key + "' to X509_NAME object");
      }
    }
    if (not X509_set_subject_name(cert_.get(), name)) {
      X509_NAME_free(name);
      int_error("Error setting subject name of certificate");
    }
    X509_NAME_free(name);
  }

  /// Construct RsaCertificate from PEM file storing X509 certificate
  explicit RsaCertificate(std::string_view filename)
      : cert_(nullptr, &X509_free) {
    FILE* fp = nullptr;
    if ((not Util::file_exists(filename)) or
        (not (fp = std::fopen(filename.data(), "r")))) {
      std::stringstream ss;
      ss << __PRETTY_FUNCTION__ << "Error reading certificate file: "
         << filename;
      throw std::invalid_argument(ss.str());
    }

    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    std::fclose(fp);
    if (not cert) int_error("Error reading certificate file");
    cert_.reset(cert);
  }

  RsaCertificate(const RsaCertificate&) = delete;
  RsaCertificate(RsaCertificate&&) = delete;
  RsaCertificate& operator=(RsaCertificate&) = delete;
  RsaCertificate& operator=(RsaCertificate&&) = delete;

  void Sign(RsaKeyPair& key_pair) {
    if (not X509_sign(cert_.get(), key_pair.Get(), KEY_SIGN_DIGEST)) {
      int_error("Error signing certificate");
    }
  }

  bool Verify(RsaKeyPair& key_pair) {
    int result = X509_verify(cert_.get(), key_pair.Get());
    if (-1 == result)
      int_error("Error verifying signature on certificate");

    return result == 1 ? true : false;
  }

  X509* Get() const {
    return cert_.get();
  }

  void Serialize(std::string_view filename) const {
    FILE* fp = std::fopen(filename.data(), "w");
    if (not fp) throw std::runtime_error(
            std::string("Error opening certificate file '")
            + filename.data() + "': " + std::strerror(errno));

    int result = PEM_write_X509(fp, cert_.get());
    std::fclose(fp);
    if (not result) int_error("Error while writing certificate");
  }

 private:
  std::unique_ptr<X509, decltype(&X509_free)> cert_;
};
}

#undef int_error
#endif  // LRM_CERTS_H_
