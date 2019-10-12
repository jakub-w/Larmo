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

#include "crypto/RsaKeyPair.h"

#include <cstring>
#include <sstream>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "crypto/certs.h"
#include "Util.h"

namespace lrm::crypto::certs {
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
}
