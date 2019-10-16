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

#ifndef LRM_CERTIFICATE_H_
#define LRM_CERTIFICATE_H_

#include <memory>
#include <unordered_map>

#include <openssl/x509.h>

#include "crypto/certs/KeyPairBase.h"

namespace lrm::crypto::certs {
using CertNameMap =
      std::unordered_map<std::string, std::string>;

class Certificate {
 public:
  Certificate();
  Certificate(KeyPairBase& key_pair,
              const CertNameMap& name_entries,
              unsigned int expiration_days = 365);
  /// Construct RsaCertificate object from the PEM form present in \e pem_str.
  /// \param pem_str String storing PEM form of the certificate.
  explicit Certificate(std::string& pem_str);

  Certificate(const Certificate&) = delete;
  Certificate(Certificate&&) = delete;
  Certificate& operator=(Certificate&) = delete;
  Certificate& operator=(Certificate&&) = delete;

  void Sign(KeyPairBase& key_pair);

  bool Verify(KeyPairBase& key_pair);

  inline X509* Get() const {
    return cert_.get();
  }

  void Serialize(std::string_view filename) const;

  /// Deserialize certificate from PEM file storing X509 certificate
  void Deserialize(std::string_view filename);
  std::string ToString() const;

 private:
  std::unique_ptr<X509, decltype(&X509_free)> cert_;
};
}

#endif  // LRM_CERTIFICATE_H_
