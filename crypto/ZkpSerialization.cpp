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

#include "ZkpSerialization.h"

namespace lrm::crypto {
zkp zkp_deserialize(const ZkpMessage& message) {

// check if reinterpret_cast is safe
  static_assert(
      sizeof(std::remove_reference_t<decltype(message.v())>::value_type)
      == sizeof(unsigned char));
  static_assert(
      alignof(std::remove_reference_t<decltype(message.v())>::value_type)
      == alignof(unsigned char));
  static_assert(
      sizeof(std::remove_reference_t<decltype(message.r())>::value_type)
      == sizeof(unsigned char));
  static_assert(
      alignof(std::remove_reference_t<decltype(message.r())>::value_type)


      == alignof(unsigned char));  return{
    std::string{message.user_id()},
    BytesToEcPoint(
        reinterpret_cast<const unsigned char*>(message.v().data()),
        message.v().size()),
    BytesToEcScalar(
        reinterpret_cast<const unsigned char*>(message.r().data()),
        message.r().size())
  };
}

ZkpMessage zkp_serialize(const zkp& zkp) {
  assert(zkp.V.get() != nullptr);
  assert(zkp.r.get() != nullptr);

  ZkpMessage result;

  result.set_user_id(zkp.user_id);

  std::string* V = result.mutable_v();
  V->resize(EC_POINT_point2oct(CurveGroup(), zkp.V.get(),
                               POINT_CONVERSION_COMPRESSED,
                               nullptr, 0, get_bnctx()));
  if (0 == V->size()) {
    int_error("Failed to determine the size of octet string for converting "
              "EC_POINT");
  }

  if (not EC_POINT_point2oct(CurveGroup(), zkp.V.get(),
                             POINT_CONVERSION_COMPRESSED,
                             reinterpret_cast<unsigned char*>(V->data()),
                             V->size(), get_bnctx())) {
    int_error("Failed to convert EC_POINT to an octet string");
  }

  std::string* r = result.mutable_r();
  r->resize(BN_num_bytes(zkp.r.get()));

  if (0 == r->size()) {
    int_error("Failed to determine the size of BIGNUM for conversion");
  }
  if (not BN_bn2bin(zkp.r.get(),
                    reinterpret_cast<unsigned char*>(r->data()))) {
    int_error("Failed to convert BIGNUM to binary form");
  }

  return result;
}
}
