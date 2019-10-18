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

#include "crypto/certs/CertificateRequest.h"

#include <cassert>
#include <functional>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "crypto/certs/CertsUtil.h"
#include "Util.h"

namespace lrm::crypto::certs {
CertificateRequest::CertificateRequest()
    : req_(make_req(nullptr)) {}

CertificateRequest::CertificateRequest(KeyPairBase& key_pair,
                                       const CertNameMap& name_entries)
    : req_{make_req(X509_REQ_new())} {
  if (not req_) int_error("Failed to create X509_REQ object");

  if (not X509_REQ_set_version(req_.get(), 2L))
    int_error("Error setting version in certificate request");

  if (not X509_REQ_set_pubkey(req_.get(), key_pair.Get()))
    int_error("Error setting public key in certificate request");

  const auto name = map_to_x509_name(name_entries);

  if (not X509_REQ_set_subject_name(req_.get(), name.get()))
    int_error("Error adding subject to certificate request");

  if (not X509_REQ_sign(req_.get(), key_pair.Get(), key_pair.DigestType()))
    int_error("Error signing certificate request");
}

Bytes CertificateRequest::ToDER() const {
  assert(req_ != nullptr);
  const auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not i2d_X509_REQ_bio(bio.get(), req_.get()))
    int_error("Error translating cert request to BIO");

  return bio_to_container<Bytes>(bio.get());
}

void CertificateRequest::FromDER(const Bytes& der) {
  const auto bio = container_to_bio(der);

  X509_REQ* req = d2i_X509_REQ_bio(bio.get(), nullptr);
  if (not req) int_error("Error reading cert request from BIO");

  req_ = make_req(req);
}

void CertificateRequest::ToPemFile(const fs::path& filename) const {
  assert(req_ != nullptr);

  FILE* fp = std::fopen(filename.c_str(), "w");
  if (not fp) {
    std::stringstream ss;
    ss << __PRETTY_FUNCTION__ << "Error reading certificate request file: "
       << filename;
    throw std::invalid_argument(ss.str());
  }

  const auto result = PEM_write_X509_REQ(fp, req_.get());
  std::fclose(fp);
  if (result != 1) int_error("Error writing certificate request to pem file");
}

void CertificateRequest::FromPemFile(const fs::path& filename) {
  FILE* fp = nullptr;
  if ((not Util::file_exists(filename.c_str())) or
      (not (fp = std::fopen(filename.c_str(), "r")))) {
    std::stringstream ss;
    ss << __PRETTY_FUNCTION__ << "Error reading certificate request file: "
       << filename;
    throw std::invalid_argument(ss.str());
  }

  X509_REQ* req = PEM_read_X509_REQ(fp, nullptr, nullptr, nullptr);
  std::fclose(fp);

  if (not req) int_error("Error reading certificate request from file");

  req_ = make_req(req);
}

CertificateRequest::ReqUnique CertificateRequest::make_req(X509_REQ* req) {
  const auto deleter = [](X509_REQ* req){ X509_REQ_free(req); };
  if (req == nullptr) {
    return ReqUnique(nullptr, deleter);
  } else {
    return ReqUnique(req, deleter);
  }
}
}
