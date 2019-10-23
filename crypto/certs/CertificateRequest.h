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

#ifndef LRM_CERTIFICATEREQUEST_H_
#define LRM_CERTIFICATEREQUEST_H_

#include <functional>

#include "crypto/certs/Certificate.h"
#include "crypto/certs/KeyPair.h"
#include "crypto/config.h"
#include "filesystem.h"

namespace lrm::crypto::certs {
class CertificateRequest {
 public:
  static CertificateRequest FromPem(std::string_view pem_str);
  static CertificateRequest FromPemFile(const fs::path& filename);
  static CertificateRequest FromDER(const Bytes& der);

  CertificateRequest() = delete;
  CertificateRequest(KeyPair& key_pair,
                     const Map& name_entries,
                     const Map& extensions = {});

  CertificateRequest(CertificateRequest&&) = default;
  CertificateRequest& operator=(CertificateRequest&&) = default;

  std::string ToPem() const;
  void ToPemFile(const fs::path& filename) const;
  Bytes ToDER() const;

  Map GetName() const;
  Map GetExtensions() const;

  inline X509_REQ* Get() {
    return req_.get();
  }

 private:
  CertificateRequest(X509_REQ* req);

  std::unique_ptr<X509_REQ, decltype(&X509_REQ_free)> req_;
};
}

#endif  // LRM_CERTIFICATEREQUEST_H_
