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

#include <cstring>
#include <sstream>

#include <openssl/pem.h>

#include "Util.h"

namespace lrm::crypto::certs {
Certificate::Certificate() : cert_(X509_new(), &X509_free) {}

Certificate::Certificate(KeyPairBase& key_pair,
                         const CertNameMap& name_entries,
                         unsigned int expiration_days)
    : cert_(X509_new(), &X509_free) {
  if (not cert_) int_error("Failed to create X509 object");
  if (not X509_set_version(cert_.get(), 2L))
    int_error("Error setting certificate version");

  if (not X509_gmtime_adj(X509_get_notBefore(cert_.get()), 0))
    int_error("Error setting beginning time of the certificate");
  if (not X509_gmtime_adj(X509_get_notAfter(cert_.get()),
                          60 * 60 * 24 * expiration_days))
    int_error("Error setting ending time of the certificate");

  if (not X509_set_pubkey(cert_.get(), key_pair.Get()))
    int_error("Error setting public key of the certificate");

  auto name = map_to_x509_name(name_entries);

  if (not X509_set_subject_name(cert_.get(), name.get()))
    int_error("Error setting subject name of certificate");
}

Certificate::Certificate(std::string& pem_str)
    : Certificate() {
  if (pem_str.empty() or (pem_str.find("-----BEGIN CERTIFICATE-----") != 0)) {
    throw std::invalid_argument(
        std::string(__PRETTY_FUNCTION__) +
        "Error reading pem_str, doesn't contain a certificate");
  }

  const auto bio = container_to_bio(pem_str);

  cert_.reset(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr));
  if (not cert_) int_error("Error reading certificate from BIO");
}

Certificate::Certificate(const Certificate& cert)
    : cert_{X509_dup(cert.Get()), &X509_free} {}

Certificate& Certificate::operator=(Certificate& cert) {
  cert_ = std::unique_ptr<X509, decltype(&X509_free)>(
      X509_dup(cert.Get()), &X509_free);

  return *this;
}

void Certificate::Sign(KeyPairBase& key_pair) {
  if (not X509_sign(cert_.get(), key_pair.Get(), key_pair.DigestType())) {
    int_error("Error signing certificate");
  }
}

bool Certificate::Verify(KeyPairBase& key_pair) {
  const int result = X509_verify(cert_.get(), key_pair.Get());
  if (-1 == result)
    int_error("Error verifying signature on certificate");

  return result == 1 ? true : false;
}

void Certificate::Serialize(std::string_view filename) const {
  FILE* fp = std::fopen(filename.data(), "w");
  if (not fp) throw std::runtime_error(
          std::string("Error opening certificate file '")
          + filename.data() + "': " + std::strerror(errno));

  const int result = PEM_write_X509(fp, cert_.get());
  std::fclose(fp);
  if (result != 1) int_error("Error while writing certificate");
}

void Certificate::Deserialize(std::string_view filename) {
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

std::string Certificate::ToString() const {
  const auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  if (not PEM_write_bio_X509(bio.get(), cert_.get()))
    int_error("Error writing certificate to BIO object");

  const std::string result = bio_to_container<std::string>(bio.get());
  if (result.empty()) int_error("Error reading from BIO");

  return result;
}

Certificate::Map Certificate::GetExtensions() const {
  const auto ext_count = X509_get_ext_count(cert_.get());
  Map extensions;
  extensions.reserve(ext_count);

  for (auto i = 0; i < ext_count; ++i) {
    const auto ext = X509_get_ext(cert_.get(), i);
    if (not ext) int_error("Error reading certificate extension");

    const auto obj = X509_EXTENSION_get_object(ext);
    if (not obj) int_error("Error reading object from extension");
    char key[256];
    OBJ_obj2txt(key, 256, obj, 0);

    const auto data = X509_EXTENSION_get_data(ext);
    if (not data) int_error("Error reading data from extension");

    extensions[key] =
        reinterpret_cast<const char*>(ASN1_STRING_get0_data(data));
  }

  return extensions;
}

Certificate::Map Certificate::GetSubjectName() const {
  const X509_NAME* name = X509_get_subject_name(cert_.get());
  if (not name) int_error("Failed to read certificate subject name");

  return name_to_map(name);
}

Certificate::Map Certificate::GetIssuerName() const {
  const X509_NAME* name = X509_get_issuer_name(cert_.get());
  if (not name) int_error("Failed to read certificate subject name");

  return name_to_map(name);
}

Certificate::Map Certificate::name_to_map(const X509_NAME* name) const {
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
