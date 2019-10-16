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
#include "crypto/config.h"

namespace lrm::crypto::certs {
class RsaKeyPair : public KeyPairBase {
 public:
  void Generate() final;
  const EVP_MD* DigestType() const final {
    return LRM_RSA_KEY_SIGN_DIGEST;
  }
  inline bool is_correct_type(const EVP_PKEY* pkey) const final {
    return EVP_PKEY_id(pkey) == EVP_PKEY_RSA;
  }
};
}
