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

#include "crypto/RsaKeyPair.h"
#include "crypto/certs.h"

namespace lrm::crypto::certs {
void RsaKeyPair::Generate() {
  EVP_PKEY* pkey = nullptr;
  EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  if (not pctx) int_error("Failed to create EVP_PKEY_CTX object");

  try {
    if (EVP_PKEY_keygen_init(pctx) <= 0)
      int_error("Error initializing EVP_PKEY keygen");
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(pctx, KEY_BITS) <= 0)
      int_error("Error setting keygen bits for RSA");
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0)
      int_error("Error generating key");
  } catch (...) {
    EVP_PKEY_CTX_free(pctx);
    throw;
  }
  EVP_PKEY_CTX_free(pctx);

  pkey_.reset(pkey);
}
}
