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

#include "crypto/certs.h"

#include <cstring>
#include <sstream>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

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

std::string bio_to_str(BIO* bio) noexcept {
  std::stringstream ss;
  char buffer[256];
  int readbytes = 0;
  while ((readbytes = BIO_read(bio, buffer, 255)) > 0) {
    buffer[readbytes] = '\0';
    ss << buffer;
  }

  return ss.str();
}
}

RsaKeyPair::RsaKeyPair(bool generate)
    : pkey_(EVP_PKEY_new(), &EVP_PKEY_free) {
  if (not generate) return;

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

void RsaKeyPair::SerializePrivateKey(std::string_view filename,
                                     std::string_view password) const {
  FILE* fp = std::fopen(filename.data(), "w");
  if (not fp) {
    throw std::runtime_error(
        std::string("Error opening file '")
        + filename.data() + "': " + std::strerror(errno));
  }

  const EVP_CIPHER* enc = password.empty() ? nullptr : KEY_PEM_CIPHER;

  std::basic_string<unsigned char> kstr;
  if (not password.empty()) {
    kstr = str_to_uc(password);
  }

  int result = PEM_write_PrivateKey(fp, pkey_.get(), enc,
                                    kstr.empty() ? nullptr : kstr.data(),
                                    password.size(), nullptr, nullptr);
  std::fclose(fp);

  if (result != 1) int_error("Error writing private key to file");
}

void RsaKeyPair::SerializePublicKey(std::string_view filename) const {
  FILE* fp = std::fopen(filename.data(), "w");
  if (not fp) {
    throw std::runtime_error(
        std::string("Error opening file '")
        + filename.data() + "': " + std::strerror(errno));
  }

  int result = PEM_write_PUBKEY(fp, pkey_.get());
  std::fclose(fp);
  if (result != 1) int_error("Error writing public key to file");
}

void RsaKeyPair::DeserializePrivateKey(std::string_view filename,
                                       std::string password) {
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

std::string RsaKeyPair::ToString() const {
  BIO* bio = BIO_new(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not PEM_write_bio_PrivateKey(bio, pkey_.get(), nullptr,
                                   nullptr, 0, nullptr, nullptr))
    int_error("Error writing private key to BIO");

  std::string result = bio_to_str(bio);
  BIO_free(bio);
  if (result.empty()) int_error("Error reading from BIO");

  return result;
}

RsaCertificate::RsaCertificate() : cert_(X509_new(), &X509_free) {}

RsaCertificate::RsaCertificate(RsaKeyPair& key_pair,
                               const CertNameMap name_entries,
                               unsigned int expiration_days)
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

RsaCertificate::RsaCertificate(std::string& pem_str)
    : RsaCertificate() {
  if (pem_str.empty() or (pem_str.find("-----BEGIN CERTIFICATE-----") != 0)) {
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) +
        "Error reading pem_str, doesn't contain a certificate");
  }

  BIO* bio = BIO_new(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  BIO_write(bio, pem_str.data(), pem_str.length());

  cert_.reset(PEM_read_bio_X509(bio, nullptr, nullptr, nullptr));
  BIO_free(bio);
  if (not cert_) int_error("Error reading certificate from BIO");
}

void RsaCertificate::Sign(RsaKeyPair& key_pair) {
  if (not X509_sign(cert_.get(), key_pair.Get(), KEY_SIGN_DIGEST)) {
    int_error("Error signing certificate");
  }
}

bool RsaCertificate::Verify(RsaKeyPair& key_pair) {
  int result = X509_verify(cert_.get(), key_pair.Get());
  if (-1 == result)
    int_error("Error verifying signature on certificate");

  return result == 1 ? true : false;
}

void RsaCertificate::Serialize(std::string_view filename) const {
  FILE* fp = std::fopen(filename.data(), "w");
  if (not fp) throw std::runtime_error(
          std::string("Error opening certificate file '")
          + filename.data() + "': " + std::strerror(errno));

  int result = PEM_write_X509(fp, cert_.get());
  std::fclose(fp);
  if (result != 1) int_error("Error while writing certificate");
}

void RsaCertificate::Deserialize(std::string_view filename) {
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

std::string RsaCertificate::ToString() const {
  BIO* bio = BIO_new(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not PEM_write_bio_X509(bio, cert_.get()))
    int_error("Error writing certificate to BIO object");

  std::string result = bio_to_str(bio);
  BIO_free(bio);
  if (result.empty()) int_error("Error reading from BIO");

  return result;
}
}

#undef int_error
