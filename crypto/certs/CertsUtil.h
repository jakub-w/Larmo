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
#include <openssl/x509.h>
#include <openssl/x509v3.h>

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

template <typename Map>
auto map_to_x509_name(const Map& map) {
  std::unique_ptr<X509_NAME, decltype(&X509_NAME_free)>
      name{X509_NAME_new(), &X509_NAME_free};
  if (not name) int_error("Failed to create X509_NAME object");

  for(const auto& [key, value] : map) {
    const auto value_uc = str_to_uc(value);
    if (not X509_NAME_add_entry_by_txt(
            name.get(), key.c_str(), MBSTRING_ASC,
            value_uc.c_str(), value_uc.size(), -1, 0)) {
      int_error("Error adding entry '" + key + "' to X509_NAME object");
    }
  }

  return name;
}

template <typename Map>
auto map_to_x509_extension_stack(const Map& map) {
  const auto deleter =
      [](STACK_OF(X509_EXTENSION)* stack) {
        sk_X509_EXTENSION_pop_free(stack, X509_EXTENSION_free);
      };
  std::unique_ptr<STACK_OF(X509_EXTENSION), decltype(deleter)>
      extlist(sk_X509_EXTENSION_new_null(), deleter);

  for(const auto& [key, value] : map) {
    X509_EXTENSION* ext = X509V3_EXT_conf(nullptr, nullptr,
                                          key.c_str(), value.c_str());
    if (not ext) int_error("Error creating extension: " + key);
    sk_X509_EXTENSION_push(extlist.get(), ext);
  }

  return extlist;
}
}
#endif  // LRM_CERTS_H_
