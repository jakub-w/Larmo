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

  X509_NAME* name = X509_NAME_new();
  if (not name) int_error("Failed to create X509_NAME object");

  for(const auto& [key, value] : name_entries) {
    const auto value_uc = str_to_uc(value);
    if (not X509_NAME_add_entry_by_txt(
            name, key.c_str(), MBSTRING_ASC,
            value_uc.c_str(), value_uc.size(), -1, 0)) {
      X509_NAME_free(name);
      int_error("Error adding entry '" + key + "' to X509_NAME object");
    }
  }
  if (not X509_set_subject_name(cert_.get(), name)) {
    X509_NAME_free(name);
    int_error("Error setting subject name of certificate");
  }
  X509_NAME_free(name);
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
}