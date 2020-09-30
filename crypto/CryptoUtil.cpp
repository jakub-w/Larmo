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

#include "CryptoUtil.h"

namespace lrm::crypto {
ShaHash encode_SHA512(std::string_view data) {
  static std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx{
    EVP_MD_CTX_new(), &EVP_MD_CTX_free};

  EVP_MD_CTX_reset(ctx.get());


  EVP_DigestInit_ex(ctx.get(), EVP_sha3_512(), nullptr);
  EVP_DigestUpdate(ctx.get(), data.data(), data.length());

  ShaHash hash;

#ifndef NDEBUG
  unsigned int hash_len;
  EVP_DigestFinal_ex(ctx.get(), hash.data(), &hash_len);
  assert(hash_len == hash.size());
#else
  EVP_DigestFinal_ex(ctx.get(), hash.data(), nullptr);
#endif

  return hash;
}
}
