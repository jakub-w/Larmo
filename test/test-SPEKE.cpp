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

#include <openssl/rand.h>

#include "SPEKE.h"

using namespace lrm::crypto;

// This sets up OpenSSL to not use random values
class SpekeTestStatic : public ::testing::Test {
 protected:
  static void test_rand_cleanup() {}
  static int test_rand_add(const void *buf, int num, double add_entropy) {
    return 1;
  }
  static int test_rand_status() { return 1; }

  static int test_rand_seed(const void *buf, int num) {
    return 1;
  }

  static int test_rand_bytes(unsigned char *buf, int num) {
    std::fill_n(buf, num, (unsigned char)0);
    return 1;
  }

  static RAND_METHOD test_rand_meth;

  static RAND_METHOD *RAND_test() { return &test_rand_meth; }

  static void SetUpTestCase() {
    old_rand_method = RAND_get_rand_method();
    RAND_set_rand_method(RAND_test());
  }

  static void TearDownTestCase() {
    RAND_set_rand_method(old_rand_method);
  }

  void SetUp() override {
    speke_ = new SPEKE("id", "password", 2692367);
  }

  void TearDown() override {
    delete speke_;
  }

  SPEKE* speke_;
  static Bytes remote_pubkey_;
  static std::string remote_id_;

  private:
  static const RAND_METHOD* old_rand_method;
};

RAND_METHOD SpekeTestStatic::test_rand_meth = {
        test_rand_seed,
        test_rand_bytes,
        test_rand_cleanup,
        test_rand_add,
        test_rand_bytes,
        test_rand_status
};

Bytes SpekeTestStatic::remote_pubkey_ = {0x15, 0xdd, 0xa1}; // 1432993
std::string SpekeTestStatic::remote_id_ = "remote-id";

const RAND_METHOD* SpekeTestStatic::old_rand_method;


TEST_F(SpekeTestStatic, GetPublicKey) {
  Bytes pubkey;
  EXPECT_NO_THROW(pubkey = speke_->GetPublicKey());
  Bytes control = {0x25, 0x1F, 0xE1}; // 2432993

  EXPECT_TRUE(pubkey == control);
}

TEST_F(SpekeTestStatic, GetEncryptionKey) {
  speke_->GetPublicKey();
  // 0x15DDA1 is
  speke_->ProvideRemotePublicKeyIdPair(remote_pubkey_, remote_id_);

  Bytes encryption_key = speke_->GetEncryptionKey();
  Bytes control =
      BigNum("3760030426667490725856578796054119828638057164983820132036")
      .to_bytes();

  int key_length = EVP_CIPHER_key_length(LRM_SPEKE_CIPHER_TYPE);

  EXPECT_EQ(key_length, encryption_key.size()) <<
      "Encryption key length doesn't match the LRM_SPEKE_CIPHER_TYPE";
  EXPECT_TRUE(encryption_key == control);
}

TEST_F(SpekeTestStatic, GetKeyConfirmationData) {
  speke_->GetPublicKey();
  speke_->ProvideRemotePublicKeyIdPair(remote_pubkey_, remote_id_);

  Bytes key_confirmation_data = speke_->GetKeyConfirmationData();
  Bytes control = BigNum(
      "1195714319814617418636809074804880777138384120623793181527475273946069"
      "6196564280087220354616556526488056639854211925099877329876934191614598"
      "793921035956980").to_bytes();

  EXPECT_TRUE(key_confirmation_data == control);
}

TEST(SpekeTest, RandomPublicKey) {
  std::set<Bytes> pubkeys;
  const int num_pubkeys = 10;

  for (auto i = 0; i < num_pubkeys; ++i) {
    SPEKE speke("id", "password", 2692367);
    pubkeys.insert(speke.GetPublicKey());
  }

  EXPECT_EQ(num_pubkeys, pubkeys.size());
}

TEST(SpekeTest, ProvideRemotePubkeyAndId_SameId) {
  SPEKE peer1("peer", "password", 2692367);
  SPEKE peer2("peer", "password", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  EXPECT_ANY_THROW(peer2.ProvideRemotePublicKeyIdPair(peer1_key, "peer"));
  EXPECT_ANY_THROW(peer1.ProvideRemotePublicKeyIdPair(peer2_key, "peer"));
}

TEST(SpekeTest, ConfirmKey) {
  SPEKE peer1("peer1", "password", 2692367);
  SPEKE peer2("peer2", "password", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, "peer1");
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, "peer2");

  auto peer1_kcd = peer1.GetKeyConfirmationData();
  auto peer2_kcd = peer2.GetKeyConfirmationData();

  EXPECT_FALSE(peer1_kcd == peer2_kcd);

  EXPECT_TRUE(peer2.ConfirmKey(peer1_kcd));
  EXPECT_TRUE(peer1.ConfirmKey(peer2_kcd));
}

TEST(SpekeTest, ConfirmKey_WrongPassword) {
  SPEKE peer1("peer1", "password1", 2692367);
  SPEKE peer2("peer2", "password2", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, "peer1");
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, "peer2");

  auto peer1_kcd = peer1.GetKeyConfirmationData();
  auto peer2_kcd = peer2.GetKeyConfirmationData();

  EXPECT_FALSE(peer2.ConfirmKey(peer1_kcd));
  EXPECT_FALSE(peer1.ConfirmKey(peer2_kcd));
}

TEST(SpekeTest, ConfirmKey_WrongPrime_BothSafe) {
  SPEKE peer1("peer1", "password", 2692367);
  SPEKE peer2("peer2", "password", 2686667);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, "peer1");
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, "peer2");

  auto peer1_kcd = peer1.GetKeyConfirmationData();
  auto peer2_kcd = peer2.GetKeyConfirmationData();

  EXPECT_FALSE(peer2.ConfirmKey(peer1_kcd));
  EXPECT_FALSE(peer1.ConfirmKey(peer2_kcd));
}

TEST(SpekeTest, HmacSign) {
  SPEKE peer1("peer1", "password", 2692367);
  SPEKE peer2("peer2", "password", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, "peer1");
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, "peer2");

  unsigned char msg[] = "message";
  Bytes msg_bytes;
  msg_bytes.reserve(sizeof(msg) - 1);
  std::copy_n(msg, sizeof(msg) - 1, std::back_inserter(msg_bytes));

  auto peer1_msg_hmac = peer1.HmacSign(msg_bytes);
  auto peer2_msg_hmac = peer2.HmacSign(msg_bytes);

  EXPECT_TRUE(peer1_msg_hmac == peer2_msg_hmac);
}

TEST(SpekeTest, HmacSign_WrongPassword) {
  SPEKE peer1("peer1", "password1", 2692367);
  SPEKE peer2("peer2", "password2", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, "peer1");
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, "peer2");

  unsigned char msg[] = "message";
  Bytes msg_bytes;
  msg_bytes.reserve(sizeof(msg) - 1);
  std::copy_n(msg, sizeof(msg) - 1, std::back_inserter(msg_bytes));

  auto peer1_msg_hmac = peer1.HmacSign(msg_bytes);
  auto peer2_msg_hmac = peer2.HmacSign(msg_bytes);

  EXPECT_FALSE(peer1_msg_hmac == peer2_msg_hmac);
}

TEST(SpekeTest, GetEncryptionKey_WithoutProvidingPkey) {
  SPEKE speke("id", "password", 2692367);

  EXPECT_ANY_THROW(speke.GetEncryptionKey());
}

TEST(SpekeTest, GetKeyConfirmationData_WithoutProvidingPkey) {
  SPEKE speke("id", "password", 2692367);

  EXPECT_ANY_THROW(speke.GetKeyConfirmationData());
}

TEST(SpekeTest, ConfirmKey_WithoutProvidingPkey) {
  SPEKE speke("id", "password", 2692367);

  EXPECT_ANY_THROW(speke.ConfirmKey({1, 1, 1}));
}

TEST(SpekeTest, HmacSign_WithoutProvidingPkey) {
  SPEKE speke("id", "password", 2692367);

  EXPECT_ANY_THROW(speke.HmacSign({1, 1, 1}));
}

TEST(SpekeTest, ImpersonationAttack) {
  SPEKE speke("id", "password", 2692367);

  EXPECT_ANY_THROW(
      speke.ProvideRemotePublicKeyIdPair(speke.GetPublicKey(), "id"));
}
