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

#include "crypto/SslUtil.h"

#include <exception>
#include <sstream>

#include <openssl/err.h>

namespace lrm::crypto {
int print_errors_cb(const char* str, size_t len, void* arg) {
  std::stringstream* ss = static_cast<std::stringstream*>(arg);
  ss->write(str, len);

  return 0;
}

void handle_ssl_error(std::string_view file, int line, std::string_view msg) {
  std::stringstream ss;
  ss << file << ':' << line << ' ' << msg << '\n';

  ERR_print_errors_cb(&print_errors_cb, static_cast<void*>(&ss));

  throw std::runtime_error(ss.str());
}

bio_ptr make_bio(const BIO_METHOD* type) {
  if (not type) {
    return std::unique_ptr<BIO, decltype(&BIO_free_all)>(
        nullptr, &BIO_free_all);
  } else {
    auto bio = std::unique_ptr<BIO, decltype(&BIO_free_all)>(
        BIO_new(type), &BIO_free_all);
    if (not bio) int_error("Failed to create BIO object");
    return bio;
  }
}
}
