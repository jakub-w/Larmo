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

#include <cstring>

#include <openssl/rand.h>
#include <openssl/md5.h>

#include "crypto/SPEKE.h"
#include "crypto/config.h"
#include "Util.h"

using namespace lrm::crypto;

// This sets up OpenSSL to not use random values
class SpekeTestStatic : public ::testing::Test {
 protected:
  static void test_rand_cleanup() {}
  static int test_rand_add(const void*, int, double) {

    return 1;
  }
  static int test_rand_status() { return 1; }

  static int test_rand_seed(const void*, int) {
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

Bytes SpekeTestStatic::remote_pubkey_ = BigNum(1432993).to_bytes();
std::string SpekeTestStatic::remote_id_ = "remote-id";

const RAND_METHOD* SpekeTestStatic::old_rand_method;


TEST_F(SpekeTestStatic, GetPublicKey) {
  Bytes pubkey;
  EXPECT_NO_THROW(pubkey = speke_->GetPublicKey());
  Bytes control = BigNum(2432993).to_bytes();

  EXPECT_TRUE(pubkey == control);
}

TEST_F(SpekeTestStatic, GetId_IdLength) {
  std::string id;
  EXPECT_NO_THROW(id = speke_->GetId());

  EXPECT_EQ(id.size(), 35);
}

// This uses static seed so the only factor that should guarantee uniqueness
// of the id is time of creation.
TEST_F(SpekeTestStatic, GetId_UniqueId) {
  std::set<std::string> ids;
  const int num_ids = 100;

  for (auto i = 0; i < num_ids; ++i) {
    SPEKE speke("id", "password", 2692367);
    ids.insert(speke.GetId());
  }

  EXPECT_EQ(ids.size(), num_ids);
}

// This test case was disabled because the encryption key relies on the ids.
// Since the id depends on the time of its creation it is unique every time
// even when the RNG always returns the same value.
// Therefore the control value isn't enough to detect if the encryption key
// was generated properly.
TEST_F(SpekeTestStatic, DISABLED_GetEncryptionKey) {
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

// TODO: Test for GetNonce()

// This test case was disabled because the key confirmation data relies on
// the ids.
// Since the id depends on the time of its creation it is unique every time
// even when the RNG always returns the same value.
// Therefore the control value isn't enough to detect if the key confirmation
// data was generated properly.
TEST_F(SpekeTestStatic, DISABLED_GetKeyConfirmationData) {
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

  EXPECT_ANY_THROW(peer2.ProvideRemotePublicKeyIdPair(peer1_key,
                                                      peer2.GetId()));
  EXPECT_ANY_THROW(peer1.ProvideRemotePublicKeyIdPair(peer2_key,
                                                      peer1.GetId()));
}

TEST(SpekeTest, EncryptionKey_SameForBoth) {
  SPEKE peer1("peer1", "password", 2692367);
  SPEKE peer2("peer2", "password", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

  EXPECT_EQ(peer1.GetEncryptionKey(), peer2.GetEncryptionKey());
}

TEST(SpekeTest, ConfirmKey) {
  SPEKE peer1("peer1", "password", 2692367);
  SPEKE peer2("peer2", "password", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

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

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

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

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

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

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

  const unsigned char msg[] = "message";
  Bytes msg_bytes(sizeof(msg) - 1);
  lrm::Util::safe_memcpy(msg_bytes.data(), msg, msg_bytes.size());

  auto peer1_msg_hmac = peer1.HmacSign(msg_bytes);
  auto peer2_msg_hmac = peer2.HmacSign(msg_bytes);

  EXPECT_TRUE(peer1_msg_hmac == peer2_msg_hmac);
}

TEST(SpekeTest, HmacSign_WrongPassword) {
  SPEKE peer1("peer1", "password1", 2692367);
  SPEKE peer2("peer2", "password2", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

  const unsigned char msg[] = "message";
  Bytes msg_bytes(sizeof(msg) - 1);
  lrm::Util::safe_memcpy(msg_bytes.data(), msg, msg_bytes.size());

  auto peer1_msg_hmac = peer1.HmacSign(msg_bytes);
  auto peer2_msg_hmac = peer2.HmacSign(msg_bytes);

  EXPECT_FALSE(peer1_msg_hmac == peer2_msg_hmac);
}

TEST(SpekeTest, ConfirmHmacSignature) {
  SPEKE peer1("peer1", "password", 2692367);
  SPEKE peer2("peer2", "password", 2692367);

  auto peer1_key = peer1.GetPublicKey();
  auto peer2_key = peer2.GetPublicKey();

  peer2.ProvideRemotePublicKeyIdPair(peer1_key, peer1.GetId());
  peer1.ProvideRemotePublicKeyIdPair(peer2_key, peer2.GetId());

  const unsigned char msg[] = "message";
  Bytes msg_bytes(sizeof(msg) - 1);
  lrm::Util::safe_memcpy(msg_bytes.data(), msg, msg_bytes.size());

  auto peer1_msg_hmac = peer1.HmacSign(msg_bytes);

  EXPECT_TRUE(peer2.ConfirmHmacSignature(peer1_msg_hmac, msg_bytes));
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

  EXPECT_ANY_THROW(speke.ConfirmKey(Bytes()));
}

TEST(SpekeTest, HmacSign_WithoutProvidingPkey) {
  SPEKE speke("id", "password", 2692367);

  EXPECT_ANY_THROW(speke.HmacSign(Bytes()));
}

TEST(SpekeTest, ImpersonationAttack) {
  const int safe_prime = 2692367;
  SPEKE session1("id", "password", safe_prime);
  SPEKE session2("id", "password", safe_prime);

  auto z = RandomInRange(BigNum(2), BigNum(safe_prime - 2));

  BigNum g_x(session1.GetPublicKey());
  BigNum g_xz = g_x.ModExp(z, safe_prime);

  session2.ProvideRemotePublicKeyIdPair(g_xz.to_bytes(), "attacker");

  BigNum g_y(session2.GetPublicKey());
  BigNum g_yz = g_y.ModExp(z, safe_prime);

  session1.ProvideRemotePublicKeyIdPair(g_yz.to_bytes(), "attacker");
  // g^xyz should be now a base for the encryption key generation for both
  // sessions.

  EXPECT_FALSE(session1.ConfirmKey(session2.GetKeyConfirmationData()));
  EXPECT_FALSE(session1.ConfirmKey(session1.GetKeyConfirmationData()));
  EXPECT_FALSE(session2.ConfirmKey(session2.GetKeyConfirmationData()));
  EXPECT_FALSE(session2.ConfirmKey(session1.GetKeyConfirmationData()));
  EXPECT_NE(session1.GetEncryptionKey(), session2.GetEncryptionKey());
}
