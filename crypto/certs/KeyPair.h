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

#ifndef LRM_KEYPAIR_H_
#define LRM_KEYPAIR_H_

#include <functional>
#include <memory>
#include <string>

#include <openssl/evp.h>

#include "crypto/config.h"
#include "crypto/certs/CertsUtil.h"

namespace lrm::crypto::certs {
class KeyPair {
 public:
  struct keypair_t {
    using evp_pkey_t = int;
    keypair_t() = delete;
    keypair_t(const keypair_t&) = delete;
    keypair_t(keypair_t&&) = delete;
    keypair_t& operator=(const keypair_t&) = delete;
    keypair_t& operator=(keypair_t&&) = delete;

    const std::function<EVP_PKEY*(void)> generate;
    const EVP_MD* digest;
    const evp_pkey_t type;
  };

  static const keypair_t& ED25519();
  static const keypair_t& RSA();

  static KeyPair FromPem(const keypair_t& type,
                         std::string_view pem_privkey,
                         std::string password = "");
  static KeyPair FromPemFile(const keypair_t& type,
                             std::string_view filename,
                             std::string password = "");
  static KeyPair FromDer(const keypair_t& type,
                         const Bytes& der);
  static KeyPair Generate(const keypair_t& type);

  KeyPair() = delete;
  KeyPair(KeyPair&&) = default;
  KeyPair& operator=(KeyPair&&) = default;

  std::string ToPemPrivKey(std::string password = "") const;
  std::string ToPemPubKey() const;
  void ToPemFilePrivKey(std::string_view filename,
                        std::string password = "") const;
  void ToPemFilePubKey(std::string_view filename) const;
  Bytes ToDerPrivKey() const;
  Bytes ToDerPubKey() const;

  inline const EVP_MD* DigestType() const {
    return type_->digest;
  }

  inline EVP_PKEY* Get() {
    return pkey_.get();
  }

 private:
  KeyPair(EVP_PKEY* pkey, const keypair_t& type);

  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_;
  const keypair_t* type_;
};
}

#endif  // LRM_KEYPAIR_H_
