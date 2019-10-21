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

#ifndef LRM_CERTIFICATEAUTHORITY_H_
#define LRM_CERTIFICATEAUTHORITY_H_

#include "crypto/certs/Certificate.h"
#include "crypto/certs/CertificateRequest.h"

namespace lrm::crypto::certs {
class CertificateAuthority {
 public:
  CertificateAuthority() = delete;
  CertificateAuthority(const Map& name,
                       std::shared_ptr<KeyPairBase>&& key_pair,
                       unsigned int expiration_days = 3650);

  /// \brief Create \ref Certificate from \ref CertificateRequest.
  ///
  /// \note All requested extensions in the \e request are currently ignored.
  Certificate Certify(CertificateRequest&& request,
                      unsigned int expiration_days);
  inline const Certificate& GetRootCertificate() const {
    return cert_;
  }

 private:
  Certificate cert_;
  std::shared_ptr<KeyPairBase> key_pair_;

  size_t serial_ = 1;
};
}

#endif  // LRM_CERTIFICATEAUTHORITY_H_
