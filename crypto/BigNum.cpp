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

#include "BigNum.h"

#include <string>
#include <stdexcept>

namespace lrm::crypto {
BigNum::BigNum() noexcept : bignum_(BN_new()), ctx_(BN_CTX_new()) {}

BigNum::BigNum(const BIGNUM* bignum) noexcept : BigNum() {
  BN_copy(bignum_, bignum);
}

BigNum::BigNum(std::string_view dec_num_str) noexcept : BigNum() {
  BN_dec2bn(&bignum_, dec_num_str.data());
}

BigNum::BigNum(BN_ULONG num) noexcept : BigNum() {
  BN_set_word(bignum_, num);
}

BigNum::BigNum(const std::vector<unsigned char>& bytes) noexcept : BigNum() {
  BN_bin2bn((unsigned char*)bytes.data(), bytes.size(), bignum_);
}

BigNum::BigNum(const unsigned char* bytes, size_t size) noexcept : BigNum() {
  BN_bin2bn(bytes, size, bignum_);
}

BigNum::BigNum(const BigNum& other) noexcept : BigNum() {
  BN_copy(this->bignum_, other.bignum_);
}

BigNum::BigNum(BigNum&& other) noexcept {
  ctx_ = other.ctx_;
  other.ctx_ = nullptr;

  bignum_ = other.bignum_;
  other.bignum_ = nullptr;
}

BigNum::~BigNum() noexcept {
  BN_free(bignum_);
  BN_CTX_free(ctx_);
}

BigNum& BigNum::operator=(const BigNum& other) noexcept {
  if (this != &other) {
    BN_copy(this->bignum_, other.bignum_);
  }
  return *this;
}

BigNum& BigNum::operator+=(const BigNum& rhs) noexcept {
  BN_add(bignum_, bignum_, rhs.bignum_);
  return *this;
}

BigNum operator+(BigNum lhs, const BigNum& rhs) noexcept {
  lhs += rhs;
  return lhs;
}

BigNum& BigNum::operator-=(const BigNum& rhs) noexcept {
  BN_sub(bignum_, bignum_, rhs.bignum_);
  return *this;
}

BigNum operator-(BigNum lhs, const BigNum& rhs) noexcept {
  lhs -= rhs;
  return lhs;
}

BigNum& BigNum::operator*=(const BigNum& rhs) noexcept {
  BN_mul(bignum_, bignum_, rhs.bignum_, ctx_);
  return *this;
}

BigNum operator*(BigNum lhs, const BigNum& rhs) noexcept {
  lhs *= rhs;
  return lhs;
}

BigNum& BigNum::operator/=(const BigNum& rhs) noexcept {
  BN_div(bignum_, NULL, bignum_, rhs.bignum_, ctx_);
  return *this;
}

BigNum operator/(BigNum lhs, const BigNum& rhs) noexcept {
  lhs /= rhs;
  return lhs;
}

BigNum& BigNum::operator%=(const BigNum& rhs) noexcept {
  BN_mod(bignum_, bignum_, rhs.bignum_, ctx_);
  return *this;
}

BigNum operator%(BigNum lhs, const BigNum& rhs) noexcept {
  lhs %= rhs;
  return lhs;
}

BigNum& BigNum::operator^=(const BigNum& rhs) noexcept {
  BN_exp(bignum_, bignum_, rhs.bignum_, ctx_);
  return *this;
}

BigNum operator^(BigNum lhs, const BigNum& rhs) noexcept {
  lhs ^= rhs;
  return lhs;
}

bool operator==(const BigNum& lhs, const BigNum& rhs) noexcept {
  return (BN_cmp(lhs.bignum_, rhs.bignum_) == 0);
}

bool operator!=(const BigNum& lhs, const BigNum& rhs) noexcept {
  return not (lhs == rhs);
}

bool operator>(const BigNum& lhs, const BigNum& rhs) noexcept {
  return (BN_cmp(lhs.bignum_, rhs.bignum_) > 0);
}

bool operator>=(const BigNum& lhs, const BigNum& rhs) noexcept {
  return lhs > rhs or lhs == rhs;
}

bool operator<(const BigNum& lhs, const BigNum& rhs) noexcept {
  return (BN_cmp(lhs.bignum_, rhs.bignum_) < 0);
}

bool operator<=(const BigNum& lhs, const BigNum& rhs) noexcept {
  return lhs < rhs or lhs == rhs;
}

std::ostream& operator<<(std::ostream& stream, const BigNum& num) {
  stream << std::string(num);
  return stream;
}

BigNum BigNum::ModAdd(const BigNum& other, const BigNum& mod) noexcept {
  BigNum result;
  BN_mod_add(result.bignum_, bignum_, other.bignum_, mod.bignum_, ctx_);
  return result;
}

BigNum BigNum::ModSub(const BigNum& other, const BigNum& mod) noexcept {
  BigNum result;
  BN_mod_sub(result.bignum_, bignum_, other.bignum_, mod.bignum_, ctx_);
  return result;
}

BigNum BigNum::ModMul(const BigNum& other, const BigNum& mod) noexcept {
  BigNum result;
  BN_mod_mul(result.bignum_, bignum_, other.bignum_, mod.bignum_, ctx_);
  return result;
}

BigNum BigNum::ModSqr(const BigNum& mod) noexcept {
  BigNum result;
  BN_mod_sqr(result.bignum_, bignum_, mod.bignum_, ctx_);
  return result;
}

BigNum BigNum::ModExp(const BigNum& power, const BigNum& mod) {
  if (not BN_is_odd(mod.bignum_)) {
    throw std::runtime_error(
        "In BigNum::ModExp(): mod must be an odd number");
  }
  if (BN_get_flags(bignum_, BN_FLG_CONSTTIME) != 0 or
      BN_get_flags(mod.bignum_, BN_FLG_CONSTTIME) != 0) {
    throw std::runtime_error(
        "In BigNum::ModExp(): none of BigNums should have BN_FLG_CONSTTIME "
        "flag set");
  }
  BigNum result;
  BN_mod_exp(result.bignum_, bignum_, power.bignum_, mod.bignum_, ctx_);
  return result;
}

bool BigNum::IsPrime() const noexcept {
  return BN_is_prime_ex(bignum_, BN_prime_checks, ctx_, nullptr);
}

bool BigNum::IsOdd() const noexcept {
  return BN_is_odd(bignum_);
}

std::string BigNum::to_string() const {
  char* str = BN_bn2dec(bignum_);
  std::string result(str);
  OPENSSL_free(str);

  return result;
}

BigNum::operator std::string() const {
  char* str = BN_bn2dec(bignum_);
  std::string result(str);
  OPENSSL_free(str);

  return result;
}

std::vector<unsigned char> BigNum::to_bytes() const {
  std::vector<unsigned char> result(BN_num_bytes(bignum_));
  BN_bn2bin(bignum_, result.data());

  return result;
}

BigNum PrimeGenerate(int bits, bool safe,
                     const BigNum& add, const BigNum& rem) {
  BIGNUM* prime = BN_new();
  BN_generate_prime_ex(prime, bits, safe, add.get(), rem.get(), nullptr);
  BigNum result(prime);
  BN_free(prime);

  return result;
}

BigNum PrimeGenerate(int bits, bool safe) {
  BIGNUM* prime = BN_new();
  BN_generate_prime_ex(prime, bits, safe, nullptr, nullptr, nullptr);
  BigNum result(prime);
  BN_free(prime);

  return result;
}

BigNum RandomInRange(const BigNum& ex_upper_bound) {
  BIGNUM* num = BN_new();
  BN_priv_rand_range(num, ex_upper_bound.get());
  BigNum result(num);
  BN_free(num);

  return result;
}

BigNum RandomInRange(const BigNum& in_lower_bound,
                     const BigNum& in_upper_bound) {
  return RandomInRange(in_upper_bound - in_lower_bound + BigNum{1})
      + in_lower_bound;
}
}
