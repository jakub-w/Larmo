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

#include "crypto/certs/KeyPairBase.h"

#include <cassert>
#include <fstream>

#include <openssl/pem.h>

#include "crypto/certs/CertsUtil.h"
#include "crypto/config.h"
#include "Util.h"

namespace lrm::crypto::certs {
KeyPairBase::KeyPairBase() : pkey_(nullptr, &EVP_PKEY_free) {}

std::string KeyPairBase::ToPemPrivKey(std::string password) const {
  assert(pkey_);
  const EVP_CIPHER* enc = password.empty() ? nullptr : LRM_RSA_KEY_PEM_CIPHER;

  const auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  unsigned char* kstr = nullptr;
  if (not password.empty()) {
    kstr = reinterpret_cast<unsigned char*>(password.data());
  }

  if (not PEM_write_bio_PrivateKey(bio.get(), pkey_.get(), enc, kstr,
                                   password.size(), nullptr, nullptr))
    int_error("Error writing private key to BIO");

  return bio_to_container<std::string>(bio.get());
}

std::string KeyPairBase::ToPemPubKey() const {
  assert(pkey_);
  const auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not PEM_write_bio_PUBKEY(bio.get(), pkey_.get()))
    int_error("Error writing public key to BIO");

  return bio_to_container<std::string>(bio.get());
}

void KeyPairBase::ToPemFilePrivKey(const fs::path& filename,
                                   std::string password) const {
  assert(pkey_);
  std::ofstream(filename) << ToPemPrivKey(std::move(password));
}

void KeyPairBase::ToPemFilePubKey(const fs::path& filename) const {
  assert(pkey_);
  std::ofstream(filename) << ToPemPubKey();
}

KeyPairBase::Bytes KeyPairBase::ToDerPrivKey() const {
  assert(pkey_);
  const auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not i2d_PrivateKey_bio(bio.get(), pkey_.get()))
    int_error("Error writing private key to BIO");

  return bio_to_container<Bytes>(bio.get());
}

KeyPairBase::Bytes KeyPairBase::ToDerPubKey() const {
  assert(pkey_);
  auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not i2d_PUBKEY_bio(bio.get(), pkey_.get()))
    int_error("Error writing public key to BIO");

  return bio_to_container<Bytes>(bio.get());
}

void KeyPairBase::FromPEM(std::string_view pem_privkey,
                          std::string password) {
  auto bio = container_to_bio(pem_privkey);
  EVP_PKEY* result =
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, password.data());
  if (not result) int_error("Error reading private key from BIO");
  if (not is_correct_type(result)) {
    EVP_PKEY_free(result);
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) + ": PEM_PRIVKEY is wrong type");
  }
  pkey_.reset(result);
}

void KeyPairBase::FromPemFile(const fs::path& filename,
                              std::string password) {
  FILE* fp = nullptr;
  if ((not Util::file_exists(filename.c_str())) or
      (not (fp = std::fopen(filename.c_str(), "r")))) {
    std::stringstream ss;
    ss << __PRETTY_FUNCTION__ << "Error reading private key file: "
       << filename;
    throw std::invalid_argument(ss.str());
  }

  EVP_PKEY* pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, password.data());
  std::fclose(fp);

  if (not pkey) int_error("Error reading private key from file");

  pkey_.reset(pkey);
}

void KeyPairBase::FromDER(const Bytes& der) {
  auto bio = container_to_bio(der);

  EVP_PKEY* pkey = nullptr;
  if (not d2i_PrivateKey_bio(bio.get(), &pkey))
    int_error("Error reading private key from DER format");
  if (not is_correct_type(pkey)) {
    EVP_PKEY_free(pkey);
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) + ": DER is wrong type");
  }

  pkey_.reset(pkey);
}
}
