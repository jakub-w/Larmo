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
Certificate::Certificate() : cert_(nullptr, &X509_free) {}

Certificate::Certificate(std::string_view pem_str)
    : Certificate() {
  FromPem(pem_str);
}

Certificate::Certificate(const Certificate& cert)
    : cert_{X509_dup(cert.Get()), &X509_free} {}

Certificate& Certificate::operator=(Certificate& cert) {
  cert_ = std::unique_ptr<X509, decltype(&X509_free)>(
      X509_dup(cert.Get()), &X509_free);

  return *this;
}

bool Certificate::Verify(const Certificate& another) const {
  assert(cert_.get() != nullptr);
  EVP_PKEY* this_pubkey = X509_get_pubkey(cert_.get());
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

void Certificate::FromPemFile(std::string_view filename) {
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
  cert_.reset(cert);
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

void Certificate::FromPem(std::string_view pem_str) {
  if (pem_str.empty() or (pem_str.find("-----BEGIN CERTIFICATE-----") != 0)) {
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) +
        "Error reading pem_str, doesn't contain a certificate");
  }

  const auto bio = container_to_bio(pem_str);

  cert_.reset(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
  if (not cert_) int_error("Error reading certificate from BIO");
}

Bytes Certificate::ToDer() const {
  assert(cert_.get() != nullptr);

  auto bio = make_bio(BIO_s_mem());

  if (not i2d_X509_bio(bio.get(), cert_.get()))
    int_error("Error writing certificate to BIO");

  return bio_to_container<Bytes>(bio.get());
}

void Certificate::FromDer(const Bytes& der) {
  auto bio = container_to_bio(der);

  X509* cert = d2i_X509_bio(bio.get(), nullptr);
  if (not cert) int_error("Error reading certificate from BIO");

  cert_.reset(cert);
}

Map Certificate::GetExtensions() const {
  const auto ext_count = X509_get_ext_count(cert_.get());
  Map extensions;
  extensions.reserve(ext_count);

  auto bio = make_bio(BIO_s_mem());
  for (auto i = 0; i < ext_count; ++i) {
    const auto ext = X509_get_ext(cert_.get(), i);
    if (not ext) int_error("Error reading certificate extension");

    const auto obj = X509_EXTENSION_get_object(ext);
    if (not obj) int_error("Error reading object from extension");

    char key[256];
    OBJ_obj2txt(key, 256, obj, 0);

    X509V3_EXT_print(bio.get(), ext, 0, 0);

    extensions[key] = bio_to_container<std::string>(bio.get());
  }

  return extensions;
}

Map Certificate::GetSubjectName() const {
  const X509_NAME* name = X509_get_subject_name(cert_.get());
  if (not name) int_error("Failed to read certificate subject name");

  return name_to_map(name);
}

Map Certificate::GetIssuerName() const {
  const X509_NAME* name = X509_get_issuer_name(cert_.get());
  if (not name) int_error("Failed to read certificate subject name");

  return name_to_map(name);
}

Map Certificate::name_to_map(const X509_NAME* name) const {
  Map entries;
  const int num_entries = X509_NAME_entry_count(name);
  entries.reserve(num_entries);
  for (int i = 0; i < num_entries; ++i) {
    const X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, i);
    if (not entry) int_error("Error reading entry from certificate name");

    const ASN1_OBJECT* obj = X509_NAME_ENTRY_get_object(entry);
    if (not obj) int_error("Error reading object from name entry");

    char key[256];
    OBJ_obj2txt(key, 256, obj, 0);
    const ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    if (not data) int_error("Error reading data from name entry");

    entries[key] = reinterpret_cast<const char*>(ASN1_STRING_get0_data(data));
  }
  return entries;
}
}
