// Copyright (C) 2020 by Jakub Wojciech

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

#include <openssl/bn.h>
#include <openssl/ec.h>

#include "crypto/CryptoUtil.h"

using namespace lrm::crypto;

TEST(REPEAT_CryptoUtil, ZKP) {
  auto gen_keypair = []() -> std::pair<EcScalar, EcPoint> {
    EcScalar privkey = generate_private_key();
    EcPoint pubkey = make_point();
    EC_POINT_mul(CurveGroup(), pubkey.get(), privkey.get(),
                 nullptr, nullptr, get_bnctx());
    return {std::move(privkey), std::move(pubkey)};
  };

  auto [privkey, pubkey] = gen_keypair();

  ASSERT_NE(privkey, nullptr) << "generate_private_key() result";
  ASSERT_FALSE(BN_is_zero(privkey.get())) << "generate_private_key() result";

  const EC_POINT* basepoint = EC_GROUP_get0_generator(CurveGroup());

  EcPoint random_point = gen_keypair().second;
  auto zkp = make_zkp("id", privkey.get(), pubkey.get(), basepoint);

  EXPECT_EQ(zkp.user_id, "id");
  EXPECT_FALSE(BN_is_zero(zkp.r.get()));
  EXPECT_FALSE(EC_POINT_is_at_infinity(CurveGroup(), zkp.V.get()));
  EXPECT_TRUE(EC_POINT_is_on_curve(CurveGroup(), zkp.V.get(), get_bnctx()));

  EXPECT_FALSE(check_zkp(zkp, pubkey.get(), "id", basepoint))
      << "Passes on bad id";
  EXPECT_TRUE(check_zkp(zkp, pubkey.get(), "another_id", basepoint))
      << "Fails on good id";
  EXPECT_FALSE(check_zkp(zkp, random_point.get(), "another_id", basepoint))
      << "Passes on wrong public key";
  EXPECT_FALSE(check_zkp(zkp, pubkey.get(), "another_id", random_point.get()))
      << "Passes on wrong generator";
}

TEST(CryptoUtil, zkp_serialization) {
  auto [k, K] = generate_key_pair(CurveGenerator());

  auto zkp = make_zkp("id", k.get(), K.get(), CurveGenerator());
  auto data = zkp.serialize();
  auto deserialized_zkp = zkp::deserialize(data);

  EXPECT_EQ(zkp.user_id, deserialized_zkp.user_id);
  EXPECT_EQ(0, BN_cmp(zkp.r.get(), deserialized_zkp.r.get()));
  EXPECT_EQ(0, EC_POINT_cmp(CurveGroup(),
                            zkp.V.get(), deserialized_zkp.V.get(),
                            get_bnctx()));
}

TEST(CryptoUtil, zkp_bad_serialization) {
  std::vector<unsigned char> buffer(sizeof(std::size_t) * 3);
  std::size_t *s1, *s2, *s3;

  s1 = reinterpret_cast<std::size_t*>(buffer.data());
  s2 = s1 + 1;
  s3 = s2 + 1;
  *s1 = *s2 = *s3 = 0;

  *s1 = sizeof(std::size_t) * 10;
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Buffer over-read by setting the size of user_id to reach outside "
      "of the buffer.\nSECURITY ISSUE!";
  *s1 = 1;
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Buffer over-read by setting the size of user_id to value inside "
      "the buffer, but buffer is too short to contain that data.\n"
      "SECURITY ISSUE!";

  *s1 = 0;
  *s2 = sizeof(std::size_t) * 10;
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Buffer over-read by setting the size of r.\nSECURITY ISSUE!";

  *s2 = 0;
  *s3 = sizeof(std::size_t) * 10;
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Buffer over-read by setting the size of V.\nSECURITY ISSUE!";

  *s3 = 0;
  *s1 = -1 - 1000;
  EXPECT_TRUE(buffer.data() > buffer.data() + sizeof(size_t) + *s1)
      << "Error in a test";
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Integer overflow on pointer by manipulating the size of user_id.\n"
      "SECURITY ISSUE!";

  *s1 = 0;
  *s2 = -1 - 1000;
  EXPECT_TRUE(buffer.data() > buffer.data() + sizeof(size_t) * 2 + *s2)
      << "Error in a test";
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Integer overflow on pointer by manipulating the size of r.\n"
      "SECURITY ISSUE!";

  *s2 = 0;
  *s3 = -1 - 1000;
  EXPECT_TRUE(buffer.data() > buffer.data() + sizeof(size_t) * 3 + *s3)
      << "Error in a test";
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Integer overflow on pointer by manipulating the size of V.\n"
      "SECURITY ISSUE!";

  *s3 = -1;
  EXPECT_ANY_THROW(zkp::deserialize(buffer))
      << "Very big value set as a size to the point of integer overflow on "
      "a pointer, but inside the buffer.\nSECURITY ISSUE!";
}
