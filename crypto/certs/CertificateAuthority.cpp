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

#include "crypto/certs/CertificateAuthority.h"

#include <cassert>

#include <openssl/x509.h>

#include "crypto/certs/CertsUtil.h"

namespace lrm::crypto::certs {

CertificateAuthority::CertificateAuthority(
    const Map& name_entries,
    std::shared_ptr<KeyPairBase>&& key_pair,
    unsigned int expiration_days)
    : key_pair_{std::move(key_pair)} {
  if (not cert_.Get()) int_error("Failed to create X509 object");
  if (not X509_set_version(cert_.Get(), 2L))
    int_error("Error setting certificate version");

  if (not X509_gmtime_adj(X509_get_notBefore(cert_.Get()), 0))
    int_error("Error setting beginning time of the certificate");
  if (not X509_gmtime_adj(X509_get_notAfter(cert_.Get()),
                          60 * 60 * 24 * expiration_days))
    int_error("Error setting ending time of the certificate");

  if (not X509_set_pubkey(cert_.Get(), key_pair->Get()))
    int_error("Error setting public key of the certificate");

  auto name = map_to_x509_name(name_entries);

  if (not X509_set_subject_name(cert_.Get(), name.get()))
    int_error("Error setting subject name of certificate");

  if (not X509_set_issuer_name(cert_.Get(), name.get()))
    int_error("Error setting issuer name for CA certificate");

  // TODO: Add extensions: basicConstraints = critical,CA:true
  //                       keyUsage = "keyCertSign, keyAgreement"

  if (not X509_sign(cert_.Get(), key_pair_->Get(), key_pair_->DigestType()))
    int_error("Error self-signing CA certificate");
}

Certificate CertificateAuthority::Certify(CertificateRequest&& request,
                                          unsigned int expiration_days) {
  assert(request.Get() != nullptr);

  auto result = Certificate{};

  if (not X509_set_version(result.Get(), 2L))
    int_error("Error setting certificate version");

  ASN1_INTEGER_set(X509_get_serialNumber(result.Get()), serial_++);

  const auto name = X509_REQ_get_subject_name(request.Get());
  if (not name) int_error("Error getting subject name from request");
  if (not X509_set_subject_name(result.Get(), name))
    int_error("Error setting subject name of certificate");

  const auto CAname = X509_get_subject_name(cert_.Get());
  if (not CAname) int_error("Error getting subject name from CA certificate");
  if (not X509_set_issuer_name(result.Get(), CAname))
    int_error("Error setting issuer name of certificate");

  const auto pubkey = X509_REQ_get_pubkey(request.Get());
  if (not pubkey) int_error("Error getting public key from request");
  if (not X509_set_pubkey(result.Get(), pubkey))
    int_error("Error setting public key of the certificate");

  if (not X509_gmtime_adj(X509_get_notBefore(result.Get()), 0))
    int_error("Error setting beginning time of the certificate");
  if (not X509_gmtime_adj(X509_get_notAfter(result.Get()),
                          60 * 60 * 24 * expiration_days))
    int_error("Error setting ending time of the certificate");

  // TODO: Add extension: basicConstraints: "critical,CA:false",
  //                      keyUsage: "keyAgreement"

  if (not X509_sign(result.Get(), key_pair_->Get(), key_pair_->DigestType()))
    int_error("Error signing the certificate");

  return result;
}
}
