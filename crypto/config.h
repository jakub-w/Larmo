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

#ifndef LRM_CRYPTO_CONFIG_H_
#define LRM_CRYPTO_CONFIG_H_

#include <vector>

#include <openssl/evp.h>

namespace lrm::crypto {
static constexpr int LRM_RSA_KEY_BITS = 2048;
static const EVP_CIPHER* LRM_RSA_KEY_PEM_CIPHER = EVP_aes_192_gcm();
static const EVP_MD* LRM_RSA_KEY_SIGN_DIGEST = EVP_sha256();

static const EVP_CIPHER* LRM_SPEKE_CIPHER_TYPE = EVP_aes_192_gcm();
static const EVP_MD* LRM_SPEKE_HASHFUNC = EVP_sha3_512();

using Bytes = std::vector<std::byte>;
}

#endif  // LRM_CRYPTO_CONFIG_H_
