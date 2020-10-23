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

#include <cstdio>
#include <memory>
#include <string_view>
#include <unordered_map>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "crypto/SslUtil.h"

namespace lrm::crypto::certs {
std::basic_string<unsigned char> str_to_uc(std::string_view str);

using Map = std::unordered_map<std::string, std::string>;

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

template <typename MapT>
auto map_to_x509_extension_stack(const MapT& map) {
  constexpr auto deleter =
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

Map x509_name_to_map(const X509_NAME* name);
Map x509_ext_stack_to_map(const STACK_OF(X509_EXTENSION)* extlist);

std::unique_ptr<FILE, decltype(&std::fclose)>
open_file(std::string_view filename, std::string_view modes);
}
#endif  // LRM_CERTS_H_
