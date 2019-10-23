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

#include <openssl/x509.h>

#include "crypto/certs/CertsUtil.h"
#include "crypto/config.h"

namespace lrm::crypto::certs {
class Certificate {
 public:
  /// Construct Certificate object from the PEM file \e filename.
  static Certificate FromPemFile(std::string_view filename);
  /// Construct Certificate object from the PEM form present in \e pem_str.
  /// \param pem_str String storing PEM form of the certificate.
  static Certificate FromPem(std::string_view pem_str);
  /// Construct Certificate object from DER format stored in \e der.
  static Certificate FromDer(const Bytes& der);

  Certificate(const Certificate& cert);
  Certificate& operator=(Certificate& cert);
  Certificate(Certificate&& cert) = default;
  Certificate& operator=(Certificate&& cert) = default;

  /// \brief Verify \e another certificate with this one.
  bool Verify(const Certificate& another) const;

  void ToPemFile(std::string_view filename) const;
  std::string ToPem() const;
  Bytes ToDer() const;

  Map GetExtensions() const;
  Map GetSubjectName() const;
  Map GetIssuerName() const;

  Bytes GetHash() const;

 private:
  friend class CertificateAuthority;

  Certificate();
  Certificate(X509* cert);

  inline X509* Get() const {
    return cert_.get();
  }

  std::unique_ptr<X509, decltype(&X509_free)> cert_;
};
}

#endif  // LRM_CERTIFICATE_H_
