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

#include <array>
#include <cassert>
#include <memory>
#include <string_view>

#include <openssl/evp.h>
#include <openssl/sha.h>

namespace lrm::crypto {
using ShaHash = std::array<unsigned char, SHA512_DIGEST_LENGTH>;
using SessionKey = std::array<unsigned char, 32>;
/* using SessionKey = std::string; */

ShaHash encode_SHA512(std::string_view data);
}
