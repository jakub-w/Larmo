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

#include "crypto/certs/Certificate.h"
#include "crypto/certs/EddsaKeyPair.h"
#include "crypto/certs/RsaKeyPair.h"

#include <openssl/pem.h>

#include "filesystem.h"
#include "Util.h"

using namespace lrm::crypto::certs;

const fs::path serialized_privkey_path = "/tmp/lrm-test-privkey.pem";
const fs::path serialized_pubkey_path = "/tmp/lrm-test-pubkey.pem";
const std::string serialized_cert_path = "/tmp/lrm-test-cert.pem";

template <typename T>
class KeyPairTest : public ::testing::Test {
 public:
  static std::unique_ptr<T> key_pair;
};
template <typename T>
std::unique_ptr<T> KeyPairTest<T>::key_pair;

using KeyPairTypes = ::testing::Types<RsaKeyPair, EddsaKeyPair>;
TYPED_TEST_CASE(KeyPairTest, KeyPairTypes);

TYPED_TEST(KeyPairTest, Constructs) {
  EXPECT_NO_THROW(TestFixture::key_pair = std::make_unique<TypeParam>());
}

TYPED_TEST(KeyPairTest, Generates) {
  EXPECT_NO_THROW(TestFixture::key_pair->Generate());
}

TYPED_TEST(KeyPairTest, SerializesPrivkeyToPemFile) {
  fs::remove(serialized_privkey_path);

  EXPECT_NO_THROW(
      TestFixture::key_pair->ToPemFilePrivKey(serialized_privkey_path));
  ASSERT_TRUE(lrm::Util::file_exists(serialized_privkey_path.c_str()));

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

TYPED_TEST(KeyPairTest, SerializesPubkeyToPemFile) {
  fs::remove(serialized_pubkey_path);

  EXPECT_NO_THROW(
      TestFixture::key_pair->ToPemFilePubKey(serialized_pubkey_path));
  ASSERT_TRUE(lrm::Util::file_exists(serialized_pubkey_path.c_str()));

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

TYPED_TEST(KeyPairTest, ToPemString) {
  std::string key_pair_str;
  ASSERT_NO_THROW(key_pair_str = TestFixture::key_pair->ToPemPrivKey());
  ASSERT_FALSE(key_pair_str.empty());

  EXPECT_EQ(0, key_pair_str.find("-----BEGIN PRIVATE KEY-----"))
      << "The PEM private key string should start with "
      "-----BEGIN PRIVATE KEY-----";
}

TYPED_TEST(KeyPairTest, DeserializeFromFile) {
  std::string key = TestFixture::key_pair->ToPemPrivKey();

  TestFixture::key_pair.reset(new TypeParam);
  ASSERT_NO_THROW(
      TestFixture::key_pair->FromPemFile(serialized_privkey_path));

  EXPECT_EQ(key, TestFixture::key_pair->ToPemPrivKey());
}

// TODO: add password encrypted (de)serialization tests
// TODO: add tests for DER serialization


// -------------------- CERTIFICATE TESTS --------------------

class CertificateTest : public ::testing::Test {
 public:
  static void SetUpTestCase() {
    key_pair = std::make_unique<EddsaKeyPair>();
    key_pair->Generate();
  }

  static std::unique_ptr<EddsaKeyPair> key_pair;
  static std::unique_ptr<Certificate> certificate;
};
std::unique_ptr<EddsaKeyPair> CertificateTest::key_pair;
std::unique_ptr<Certificate> CertificateTest::certificate;

TEST_F(CertificateTest, FromKeyPair) {
  EXPECT_NO_THROW(
      certificate = std::make_unique<Certificate>(
          *key_pair,
          Certificate::CertNameMap({{"countryName", "FO"},
                                    {"stateOrProvinceName", "BAR"},
                                    {"organizationName", "FooBar"},
                                    {"commonName", "CN"}})));
}

TEST_F(CertificateTest, VerifyWithoutSignThrows) {
  ASSERT_THROW(certificate->Verify(*key_pair), std::runtime_error);
}

TEST_F(CertificateTest, SelfSignAndVerify) {
  certificate->Sign(*key_pair);
  ASSERT_NO_THROW(certificate->Sign(*key_pair));

  bool verified = false;
  certificate->Verify(*key_pair);
  ASSERT_NO_THROW(verified = certificate->Verify(*key_pair));

  EXPECT_TRUE(verified);
}

TEST_F(CertificateTest, Serialize) {
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

TEST_F(CertificateTest, ToString) {
  std::string cert_str;
  ASSERT_NO_THROW(cert_str = certificate->ToString());
  ASSERT_FALSE(cert_str.empty());

  EXPECT_EQ(0, cert_str.find("-----BEGIN CERTIFICATE-----"))
      << "The PEM certificate string should start with "
      "-----BEGIN CERTIFICATE-----";
}

TEST_F(CertificateTest, DeserializeFromFile) {
  std::string cert_str = certificate->ToString();

  certificate = std::make_unique<Certificate>();
  ASSERT_NO_THROW(certificate->Deserialize(serialized_cert_path));
  EXPECT_EQ(cert_str, certificate->ToString());
}

TEST_F(CertificateTest, ConstructFromPemString) {
  std::string cert_str = certificate->ToString();

  certificate.reset();
  ASSERT_NO_THROW(certificate = std::make_unique<Certificate>(cert_str));
  ASSERT_NE(certificate.get(), nullptr);
  EXPECT_EQ(cert_str, certificate->ToString());
}
