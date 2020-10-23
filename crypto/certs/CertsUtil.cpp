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

#include "crypto/certs/CertsUtil.h"

#include <cstring>
#include <sstream>

#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "Util.h"

namespace lrm::crypto::certs {
std::basic_string<unsigned char> str_to_uc(std::string_view str) {
  std::basic_string<unsigned char> result(str.size(), ' ');
  Util::safe_memcpy(result.data(), str.data(), str.size());

  return result;
}

Map x509_name_to_map(const X509_NAME* name) {
  Map entries;
  const int num_entries = X509_NAME_entry_count(name);
  entries.reserve(num_entries);
  for (int i = 0; i < num_entries; ++i) {
    const X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, i);
    if (not entry) int_error("Error reading entry from certificate name");

    const ASN1_OBJECT* obj = X509_NAME_ENTRY_get_object(entry);
    if (not obj) int_error("Error reading object from name entry");

    char key[256];
    OBJ_obj2txt(key, 256, obj, 0);
    const ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    if (not data) int_error("Error reading data from name entry");

    entries[key] = reinterpret_cast<const char*>(ASN1_STRING_get0_data(data));
  }
  return entries;
}

Map x509_ext_stack_to_map(const STACK_OF(X509_EXTENSION)* extlist) {
  if (not extlist) return Map{};

  const auto ext_count = X509v3_get_ext_count(extlist);
  Map extensions;
  extensions.reserve(ext_count);

  auto bio = make_bio(BIO_s_mem());
  for (int i = 0; i < ext_count; ++i) {
    auto ext = X509v3_get_ext(extlist, i);
    if (not ext) int_error("Error reading certificate extension");

    const auto obj = X509_EXTENSION_get_object(ext);
    if (not obj) int_error("Error reading object from extension");

    int length = OBJ_obj2txt(nullptr, 0, obj, 0) + 1;
    char key[length];
    OBJ_obj2txt(key, length, obj, 0);

    X509V3_EXT_print(bio.get(), ext, 0, 0);

    extensions[key] = bio_to_container<std::string>(bio.get());
  }

  return extensions;
}

std::unique_ptr<FILE, decltype(&std::fclose)>
open_file(std::string_view filename, std::string_view modes) {
  FILE* fp = nullptr;
  if (not (fp = std::fopen(filename.data(), modes.data()))) {
    std::stringstream ss;
    ss << __PRETTY_FUNCTION__ << "Error opening file: "
       << filename;
    throw std::system_error(errno, std::system_category(), ss.str());
  }

  return std::unique_ptr<FILE, decltype(&std::fclose)>(fp, &std::fclose);
}
}
