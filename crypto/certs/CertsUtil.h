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

#include <memory>
#include <string_view>

#include <openssl/rsa.h>
#include <openssl/evp.h>

namespace lrm::crypto::certs {
#define int_error(msg) handle_ssl_error(__FILE__, __LINE__, (msg))

int print_errors_cb(const char* str, size_t len, void* arg);
void handle_ssl_error(std::string_view file, int line, std::string_view msg);
std::basic_string<unsigned char> str_to_uc(std::string_view str);


using bio_ptr = std::unique_ptr<BIO, decltype(&BIO_free_all)>;
bio_ptr make_bio(const BIO_METHOD* type = nullptr);

template <typename Container>
Container bio_to_container(BIO* bio) noexcept {
  Container result;
  result.resize(BIO_pending(bio));
  BIO_read(bio, result.data(), result.size());
  return result;
}

template <typename Container>
bio_ptr container_to_bio(const Container& container) {
  auto bio = make_bio(BIO_s_mem());
  if (not bio) int_error("Failed to create BIO object");

  BIO_write(bio.get(), container.data(), container.size());
  return bio;
}

}
#endif  // LRM_CERTS_H_
