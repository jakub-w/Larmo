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

#include "CryptoUtil.h"

#include <cstring>
#include <openssl/ec.h>
#include <stdexcept>

#include <openssl/evp.h>

#include "Util.h"

namespace {
template<typename InputIt, typename OutputIt>
void safe_copy(const InputIt from, size_t count, const InputIt from_limit,
               OutputIt to, OutputIt to_limit) {
  if (from == nullptr or from_limit == nullptr or
      to == nullptr or to_limit == nullptr or
      count == 0) {
    throw std::invalid_argument("Any of arguments is either 0 or nullptr");
  }
  const auto from_end = from + count;
  const auto to_end = to + count;
  if (from_end < from or
      from_limit < from_end or
      to_end < to or
      to_limit < to_end) {
    throw std::range_error("Invalid access");
  }

  std::memcpy(to, from, count);
}
}

namespace lrm::crypto {
ShaHash encode_SHA512(std::string_view data) {
  static std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx{
    EVP_MD_CTX_new(), &EVP_MD_CTX_free};

  EVP_MD_CTX_reset(ctx.get());


  EVP_DigestInit_ex(ctx.get(), EVP_sha3_512(), nullptr);
  EVP_DigestUpdate(ctx.get(), data.data(), data.length());

  ShaHash hash;

#ifndef NDEBUG
  unsigned int hash_len;
  EVP_DigestFinal_ex(ctx.get(), hash.data(), &hash_len);
  assert(hash_len == hash.size());
#else
  EVP_DigestFinal_ex(ctx.get(), hash.data(), nullptr);
#endif

  return hash;
}

EcPoint make_generator(std::string_view password) {
  const auto hash = encode_SHA512(password);
  EcScalar num = make_scalar(BN_bin2bn(hash.data(), hash.size(), nullptr));

  static thread_local std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)> bnctx{
    BN_CTX_secure_new(), &BN_CTX_free};
  if (nullptr == bnctx.get()) {
    int_error("Failed to create BN_CTX object");
  }

  auto result = make_point();
  if (not EC_POINT_mul(CurveGroup(), result.get(), nullptr,
                       EC_GROUP_get0_generator(CurveGroup()), num.get(),
                       bnctx.get())) {
    int_error("Failed to perform scalar multiplication on EC");
  }

  if (EC_POINT_is_at_infinity(CurveGroup(), result.get())) {
    return make_generator(std::string_view{
        reinterpret_cast<const char*>(hash.data()),
        hash.size()});
  }

  return result;
}

EcScalar generate_private_key() {
  // TODO: handle errors
  EcScalar result = make_scalar();

  // order
  EcScalar order_minus_two =
      make_scalar(BN_dup(CurveGroupOrder()));
  // order - 2
  BN_sub_word(order_minus_two.get(), 2);

  // random [0, order - 2]
  BN_priv_rand_range(result.get(), order_minus_two.get());
  // random [1, order - 1]
  BN_add_word(result.get(), 1);

  return result;
}

std::pair<EcScalar, EcPoint> generate_key_pair(const EC_POINT* generator) {
  EcScalar privkey = generate_private_key();
  EcPoint pubkey = make_point();

  EC_POINT_mul(CurveGroup(), pubkey.get(), nullptr,
               generator, privkey.get(), get_bnctx());

  return {std::move(privkey), std::move(pubkey)};
}

std::vector<unsigned char> EcPointToBytes(
    const EC_POINT* p,
    point_conversion_form_t form) {
  std::size_t oct_size = EC_POINT_point2oct(CurveGroup(), p, form,
                                            nullptr, 0, get_bnctx());
  if (0 == oct_size) {
    int_error("Failed to determine the size of octet string for converting "
              "EC_POINT");
  }

  std::vector<unsigned char> result(oct_size);
  if (not EC_POINT_point2oct(CurveGroup(), p, form,
                             result.data(), result.size(), get_bnctx())) {
    int_error("Failed to convert EC_POINT to an octet string");
  }

  return result;
}

EcPoint BytesToEcPoint(const unsigned char* data, std::size_t size) {
  assert(data != nullptr);

  EcPoint point = make_point();
  if (not EC_POINT_oct2point(CurveGroup(), point.get(),
                             data, size, get_bnctx())) {
    int_error("Failed to convert data to EC_POINT");
  }

  return point;
}

EcScalar make_zkp_challenge(const EC_POINT* V,
                            const EC_POINT* public_key,
                            std::string_view user_id,
                            const EC_POINT* generator) {
  // challenge: H(gen || V || pubkey || user_id)
  // auto bio = make_bio(BIO_s_mem());
  // BIO_write_ex(bio.get(), const void *data, size_t dlen, size_t *written)

  static thread_local std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>
      ctx{EVP_MD_CTX_new(), &EVP_MD_CTX_free};

  EVP_MD_CTX_reset(ctx.get());

  auto V_vec = EcPointToBytes(V);
  auto pubkey_vec = EcPointToBytes(public_key);
  auto gen_vec = EcPointToBytes(generator);

  EVP_DigestInit_ex(ctx.get(), HASH_TYPE, nullptr);
  EVP_DigestUpdate(ctx.get(), gen_vec.data(), gen_vec.size());
  EVP_DigestUpdate(ctx.get(), V_vec.data(), V_vec.size());
  EVP_DigestUpdate(ctx.get(), pubkey_vec.data(), pubkey_vec.size());
  EVP_DigestUpdate(ctx.get(), user_id.data(), user_id.size());

  ShaHash hash;

#ifndef NDEBUG
  unsigned int hash_len;
  EVP_DigestFinal_ex(ctx.get(), hash.data(), &hash_len);
  assert(hash_len == hash.size());
#else
  EVP_DigestFinal_ex(ctx.get(), hash.data(), nullptr);
#endif

  auto result = make_scalar();
  BN_bin2bn(hash.data(), hash.size(), result.get());
  assert(not BN_is_negative(result.get()));

  // BN_mod(result.get(), result.get(),
  //        CurveGroupOrder(), get_bnctx());

  return result;
}

zkp make_zkp(std::string_view user_id,
             const BIGNUM* private_key,
             const EC_POINT* public_key,
             const EC_POINT* generator) {

  // random v and V = G x [v]
  auto [v, V] = generate_key_pair(generator);

  // challenge: H(gen || V || pubkey || user_id)
  EcScalar c = make_zkp_challenge(V.get(), public_key, user_id, generator);

  // challenge response (r)
  // privkey * c
  EcScalar r = make_scalar();
  BN_mod_mul(r.get(), private_key, c.get(),
             CurveGroupOrder(), get_bnctx());
  // v - (privkey * c)
  BN_mod_sub(r.get(), v.get(), r.get(), CurveGroupOrder(), get_bnctx());

  return zkp{user_id.data(), std::move(V), std::move(r)};
}

bool check_zkp(const zkp& zkp,
               const EC_POINT* public_key,
               std::string_view local_id,
               const EC_POINT* generator) {
  auto is_valid_point = [](const EC_POINT* point){
    return EC_POINT_is_on_curve(CurveGroup(), point, get_bnctx()) and
        (not EC_POINT_is_at_infinity(CurveGroup(), point));
  };

  if (not is_valid_point(public_key)) {
    return false;
  }

  // Check if public_key x [cofactor] == inf
  {
    EcPoint test_point = make_point();
    EC_POINT_mul(CurveGroup(), test_point.get(), nullptr,
                 public_key, EC_GROUP_get0_cofactor(CurveGroup()),
                 get_bnctx());
    if (EC_POINT_is_at_infinity(CurveGroup(), test_point.get())) {
      return false;
    }
  }

  if (not is_valid_point(zkp.V.get())) {
    return false;
  }
  if (zkp.user_id == local_id) {
    return false;
  }
  if (zkp.user_id.empty()) {
    return false;
  }

  EcPoint V = make_point();
  EcScalar c = make_zkp_challenge(zkp.V.get(), public_key,
                                  zkp.user_id, generator);
  const EC_POINT* points[2] = {generator, public_key};
  const BIGNUM* nums[2] = {zkp.r.get(), c.get()};
  EC_POINTs_mul(CurveGroup(), V.get(), nullptr, 2, points, nums, get_bnctx());

  return 0 == EC_POINT_cmp(CurveGroup(), V.get(), zkp.V.get(), get_bnctx());
}

std::vector<unsigned char> zkp::serialize() const {
    unsigned char* V_buffer = nullptr;
    const std::size_t V_size = EC_POINT_point2buf(CurveGroup(), V.get(),
                                                  POINT_CONVERSION_COMPRESSED,
                                                  &V_buffer, get_bnctx());
    if (0 == V_size) {
      int_error("Failed converting EC_POINT to the octet string");
    }

    const std::size_t user_id_size = user_id.size();
    const std::size_t r_size = BN_num_bytes(r.get());

    std::vector<unsigned char> result(user_id_size +
                                      V_size +
                                      r_size +
                                      3 * sizeof(std::size_t));

    unsigned char* address = result.data();

    Util::safe_memcpy(address, &user_id_size, sizeof(user_id_size));
    address += sizeof(user_id_size);
    Util::safe_memcpy(address, user_id.data(), user_id_size);
    address += user_id_size;

    Util::safe_memcpy(address, &V_size, sizeof(V_size));
    address += sizeof(V_size);
    Util::safe_memcpy(address, V_buffer, V_size);
    address += V_size;

    Util::safe_memcpy(address, &r_size, sizeof(r_size));
    address += sizeof(r_size);
    BN_bn2bin(r.get(), address);

    assert(address + r_size == result.data() + result.size());

    return result;
  }

zkp zkp::deserialize(const unsigned char* data, std::size_t size) {
  const std::range_error err("Invalid access");

  const unsigned char* address = data;
  const unsigned char* const address_limit = data + size;

  std::size_t user_id_size = 0;
  safe_copy(address, sizeof(user_id_size), address_limit,
            &user_id_size, &user_id_size + sizeof(user_id_size));
  address += sizeof(user_id_size);
  if (user_id_size > size - std::distance(data, address)) {
    throw err;
  }

  const unsigned char* const user_id_ptr = address;
  address += user_id_size;

  std::size_t V_size = 0;
  safe_copy(address, sizeof(V_size), address_limit,
            &V_size, &V_size + sizeof(V_size));
  address += sizeof(V_size);
  if (V_size > size - std::distance(data, address)) {
    throw err;
  }

  const unsigned char* const V_ptr = address;
  address += V_size;

  std::size_t r_size = 0;
  safe_copy(address, sizeof(r_size), address_limit,
            &r_size, &r_size + sizeof(r_size));
  address += sizeof(r_size);
  if (r_size > size - std::distance(data, address)) {
    throw err;
  }

  const unsigned char* const r_ptr = address;
  address += r_size;

  if (address > address_limit) {
    throw err;
  }

  assert(address == data + size);

  auto result = zkp{
    std::string{user_id_ptr, user_id_ptr + user_id_size},
    [=]{
      EcPoint point = make_point();
      EC_POINT_oct2point(CurveGroup(), point.get(),
                         V_ptr, V_size, get_bnctx());
      return point;
    }(),
    make_scalar(BN_bin2bn(r_ptr, r_size, nullptr))
  };

  if (not (result.r and result.V)) {
    int_error("Failed to deserialize zkp");
  }

  return result;
}
}
