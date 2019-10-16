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

#include "crypto/certs.h"

#include <cstring>
#include <sstream>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace lrm::crypto::certs {
int print_errors_cb(const char* str, size_t len, void* arg) {
  std::stringstream* ss = static_cast<std::stringstream*>(arg);
  ss->write(str, len);

  return 0;
}

void handle_ssl_error(std::string_view file, int line, std::string_view msg) {
  std::stringstream ss;
  ss << msg << '\n';

  ERR_print_errors_cb(&print_errors_cb, static_cast<void*>(&ss));

  throw std::runtime_error(ss.str());
}

std::basic_string<unsigned char> str_to_uc(std::string_view str) {
  std::basic_string<unsigned char> result(str.size(), ' ');
  std::memcpy(result.data(), str.data(), str.size());

  return result;
}

bio_ptr make_bio(const BIO_METHOD* type) {
  if (not type) {
    return std::unique_ptr<BIO, decltype(&BIO_free_all)>(
        nullptr, &BIO_free_all);
  } else {
    return std::unique_ptr<BIO, decltype(&BIO_free_all)>(
        BIO_new(type), &BIO_free_all);
  }
}
}
