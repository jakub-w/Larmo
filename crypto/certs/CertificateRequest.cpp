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
CertificateRequest CertificateRequest::FromPem(std::string_view pem_str) {
  auto bio = container_to_bio(pem_str);

  X509_REQ* req = PEM_read_bio_X509_REQ(bio.get(), nullptr, nullptr, nullptr);
  if (not req) int_error("Error reading certificate request from BIO");

  return CertificateRequest(req);
}

CertificateRequest CertificateRequest::FromPemFile(const fs::path& filename) {
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

  return CertificateRequest(req);
}

CertificateRequest CertificateRequest::FromDER(const Bytes& der) {
  const auto bio = container_to_bio(der);

  X509_REQ* req = d2i_X509_REQ_bio(bio.get(), nullptr);
  if (not req) int_error("Error reading cert request from BIO");

  return CertificateRequest(req);
}

CertificateRequest::CertificateRequest(X509_REQ* req)
    : req_{req, &X509_REQ_free} {}

CertificateRequest::CertificateRequest(KeyPairBase& key_pair,
                                       const Map& name_entries,
                                       const Map& extensions)
    : req_{X509_REQ_new(), &X509_REQ_free} {
  if (not req_) int_error("Failed to create X509_REQ object");

  if (not X509_REQ_set_version(req_.get(), 2L))
    int_error("Error setting version in certificate request");

  if (not X509_REQ_set_pubkey(req_.get(), key_pair.Get()))
    int_error("Error setting public key in certificate request");

  const auto name = map_to_x509_name(name_entries);
  if (not X509_REQ_set_subject_name(req_.get(), name.get()))
    int_error("Error adding subject to certificate request");

  const auto extlist = map_to_x509_extension_stack(extensions);
  if (not X509_REQ_add_extensions(req_.get(), extlist.get()))
    int_error("Error adding extensions to the certificate request");

  if (not X509_REQ_sign(req_.get(), key_pair.Get(), key_pair.DigestType()))
    int_error("Error signing certificate request");
}

std::string CertificateRequest::ToPem() const {
  assert(req_ != nullptr);
  auto bio = make_bio(BIO_s_mem());

  if (not PEM_write_bio_X509_REQ(bio.get(), req_.get()))
    int_error("Error writing certificate request to BIO");

  return bio_to_container<std::string>(bio.get());
}

Bytes CertificateRequest::ToDER() const {
  assert(req_ != nullptr);
  const auto bio = make_bio(BIO_s_mem());

  if (not i2d_X509_REQ_bio(bio.get(), req_.get()))
    int_error("Error translating cert request to BIO");

  return bio_to_container<Bytes>(bio.get());
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

Map CertificateRequest::GetName() const {
  assert(req_.get() != nullptr);

  const X509_NAME* name = X509_REQ_get_subject_name(req_.get());
  if (not name) int_error("Error reading name from certificate request");

  return x509_name_to_map(name);
}

Map CertificateRequest::GetExtensions() const {
  assert(req_.get() != nullptr);

  auto extlist = X509_REQ_get_extensions(req_.get());
  const auto result = x509_ext_stack_to_map(extlist);
  sk_X509_EXTENSION_pop_free(extlist, X509_EXTENSION_free);
  return result;
}
}
