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

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>

#include "crypto/SslUtil.h"

namespace lrm::crypto {
using ShaHash = std::array<unsigned char, SHA512_DIGEST_LENGTH>;
static const auto LRM_SESSION_KEY_SIZE = 64;

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

// TODO: Schnorr NIZK
// TODO: For error handling use handle_ssl_error() and int_error() from
//       CertsUtil.{h,cpp}
using EcPoint = std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)>;
inline static const EC_GROUP* Ed25519() {
  static const std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)> group{
    EC_GROUP_new_by_curve_name(NID_ED25519), &EC_GROUP_free};

  return group.get();
}

inline EcPoint make_point() {
  auto result = EcPoint{EC_POINT_new(Ed25519()), &EC_POINT_free};
  if (nullptr == result.get()) {
    // TODO: Error
  }
  return result;
}

EcPoint make_generator(std::string_view password);
}

#endif  // LRM_CRYPTOUTIL_H_
