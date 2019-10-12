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

#ifndef LRM_CERTS_H_
#define LRM_CERTS_H_

#include <string_view>

#include <openssl/rsa.h>
#include <openssl/evp.h>

namespace lrm::crypto::certs {
#define int_error(msg) handle_ssl_error(__FILE__, __LINE__, (msg))

static constexpr int KEY_BITS = 2048;
static constexpr int KEY_EXPONENT = RSA_F4;
static const EVP_CIPHER* KEY_PEM_CIPHER = EVP_aes_192_gcm();
static const EVP_MD* KEY_SIGN_DIGEST = EVP_sha256();

int print_errors_cb(const char* str, size_t len, void* arg);
void handle_ssl_error(std::string_view file, int line, std::string_view msg);
std::basic_string<unsigned char> str_to_uc(std::string_view str);
std::string bio_to_str(BIO* bio) noexcept;
}
#endif  // LRM_CERTS_H_
