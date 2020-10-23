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

#ifndef LRM_CRYPTOUTIL_H_
#define LRM_CRYPTOUTIL_H_

#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>

#include "crypto/SslUtil.h"
#include "Util.h"

namespace lrm::crypto {
using ShaHash = std::array<unsigned char, SHA512_DIGEST_LENGTH>;
using EcPoint = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>;
using EcScalar = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

static constexpr auto LRM_CURVE_NID = NID_X9_62_prime256v1;
static const auto LRM_SESSION_KEY_SIZE = 64;

#define HASH_TYPE EVP_sha3_512()

struct zkp {
  std::string user_id;
  // std::string other_info;

  // V = G x [v], where v is a random number
  EcPoint V;

  // r = v - privkey * c, where c = H(gen || V || pubkey || user_id)
  EcScalar r;

  std::vector<unsigned char> serialize();
  static zkp deserialize(const std::vector<unsigned char>& data);
};

ShaHash encode_SHA512(std::string_view data);

template<typename Container>
std::string to_hex(Container c) {
  const unsigned char* buf =
      reinterpret_cast<const unsigned char*>(std::data(c));
  const auto size = std::size(c);
  constexpr auto value_size = sizeof(typename decltype(c)::value_type);

  auto output = std::string(size * value_size * 2, ' ');

  for (size_t i = 0; i < size; ++i) {
    std::snprintf(output.data() + (i * value_size * 2), 3, "%02x", buf[i]);
  }

  return output;
}

inline BN_CTX* get_bnctx() {
  static thread_local std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)> bnctx{
    BN_CTX_secure_new(), &BN_CTX_free};

  return bnctx.get();
}

inline static const EC_GROUP* CurveGroup() {
  static const std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)> group{
    []{
      EC_GROUP* group = EC_GROUP_new_by_curve_name(LRM_CURVE_NID);
      if (not group) {
        int_error(std::string("Failed to create EC_GROUP"));
      }
      return group;
    }(),
    &EC_GROUP_free};

  return group.get();
}

inline static const BIGNUM* Curve_group_order() {
  // static const std::unique_ptr<BIGNUM, decltype(&BN_free)> order{
  //   []{
  //     BIGNUM* num = BN_new();
  //     const int result = EC_GROUP_get_order(CurveGroup(), num, get_bnctx());
  //     assert(result != 0 && "Group NIST P-256 not initialized?");
  //     return num;
  //   }(),
  //   &BN_free
  // };
  static const std::unique_ptr<BIGNUM, decltype(&BN_free)> order{
    BN_dup(EC_GROUP_get0_order(CurveGroup())), &BN_free};

  return order.get();
}

inline EcPoint make_point() {
  auto result = EcPoint{EC_POINT_new(CurveGroup()), &EC_POINT_free};
  if (nullptr == result.get()) {
    int_error("Failed to create EC_POINT object");
  }
  return result;
}

inline EcScalar make_scalar(BIGNUM* bn) {
  auto result = EcScalar{bn, &BN_free};
  if (nullptr == result.get()) {
    int_error("Failed to create BIGNUM object");
  }
  return result;
}

inline EcScalar make_scalar() {
  return make_scalar(BN_new());
}

EcPoint make_generator(std::string_view password);

EcScalar generate_private_key();

std::vector<unsigned char> EcPointToBytes(
    const EC_POINT* p,
    point_conversion_form_t form = POINT_CONVERSION_UNCOMPRESSED);

EcScalar make_zkp_challenge(const EC_POINT* V,
                            const EC_POINT* public_key,
                            std::string_view user_id,
                            const EC_POINT* generator);

zkp make_zkp(std::string_view user_id,
             const BIGNUM* private_key,
             const EC_POINT* public_key,
             const EC_POINT* generator);

bool check_zkp(const zkp& zkp,
               const EC_POINT* public_key,
               std::string_view local_id,
               const EC_POINT* generator);
}

#endif  // LRM_CRYPTOUTIL_H_
