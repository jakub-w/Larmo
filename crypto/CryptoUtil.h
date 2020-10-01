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

#include <openssl/sha.h>

namespace lrm::crypto {
using ShaHash = std::array<unsigned char, SHA512_DIGEST_LENGTH>;
static const auto LRM_SESSION_KEY_SIZE = 64;

ShaHash encode_SHA512(std::string_view data);

template<typename Container>
std::string to_hex(Container c) {
  const unsigned char* buf =
      reinterpret_cast<const unsigned char*>(std::data(c));
  const auto size = std::size(c);
  const auto value_size = sizeof(typename decltype(c)::value_type);

  auto output = std::string(size * value_size * 2, ' ');

  for (size_t i = 0; i < size; ++i) {
    std::snprintf(output.data() + (i * value_size * 2), 3, "%02x", buf[i]);
  }

  return output;
}
}

#endif  // LRM_CRYPTOUTIL_H_
