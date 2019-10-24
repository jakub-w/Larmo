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

#include "crypto/certs/Certificate.h"
#include "crypto/certs/CertsUtil.h"

#include <cassert>
#include <cstring>
#include <sstream>

#include <openssl/pem.h>

#include "Util.h"

namespace lrm::crypto::certs {
Certificate Certificate::FromPem(std::string_view pem_str) {
  if (pem_str.empty() or (pem_str.find("-----BEGIN CERTIFICATE-----") != 0)) {
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) +
        "Error reading pem_str, doesn't contain a certificate");
  }

  const auto bio = container_to_bio(pem_str);

  X509* cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
  if (not cert) int_error("Error reading certificate from BIO");

  return Certificate{cert};
}

Certificate Certificate::FromPemFile(std::string_view filename) {
  FILE* fp = nullptr;
  if ((not Util::file_exists(filename)) or
      (not (fp = std::fopen(filename.data(), "r")))) {
    std::stringstream ss;
    ss << __PRETTY_FUNCTION__ << "Error reading certificate file: "
       << filename;
    throw std::invalid_argument(ss.str());
  }

  X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
  std::fclose(fp);
  if (not cert) int_error("Error reading certificate file");

  return Certificate{cert};
}

Certificate Certificate::FromDer(const Bytes& der) {
  auto bio = container_to_bio(der);

  X509* cert = d2i_X509_bio(bio.get(), nullptr);
  if (not cert) int_error("Error reading certificate from BIO");

  return Certificate{cert};
}

Certificate::Certificate() : cert_(X509_new(), &X509_free) {}
Certificate::Certificate(X509* cert) : cert_(cert, &X509_free) {}

Certificate::Certificate(const Certificate& cert)
    : cert_{X509_dup(cert.Get()), &X509_free} {}

Certificate& Certificate::operator=(Certificate& cert) {
  cert_ = std::unique_ptr<X509, decltype(&X509_free)>(
      X509_dup(cert.Get()), &X509_free);

  return *this;
}

bool Certificate::Verify(const Certificate& another) const {
  assert(cert_.get() != nullptr);
  EVP_PKEY* this_pubkey = X509_get0_pubkey(cert_.get());
  if (not this_pubkey) int_error("Error getting certificate public key");

  const int result = X509_verify(another.Get(), this_pubkey);
  if (-1 == result)
    int_error("Error verifying signature on certificate");

  return result == 1 ? true : false;
}

void Certificate::ToPemFile(std::string_view filename) const {
  assert(cert_.get() != nullptr);
  FILE* fp = std::fopen(filename.data(), "w");
  if (not fp) throw std::runtime_error(
          std::string("Error opening certificate file '")
          + filename.data() + "': " + std::strerror(errno));

  const int result = PEM_write_X509(fp, cert_.get());
  std::fclose(fp);
  if (result != 1) int_error("Error while writing certificate");
}

std::string Certificate::ToPem() const {
  assert(cert_.get() != nullptr);
  auto bio = make_bio(BIO_s_mem());

  if (not PEM_write_bio_X509(bio.get(), cert_.get()))
    int_error("Error writing certificate to BIO object");

  const std::string result = bio_to_container<std::string>(bio.get());
  if (result.empty()) int_error("Error reading from BIO");

  return result;
}

Bytes Certificate::ToDer() const {
  assert(cert_.get() != nullptr);

  auto bio = make_bio(BIO_s_mem());

  if (not i2d_X509_bio(bio.get(), cert_.get()))
    int_error("Error writing certificate to BIO");

  return bio_to_container<Bytes>(bio.get());
}

Map Certificate::GetExtensions() const {
  assert(cert_.get() != nullptr);

  return x509_ext_stack_to_map(X509_get0_extensions(cert_.get()));
}

Map Certificate::GetSubjectName() const {
  assert(cert_.get() != nullptr);

  const X509_NAME* name = X509_get_subject_name(cert_.get());
  if (not name) int_error("Failed to read certificate subject name");

  return x509_name_to_map(name);
}

Map Certificate::GetIssuerName() const {
  assert(cert_.get() != nullptr);

  const X509_NAME* name = X509_get_issuer_name(cert_.get());
  if (not name) int_error("Failed to read certificate subject name");

  return x509_name_to_map(name);
}

Bytes Certificate::GetHash() const {
  const auto der = ToDer();
  Bytes result(SHA256_DIGEST_LENGTH);
  SHA256(reinterpret_cast<const unsigned char*>(der.data()),
         der.size(),
         reinterpret_cast<unsigned char*>(result.data()));
  return result;
}
}
