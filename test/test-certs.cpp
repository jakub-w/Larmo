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

#include <gtest/gtest.h>

#include "crypto/certs.h"
#include "filesystem.h"

using namespace lrm::crypto::certs;

const std::string serialized_privkey_path = "/tmp/lrm-test-privkey.pem";
const std::string serialized_pubkey_path = "/tmp/lrm-test-pubkey.pem";
const std::string serialized_cert_path = "/tmp/lrm-test-cert.pem";
std::unique_ptr<RsaKeyPair> key_pair;
std::unique_ptr<RsaCertificate> certificate;
std::string pem_privkey_content;

std::string bio_to_str(BIO* bio) {
  std::stringstream ss;
  char buffer[256];
  int readbytes = 0;
  while ((readbytes = BIO_read(bio, buffer, 255)) > 0) {
    buffer[readbytes] = '\0';
    ss << buffer;
  }

  return ss.str();
}

TEST(CertTest, RsaKeyPair_Generates) {
  EXPECT_NO_THROW(key_pair = std::make_unique<RsaKeyPair>());
}

TEST(CertTest, RsaKeyPair_SerializesPrivkeyToFile) {
  fs::remove(serialized_privkey_path);

  EXPECT_NO_THROW(key_pair->SerializePrivateKey(serialized_privkey_path));
  ASSERT_TRUE(lrm::Util::file_exists(serialized_privkey_path));

  std::string line;
  auto fs = std::ifstream(serialized_privkey_path);
  std::getline(fs, line);
  ASSERT_EQ(line, "-----BEGIN PRIVATE KEY-----");

  std::string last_line = line;
  while (std::getline(fs, line)) {
    if (line.empty()) break;
    last_line = line;
  }
  EXPECT_EQ(last_line, "-----END PRIVATE KEY-----");
}

TEST(CertTest, RsaKeyPair_SerializesPubkeyToFile) {
  fs::remove(serialized_pubkey_path);

  EXPECT_NO_THROW(key_pair->SerializePublicKey(serialized_pubkey_path));
  ASSERT_TRUE(lrm::Util::file_exists(serialized_pubkey_path));

  std::string line;
  auto fs = std::ifstream(serialized_pubkey_path);
  std::getline(fs, line);
  ASSERT_EQ(line, "-----BEGIN PUBLIC KEY-----");

  std::string last_line = line;
  while (std::getline(fs, line)) {
    if (line.empty()) break;
    last_line = line;
  }
  EXPECT_EQ(last_line, "-----END PUBLIC KEY-----");
}

TEST(CertTest, RsaKeyPair_DeserializeFromFile) {
  BIO* pk_bio = BIO_new(BIO_s_mem());
  PEM_write_bio_PrivateKey(pk_bio, key_pair->Get(), nullptr,
                           nullptr, 0, nullptr, nullptr);
  std::string key = bio_to_str(pk_bio);

  key_pair.reset();
  ASSERT_NO_THROW(
      key_pair = std::make_unique<RsaKeyPair>(serialized_privkey_path));
  PEM_write_bio_PrivateKey(pk_bio, key_pair->Get(), nullptr,
                           nullptr, 0, nullptr, nullptr);
  std::string file_key = bio_to_str(pk_bio);
  BIO_free(pk_bio);

  EXPECT_EQ(key, file_key);
}

// TODO: add password encrypted (de)serialization tests

TEST(CertTest, RsaCertificate_FromKeyPair) {
  EXPECT_NO_THROW(
      certificate = std::make_unique<RsaCertificate>(
          *key_pair,
          RsaCertificate::CertNameMap({{"countryName", "FO"},
                                       {"stateOrProvinceName", "BAR"},
                                       {"organizationName", "FooBar"},
                                       {"commonName", "CN"}})));
}

TEST(CertTest, RsaCertificate_VerifyWithoutSignThrows) {
  ASSERT_ANY_THROW(certificate->Verify(*key_pair));
}

TEST(CertTest, RsaCertificate_SelfSignAndVerify) {
  ASSERT_NO_THROW(certificate->Sign(*key_pair));

  bool verified = false;
  certificate->Verify(*key_pair);
  ASSERT_NO_THROW(verified = certificate->Verify(*key_pair));

  EXPECT_TRUE(verified);
}

TEST(CertTest, RsaCertificate_Serialize) {
  ASSERT_NO_THROW(certificate->Serialize(serialized_cert_path));

  std::string line;
  auto fs = std::ifstream(serialized_cert_path);
  std::getline(fs, line);
  ASSERT_EQ(line, "-----BEGIN CERTIFICATE-----");

  std::string last_line = line;
  while (std::getline(fs, line)) {
    if (line.empty()) break;
    last_line = line;
  }
  EXPECT_EQ(last_line, "-----END CERTIFICATE-----");
}

TEST(CertTest, RsaCertificate_DeserializeFromFile) {
  BIO* cert_bio = BIO_new(BIO_s_mem());
  PEM_write_bio_X509(cert_bio, certificate->Get());
  std::string cert_str = bio_to_str(cert_bio);

  certificate.reset();
  ASSERT_NO_THROW(
      certificate = std::make_unique<RsaCertificate>(serialized_cert_path));
  PEM_write_bio_X509(cert_bio, certificate->Get());

  EXPECT_EQ(cert_str, bio_to_str(cert_bio));

  BIO_free(cert_bio);
}
