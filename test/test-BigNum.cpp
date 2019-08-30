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

#include <algorithm>

#include <gtest/gtest.h>

#include "SPEKE.h"
#include "BigNum.h"

using namespace lrm::crypto;

TEST(BigNumTest, ConstructFromBIGNUM) {
  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  auto number = BigNum(bn);

  EXPECT_NE(bn, number.get()) <<
      "BigNum points to the same address as the BIGNUM it was constructed "
      "from";
  EXPECT_NE(nullptr, number.get());

  EXPECT_EQ(0, BN_cmp(number.get(), bn)) <<
      "BigNum stores a different value than the BIGNUM it was constructed "
      "from";

  BN_free(bn);
}

TEST(BigNumTest, ConstructFromDecimalRepString) {
  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  auto number = BigNum("1234");
  EXPECT_NE(nullptr, number.get());
  EXPECT_EQ(0, BN_cmp(number.get(), bn)) <<
      "BigNum doesn't store the value representing '1234' decimal number";

  BN_free(bn);
}

TEST(BigNumTest, ConstructFromUnsignedLong) {
  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  auto number = BigNum{1234};
  EXPECT_NE(nullptr, number.get());
  EXPECT_EQ(0, BN_cmp(number.get(), bn)) <<
      "BigNum doesn't store the value representing '1234' decimal number";

  BN_free(bn);
}

TEST(BigNumTest, ConstructFromUCharVector) {
  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  int length = BN_num_bytes(bn);
  unsigned char chars[length];
  BN_bn2bin(bn, chars);

  std::vector<unsigned char> vect(chars, chars + length);

  auto number = BigNum{vect};
  EXPECT_NE(nullptr, number.get());
  EXPECT_EQ(0, BN_cmp(number.get(), bn)) <<
      "BigNum doesn't store the value representing '1234' decimal number";

  BN_free(bn);
}

TEST(BigNumTest, ConstructFromUChars) {
  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  int length = BN_num_bytes(bn);
  unsigned char chars[length];
  BN_bn2bin(bn, chars);

  auto number = BigNum{chars, (size_t)length};
  EXPECT_NE(nullptr, number.get());
  EXPECT_EQ(0, BN_cmp(number.get(), bn)) <<
      "BigNum doesn't store the value representing '1234' decimal number";

  BN_free(bn);
}

TEST(BigNumTest, CopyConstruct) {
  auto num1 = BigNum(1234);
  auto num2 = BigNum(num1);

  EXPECT_NE(nullptr, num2.get());
  EXPECT_NE(num1.get(), num2.get()) <<
      "Copying BigNum makes a shallow copy instead of a deep one";
  EXPECT_EQ(0, BN_cmp(num1.get(), num2.get())) <<
      "Copied number is different than the original one";
}

TEST(BigNumTest, MoveConstruct) {
  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  auto num1 = BigNum(bn);
  const void* num1_p = num1.get();
  auto num2 = BigNum(std::move(num1));

  EXPECT_NE(nullptr, num2.get());
  EXPECT_EQ(nullptr, num1.get()) <<
      "After a move operation the BigNum still points to the actual address";
  EXPECT_NE(num1.get(), num2.get()) <<
      "After a move operation the BigNum still points to the same address";
  EXPECT_EQ(0, BN_cmp(bn, num2.get())) <<
      "After a move operation the number stored is different than the "
      "original";
  EXPECT_EQ(num1_p, num2.get()) <<
      "After a move operation the internal pointer points to the different "
      "address than before";

  BN_free(bn);
}

TEST(BigNumTest, OperatorEquals) {
  auto num = BigNum(1234);
  auto numEqual = BigNum(1234);
  auto numLower = BigNum(1233);
  auto numGreater = BigNum(1235);

  EXPECT_TRUE(num == numEqual);
  EXPECT_FALSE(num == numLower);
  EXPECT_FALSE(num == numGreater);
}

TEST(BigNumTest, OperatorNotEquals) {
  auto num = BigNum(1234);
  auto numEqual = BigNum(1234);
  auto numLower = BigNum(1233);
  auto numGreater = BigNum(1235);

  EXPECT_FALSE(num != numEqual);
  EXPECT_TRUE(num != numLower);
  EXPECT_TRUE(num != numGreater);
}

TEST(BigNumTest, OperatorGeater) {
  auto num = BigNum(1234);
  auto numEqual = BigNum(1234);
  auto numLower = BigNum(1233);
  auto numGreater = BigNum(1235);

  EXPECT_FALSE(num > numEqual);
  EXPECT_TRUE(num > numLower);
  EXPECT_FALSE(num > numGreater);
}

TEST(BigNumTest, OperatorGeaterEqual) {
  auto num = BigNum(1234);
  auto numEqual = BigNum(1234);
  auto numLower = BigNum(1233);
  auto numGreater = BigNum(1235);

  EXPECT_TRUE(num >= numEqual);
  EXPECT_TRUE(num >= numLower);
  EXPECT_FALSE(num >= numGreater);
}

TEST(BigNumTest, OperatorLower) {
  auto num = BigNum(1234);
  auto numEqual = BigNum(1234);
  auto numLower = BigNum(1233);
  auto numGreater = BigNum(1235);

  EXPECT_FALSE(num < numEqual);
  EXPECT_FALSE(num < numLower);
  EXPECT_TRUE(num < numGreater);
}

TEST(BigNumTest, OperatorLowerEqual) {
  auto num = BigNum(1234);
  auto numEqual = BigNum(1234);
  auto numLower = BigNum(1233);
  auto numGreater = BigNum(1235);

  EXPECT_TRUE(num >= numEqual);
  EXPECT_TRUE(num >= numLower);
  EXPECT_FALSE(num >= numGreater);
}

TEST(BigNumTest, OperatorAssign) {
  auto num1 = BigNum(1234);
  BigNum num2 = num1;

  EXPECT_TRUE(num1 == num2);
  EXPECT_NE(num1.get(), num2.get());
}

TEST(BigNumTest, OperatorPlus) {
  auto number = BigNum(1234);

  EXPECT_TRUE(BigNum(1240) == number + BigNum(6));
}

TEST(BigNumTest, OperatorPlusAssign) {
  auto number = BigNum(1234);
  number += BigNum(6);

  EXPECT_TRUE(BigNum(1240) == number);
}

TEST(BigNumTest, OperatorMinus) {
  auto number = BigNum(1234);

  EXPECT_TRUE(BigNum(1230) == number - BigNum(4));
}

TEST(BigNumTest, OperatorMinusAssign) {
  auto number = BigNum(1234);
  number -= BigNum(4);

  EXPECT_TRUE(BigNum(1230) == number);
}

TEST(BigNumTest, OperatorMultiply) {
  auto number = BigNum(1234);

  EXPECT_TRUE(BigNum(2468) == number * BigNum(2));
}

TEST(BigNumTest, OperatorMultiplyAssign) {
  auto number = BigNum(1234);
  number *= BigNum(2);

  EXPECT_TRUE(BigNum(2468) == number);
}

TEST(BigNumTest, OperatorDivide) {
  EXPECT_TRUE(BigNum(617) == BigNum(1234) / BigNum(2));
  EXPECT_TRUE(BigNum(308) == BigNum(617) / BigNum(2));
}

TEST(BigNumTest, OperatorDivideAssign) {
  auto number = BigNum(1234);
  number /= BigNum(2);

  EXPECT_TRUE(BigNum(617) == number);
}

TEST(BigNumTest, OperatorModulo) {
  EXPECT_EQ(BigNum(4), BigNum(1234) % BigNum(10));
}

TEST(BigNumTest, OperatorModuloAssign) {
  auto number = BigNum(1234);
  number %= BigNum(10);

  EXPECT_EQ(BigNum(4), number);
}

TEST(BigNumTest, OperatorExponentiation) {
  EXPECT_EQ(BigNum(1522756), BigNum(1234) ^ BigNum(2));
}

TEST(BigNumTest, OperatorExponentiationAssign) {
  auto number = BigNum(1234);
  number ^= BigNum(2);

  EXPECT_EQ(BigNum(1522756), number);
}

TEST(BigNumTest, ModularAddition) {
  auto number = BigNum(10);

  EXPECT_EQ(BigNum(6), number.ModAdd(BigNum(5), BigNum(9)));
}

TEST(BigNumTest, ModularSubstraction) {
  auto number = BigNum(10);

  EXPECT_EQ(BigNum((unsigned long)0), number.ModSub(BigNum(1), BigNum(9)));
}

TEST(BigNumTest, ModularMultiplication) {
  auto number = BigNum(10);

  EXPECT_EQ(BigNum(5), number.ModMul(BigNum(5), BigNum(9)));
}

TEST(BigNumTest, ModularSquare) {
  auto number = BigNum(10);

  EXPECT_EQ(BigNum(1), number.ModSqr(BigNum(9)));
}

TEST(BigNumTest, ModularExponentiation) {
  auto number = BigNum(10);

  EXPECT_EQ(BigNum(1), number.ModExp(BigNum(3), BigNum(9)));

  EXPECT_THROW(number.ModExp(BigNum(3), BigNum(8)), std::runtime_error) <<
      "Modular exponentiation isn't designed for even moduli but it doesn't "
      "throw";
}

TEST(BigNumTest, IsPrime) {
  EXPECT_TRUE(BigNum(11).IsPrime());
  EXPECT_FALSE(BigNum(21).IsPrime());

  BigNum prime(
    "278718366047762056480812033078788736797271546707818954946093113154729289"
    "302927639454685517685322534387999931631023284419975952744520278746058579"
    "490952509308007698496572377391031655659623611725625811328290785236313847"
    "852866778866556089335185928593711814424294422098927785875857660733304530"
    "245616955902099831681506333395671004864385216013882795502805476926572611"
    "129875972685685499239652450043007129462522525657896303813251004122336428"
    "215423586299334065077029417930733797410166196486859337602267490201314093"
    "337915591074018203327127313751112674378016066828237858656290265502302950"
    "66560001987548566431890030284030054139119");

  EXPECT_TRUE(prime.IsPrime());
}

TEST(BigNumTest, IsOdd) {
  EXPECT_TRUE(BigNum(11).IsOdd());
  EXPECT_FALSE(BigNum(6).IsOdd());
}

TEST(BigNumTest, to_string) {
  EXPECT_EQ("1234", BigNum(1234).to_string());
}

TEST(BigNumTest, OperatorStdString) {
  EXPECT_EQ("1234", (std::string)BigNum(1234));
}

TEST(BigNumTest, to_bytes) {
  std::vector<unsigned char> bytes = BigNum(1234).to_bytes();

  BIGNUM* bn = BN_new();
  BN_dec2bn(&bn, "1234");

  int length = BN_num_bytes(bn);
  unsigned char chars[length];
  BN_bn2bin(bn, chars);

  BN_free(bn);

  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(),
                         chars, chars + length));
}

TEST(BigNumTest, OperatorToStream) {
  std::stringstream ss;
  ss << BigNum(1234);

  EXPECT_EQ("1234", ss.str());
}

TEST(BigNumTest, PrimeGenerate) {
  BigNum prime = PrimeGenerate(8);

  EXPECT_TRUE(prime.IsPrime());
}

TEST(BigNumTest, SafePrimeGenerate) {
  BigNum prime = PrimeGenerate(8, true);

  EXPECT_TRUE(prime.IsPrime());
  EXPECT_TRUE(((prime - BigNum(1)) / BigNum(2)).IsPrime()) <<
      "Generated prime is not a safe prime";
}

TEST(BigNumTest, RandomInRange) {
  BigNum bound(10);
  BigNum zero((unsigned long) 0);
  for (auto i = 0; i < 100; ++i) {
    auto number = RandomInRange(bound);
    ASSERT_GE(number, zero);
    ASSERT_LT(number, bound);
  }

  BigNum lowbound(10);
  BigNum upbound(19);
  for (auto i = 0; i < 100; ++i) {
    auto number = RandomInRange(lowbound, upbound);
    ASSERT_GE(number, lowbound);
    ASSERT_LE(number, upbound);
  }
}
