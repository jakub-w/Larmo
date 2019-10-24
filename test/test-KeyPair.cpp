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

#include <fstream>

#include <openssl/evp.h>

#include "filesystem.h"
#include "crypto/certs/KeyPair.h"

using namespace lrm::crypto::certs;

bool check_pem_file_contents(std::string_view filename);

class KeyPairTest
    : public ::testing::TestWithParam<const KeyPair::keypair_t*> {
 protected:
  static constexpr char eddsa_pem[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MC4CAQAwBQYDK2VwBCIEIBdt7V+KtYiuSAqS6YKVybel/asHvzMemhSF3/OnPlGh\n"
      "-----END PRIVATE KEY-----\n";
  static constexpr char rsa_pem[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIBVQIBADANBgkqhkiG9w0BAQEFAASCAT8wggE7AgEAAkEAtHQ2BSHgzrsrnBNB\n"
      "ZZBZZ8U6FOJ6GrKTe76vhAp9s9/FlShQNsIqYiikACyvhPmWIpI9u8G0atQ24wOq\n"
      "jb/aRwIDAQABAkEAjJNTWeAwbxvkNyvQ8DvpSiucuZRMpuuidO3xcR1zG2HSIH+l\n"
      "/C3ow5rXl2Lw0POxFAem9+hCSPfGY0pFhE2mIQIhAOEX/f+Rgo3zBN5ble4lbCcG\n"
      "8SNs0MiuvbEaG6AR5BqJAiEAzTsjXN8Ipk4B2W2eljDOf9jYYBl+F6nyKYGerX7r\n"
      "2k8CIAkyp8hns8QFKC/F4kyG7vJxUC04ZxesPEgeXv6dfIqxAiB5ypqVxpXve2OF\n"
      "kJQINTaWkCz3+qjliij3kMCF3UhB6QIhANR5UwuvVDshR1gAl2X9af88gRX6g1uT\n"
      "cekKXXLpKJ6e\n"
      "-----END PRIVATE KEY-----\n";

  static constexpr char priv_key_file[] = "lrm-test-priv-key.pem";
  static constexpr char pub_key_file[] = "lrm-test-pub-key.pem";

  auto GetPem() const {
    switch(GetParam()->type) {
      case EVP_PKEY_ED25519: return eddsa_pem;
      case EVP_PKEY_RSA: return rsa_pem;
      default: return "Unknown";
    }
  }

  const std::remove_pointer_t<ParamType>& Param() {
    return *GetParam();
  }

  void SetUp() {
    fs::remove(priv_key_file);
    fs::remove(pub_key_file);
  }
};

TEST_P(KeyPairTest, FromPem) {
  EXPECT_NO_THROW(KeyPair::FromPem(Param(), GetPem()));
}

TEST_P(KeyPairTest, ToPemPubKey) {
  auto kp = KeyPair::FromPem(Param(), GetPem());

  auto pubkey = kp.ToPemPubKey();

  ASSERT_NE(0, pubkey.size());
  EXPECT_EQ("-----BEGIN PUBLIC KEY-----", pubkey.substr(0, 26));
}

TEST_P(KeyPairTest, ToPemPrivKey) {
  auto kp = KeyPair::FromPem(Param(), GetPem());

  EXPECT_EQ(GetPem(), kp.ToPemPrivKey());
}

TEST_P(KeyPairTest, ToPemFilePrivKey) {
  auto kp = KeyPair::FromPem(Param(), GetPem());

  kp.ToPemFilePrivKey(priv_key_file);
  EXPECT_TRUE(check_pem_file_contents(priv_key_file));
}

TEST_P(KeyPairTest, FromPemFile) {
  {
    auto kp = KeyPair::FromPem(Param(), GetPem());
    kp.ToPemFilePrivKey(priv_key_file);
  }

  auto kp = KeyPair::FromPemFile(Param(), priv_key_file);
  EXPECT_EQ(kp.ToPemPrivKey(), GetPem());
}

TEST_P(KeyPairTest, DISABLED_ToPemFilePrivKey_Password) {
  auto kp = KeyPair::FromPem(Param(), GetPem());

  kp.ToPemFilePrivKey(priv_key_file, "password");
  EXPECT_TRUE(check_pem_file_contents(priv_key_file));
}

TEST_P(KeyPairTest, DISABLED_FromPemFile_Password) {
  {
    auto kp = KeyPair::FromPem(Param(), GetPem());
    kp.ToPemFilePrivKey(priv_key_file, "password");
  }

  auto kp = KeyPair::FromPemFile(Param(), priv_key_file, "password");
  EXPECT_EQ(kp.ToPemPrivKey(), GetPem());
}

TEST_P(KeyPairTest, ToDerPrivKey) {
  auto kp = KeyPair::FromPem(Param(), GetPem());

  auto der = kp.ToDerPrivKey();
  ASSERT_GT(der.size(), 0);
}

TEST_P(KeyPairTest, ToDerPubKey) {
  auto kp = KeyPair::FromPem(Param(), GetPem());

  auto der = kp.ToDerPubKey();
  ASSERT_GT(der.size(), 0);
}

TEST_P(KeyPairTest, FromDer) {
  lrm::crypto::Bytes der;
  {
    auto kp = KeyPair::FromPem(Param(), GetPem());
    der = kp.ToDerPrivKey();
  }

  auto kp = KeyPair::FromDer(Param(), der);
  EXPECT_EQ(kp.ToPemPrivKey(), GetPem());
}

TEST_P(KeyPairTest, Generate) {
  auto kp = KeyPair::Generate(Param());

  EXPECT_GT(kp.ToPemPrivKey().size(), 0);
}

INSTANTIATE_TEST_CASE_P(
    KeyPairTest, KeyPairTest,
    testing::Values(&KeyPair::ED25519(),
                    &KeyPair::RSA()),
    [](const testing::TestParamInfo<KeyPairTest::ParamType>& info){
      switch(info.param->type) {
        case EVP_PKEY_ED25519: return "ED25519";
        case EVP_PKEY_RSA: return "RSA";
        default: return "Unknown";
      }
    });
