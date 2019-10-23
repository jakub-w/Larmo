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

#include "crypto/certs/KeyPair.h"

#include <cassert>
#include <sstream>

#include <openssl/pem.h>

#include "crypto/certs/CertsUtil.h"
#include "Util.h"

namespace lrm::crypto::certs {
KeyPair KeyPair::FromPem(const keypair_t& type,
                         std::string_view pem_privkey,
                         std::string password) {
  assert(password.empty() && "Encryption not implemented");
  auto bio = container_to_bio(pem_privkey);

  EVP_PKEY* pkey =
      PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
  if (not pkey) int_error("Error reading private key from BIO");
  if (EVP_PKEY_id(pkey) != type.type) {
    EVP_PKEY_free(pkey);
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) + ": PEM_PRIVKEY is wrong type");
  }

  return KeyPair(pkey, type);
}

KeyPair KeyPair::FromPemFile(const keypair_t& type,
                             std::string_view filename,
                             std::string password) {
  assert(password.empty() && "Encryption not implemented");
  auto fp = open_file(filename, "r");
  EVP_PKEY* pkey = PEM_read_PrivateKey(fp.get(), nullptr, nullptr, nullptr);

  if (not pkey) int_error("Error reading private key from file");
  if (EVP_PKEY_id(pkey) != type.type) {
    EVP_PKEY_free(pkey);
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) + ": PEM_PRIVKEY is wrong type");
  }

  return KeyPair(pkey, type);
}

KeyPair KeyPair::FromDer(const keypair_t& type,
                         const Bytes& der) {
  auto bio = container_to_bio(der);

  EVP_PKEY* pkey = nullptr;
  if (not d2i_PrivateKey_bio(bio.get(), &pkey))
    int_error("Error reading private key from DER format");
  if (EVP_PKEY_id(pkey) != type.type) {
    EVP_PKEY_free(pkey);
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) + ": PEM_PRIVKEY is wrong type");
  }

  return KeyPair(pkey, type);
}

KeyPair KeyPair::Generate(const keypair_t& type) {
  return KeyPair(type.generate(), type);
}

KeyPair::KeyPair(EVP_PKEY* pkey, const keypair_t& type)
    : pkey_{pkey, &EVP_PKEY_free},
      type_{&type} {}

std::string KeyPair::ToPemPrivKey(std::string password) const {
  assert(password.empty() && "Encryption not implemented");
  assert(pkey_.get() != nullptr);

  const auto bio = make_bio(BIO_s_mem());

  if (not PEM_write_bio_PKCS8PrivateKey(bio.get(), pkey_.get(), nullptr,
                                        nullptr, 0, nullptr, nullptr))
    int_error("Error writing private key to BIO");

  return bio_to_container<std::string>(bio.get());
}

std::string KeyPair::ToPemPubKey() const {
  assert(pkey_.get() != nullptr);
  const auto bio = make_bio(BIO_s_mem());

  if (not PEM_write_bio_PUBKEY(bio.get(), pkey_.get()))
    int_error("Error writing public key to BIO");

  return bio_to_container<std::string>(bio.get());
}

void KeyPair::ToPemFilePrivKey(std::string_view filename,
                               std::string password) const {
  assert(password.empty() && "Encryption not implemented");
  assert(pkey_.get() != nullptr);

  auto fp = open_file(filename, "w");
  if (not PEM_write_PKCS8PrivateKey(fp.get(), pkey_.get(), nullptr, nullptr,
                                    0, nullptr, nullptr))
    int_error("Error writing private key to a file");
}

void KeyPair::ToPemFilePubKey(std::string_view filename) const {
  assert(pkey_.get() != nullptr);

  auto fp = open_file(filename, "w");
  if (not PEM_write_PUBKEY(fp.get(), pkey_.get()))
    int_error("Error writing public key to a file");
}

Bytes KeyPair::ToDerPrivKey() const {
  assert(pkey_.get() != nullptr);

  const auto bio = make_bio(BIO_s_mem());

  if (not i2d_PrivateKey_bio(bio.get(), pkey_.get()))
    int_error("Error writing private key to BIO");

  return bio_to_container<Bytes>(bio.get());
}

Bytes KeyPair::ToDerPubKey() const {
  assert(pkey_.get() != nullptr);

  auto bio = make_bio(BIO_s_mem());

  if (not i2d_PUBKEY_bio(bio.get(), pkey_.get()))
    int_error("Error writing public key to BIO");

  return bio_to_container<Bytes>(bio.get());
}

/* ======================== KEY TYPES ======================== */

const KeyPair::keypair_t KeyPair::ED25519{
  []{
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr);
    if (not pctx) int_error("Failed to create EVP_PKEY_CTX object");

    if (not EVP_PKEY_keygen_init(pctx)) {
      EVP_PKEY_CTX_free(pctx);
      int_error("Error initializing EVP_PKEY keygen");
    }
    if (not EVP_PKEY_keygen(pctx, &pkey)) {
      EVP_PKEY_CTX_free(pctx);
      int_error("Error generating key");
    }
    EVP_PKEY_CTX_free(pctx);

    return pkey;
  },
  EVP_md_null(),
  EVP_PKEY_ED25519
};

const KeyPair::keypair_t KeyPair::RSA{
  []{
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (not pctx) int_error("Failed to create EVP_PKEY_CTX object");

    try {
      if (EVP_PKEY_keygen_init(pctx) <= 0)
        int_error("Error initializing EVP_PKEY keygen");
      if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, LRM_RSA_KEY_BITS) <= 0)
        int_error("Error setting keygen bits for RSA");
      if (EVP_PKEY_keygen(pctx, &pkey) <= 0)
        int_error("Error generating key");
    } catch (...) {
      EVP_PKEY_CTX_free(pctx);
      throw;
    }
    EVP_PKEY_CTX_free(pctx);

    return pkey;
  },
  LRM_RSA_KEY_SIGN_DIGEST,
  EVP_PKEY_RSA
};
}
