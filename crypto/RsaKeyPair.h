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

#ifndef LRM_RSAKEYPAIR_H_
#define LRM_RSAKEYPAIR_H_

#include <memory>
#include <string_view>

#include <openssl/evp.h>

namespace lrm::crypto::certs {
class RsaKeyPair {
 public:
  /// \param generate If true, generate new RSA key pair. If not, construct
  /// empty RsaKeyPair object.
  RsaKeyPair(bool generate = false);

  RsaKeyPair(const RsaKeyPair&) = delete;
  RsaKeyPair(RsaKeyPair&&) = delete;
  RsaKeyPair& operator=(const RsaKeyPair&) = delete;
  RsaKeyPair& operator=(RsaKeyPair&&) = delete;

  inline EVP_PKEY* Get() {
    return pkey_.get();
  }

  void SerializePrivateKey(std::string_view filename,
                           std::string_view password = "") const;

  void SerializePublicKey(std::string_view filename) const;

  /// Read RsaKeyPair from PEM file storing RSA private key
  void DeserializePrivateKey(std::string_view filename,
                             std::string password = "");

  std::string ToString() const;

 private:
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_;
};
}

#endif  // LRM_RSAKEYPAIR_H_
