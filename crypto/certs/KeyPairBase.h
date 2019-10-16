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

#ifndef LRM_KEYPAIRBASE_H_
#define LRM_KEYPAIRBASE_H_

#include <memory>
#include <string_view>
#include <vector>

#include <openssl/evp.h>

#include "filesystem.h"

namespace lrm::crypto::certs {
class KeyPairBase {
 public:
  using Bytes = std::vector<std::byte>;
  KeyPairBase();
  virtual ~KeyPairBase() = default;

  inline EVP_PKEY* Get() {
    return pkey_.get();
  }

  virtual const EVP_MD* DigestType() const = 0;

  virtual std::string ToPemPrivKey(std::string password = "") const;
  virtual std::string ToPemPubKey() const;
  virtual void ToPemFilePrivKey(const fs::path& filename,
                                std::string password = "") const;
  virtual void ToPemFilePubKey(const fs::path& filename) const;
  virtual Bytes ToDerPrivKey() const;
  virtual Bytes ToDerPubKey() const;

  virtual void Generate() = 0;
  virtual void FromPEM(std::string_view pem_privkey,
                       std::string password = "");
  virtual void FromPemFile(const fs::path& filename,
                           std::string password = "");
  virtual void FromDER(const Bytes& der);

 protected:
  virtual bool is_correct_type(const EVP_PKEY* pkey) const = 0;

 protected:
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey_;
};
}

#endif  // LRM_KEYPAIRBASE_H_
