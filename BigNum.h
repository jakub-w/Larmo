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

#ifndef LRM_BIGNUM_H_
#define LRM_BIGNUM_H_

#include <string_view>
#include <vector>

#include <openssl/bn.h>

namespace lrm::crypto {
class BigNum {
 public:
  BigNum() noexcept;
  explicit BigNum(const BIGNUM* bignum) noexcept;
  explicit BigNum(std::string_view dec_num_str) noexcept;
  BigNum(BN_ULONG num) noexcept;
  explicit BigNum(const std::vector<unsigned char>& bytes) noexcept;
  explicit BigNum(const unsigned char* bytes, size_t size) noexcept;
  BigNum(const BigNum& other) noexcept;
  BigNum(BigNum&& other) noexcept;
  ~BigNum() noexcept;

  BigNum& operator=(const BigNum& other) noexcept;
  BigNum& operator+=(const BigNum& rhs) noexcept;
  friend BigNum operator+(BigNum lhs, const BigNum& rhs) noexcept;
  BigNum& operator-=(const BigNum& rhs) noexcept;
  friend BigNum operator-(BigNum lhs, const BigNum& rhs) noexcept;
  BigNum& operator*=(const BigNum& rhs) noexcept;
  friend BigNum operator*(BigNum lhs, const BigNum& rhs) noexcept;
  BigNum& operator/=(const BigNum& rhs) noexcept;
  friend BigNum operator/(BigNum lhs, const BigNum& rhs) noexcept;
  BigNum& operator%=(const BigNum& rhs) noexcept;
  friend BigNum operator%(BigNum lhs, const BigNum& rhs) noexcept;
  BigNum& operator^=(const BigNum& rhs) noexcept;
  friend BigNum operator^(BigNum lhs, const BigNum& rhs) noexcept;

  friend bool operator==(const BigNum& lhs, const BigNum& rhs) noexcept;
  friend bool operator!=(const BigNum& lhs, const BigNum& rhs) noexcept;
  friend bool operator>(const BigNum& lhs, const BigNum& rhs) noexcept;
  friend bool operator>=(const BigNum& lhs, const BigNum& rhs) noexcept;
  friend bool operator<(const BigNum& lhs, const BigNum& rhs) noexcept;
  friend bool operator<=(const BigNum& lhs, const BigNum& rhs) noexcept;

  BigNum ModAdd(const BigNum& other, const BigNum& mod) noexcept;
  BigNum ModSub(const BigNum& other, const BigNum& mod) noexcept;
  BigNum ModMul(const BigNum& other, const BigNum& mod) noexcept;
  BigNum ModSqr(const BigNum& mod) noexcept;
  BigNum ModExp(const BigNum& power, const BigNum& mod);

  inline const BIGNUM* get() const noexcept {
    return bignum_;
  }

  bool IsPrime() const noexcept;
  bool IsOdd() const noexcept;

  friend std::ostream& operator<<(std::ostream& stream, const BigNum& num);

  std::string to_string() const;
  operator std::string() const;
  std::vector<unsigned char> to_bytes() const;

 private:
  BIGNUM* bignum_;
  BN_CTX* ctx_;
};

BigNum PrimeGenerate(int bits, bool safe,
                     const BigNum& add, const BigNum& rem);
BigNum PrimeGenerate(int bits, bool safe = false);

/// Generate a random number from range [0; \e ex_upper_bound)
/// \param ex_upper_bound Upper bound, excluded from the set.
BigNum RandomInRange(const BigNum& ex_upper_bound);
/// Generate a random number from range [\e in_lower_bound; \e in_upper_bound]
/// \param in_lower_bound Lower bound, included in the set.
/// \param in_upper_bound Upper bound, included in the set.
BigNum RandomInRange(const BigNum& in_lower_bound,
                     const BigNum& in_upper_bound);
}

#endif  // LRM_BIGNUM_H_
