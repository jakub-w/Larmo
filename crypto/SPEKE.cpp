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

#include "SPEKE.h"

#include <stdexcept>

#include <openssl/hmac.h>
#include <openssl/kdf.h>

#include "crypto/SPEKE.pb.h"

namespace lrm::crypto {
std::unordered_map<std::string, int> SPEKE::id_counts;

SPEKE::SPEKE(std::string_view id,
             std::string_view password,
             BigNum safe_prime)
    : mdctx_{EVP_MD_CTX_new()},
      id_{id},
      p_{safe_prime},
      q_{(p_ - 1) / 2} {
  if (not safe_prime.IsOdd()) {
    throw std::runtime_error(
        "In SPEKE::SPEKE(): safe_prime is not an odd number");
  }

  // Create a hash of the password: H(s)
  EVP_DigestInit_ex(mdctx_, LRM_SPEKE_HASHFUNC, nullptr);
  EVP_DigestUpdate(mdctx_, password.data(), password.length());

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;
  EVP_DigestFinal_ex(mdctx_, md_value, &md_len);

  EVP_MD_CTX_reset(mdctx_);

  // g = H(s)^2 mod p
  gen_ = BigNum(md_value, md_len);
  gen_ = gen_.ModExp(2, p_);

  // x
  privkey_ = RandomInRange(1, q_);
  // g^x mod p
  pubkey_ = gen_.ModExp(privkey_, p_);

}

SPEKE::~SPEKE() {
  EVP_MD_CTX_free(mdctx_);
}

Bytes SPEKE::GetPublicKey() const {
  if(0 == pubkey_) {
    throw std::logic_error(
          "SPEKE uninitialized: Can't get the public key");
  }

  return pubkey_.to_bytes();
}

// TODO: This should return an error code in a second argument
void SPEKE::ProvideRemotePublicKeyIdPair(const Bytes& remote_pubkey,
                                         const std::string& remote_id) {
  if (not (0 == remote_pubkey_ and remote_id_numbered_.empty())) {
    throw std::logic_error(
        "SPEKE: The remote's information already provided");
  }
  if (remote_id == id_) {
    throw std::logic_error(
        "SPEKE: The remote's identifier is the same as the local identifier");
  }

  BigNum temp = BigNum(remote_pubkey);
  if (temp > p_ - 2 or temp < 2) {
    throw std::logic_error("SPEKE: The remote's public key is invalid");
  }
  remote_pubkey_ = std::move(temp);

  std::string id_num = std::to_string(++id_counts[remote_id]);
  id_numbered_ = id_ + "-" + id_num;
  remote_id_numbered_ = remote_id + "-" + id_num;
}

const Bytes& SPEKE::GetEncryptionKey() {
  if (encryption_key_.empty()) {
    ensure_encryption_key();
  }

  return encryption_key_;
}

const Bytes& SPEKE::GetKeyConfirmationData() {
  if (key_confirmation_data_.empty()) {
    key_confirmation_data_ =
        gen_kcd(id_numbered_, remote_id_numbered_,
                pubkey_, remote_pubkey_);
  }

  return key_confirmation_data_;
}

bool SPEKE::ConfirmKey(const Bytes& remote_kcd) {
  return remote_kcd == gen_kcd(remote_id_numbered_, id_numbered_,
                               remote_pubkey_, pubkey_);
}

Bytes SPEKE::HmacSign(const Bytes& message) {
  ensure_encryption_key();

  HMAC_CTX* ctx = HMAC_CTX_new();

  HMAC_Init_ex(ctx, encryption_key_.data(), encryption_key_.size(),
               LRM_SPEKE_HASHFUNC, nullptr);
  HMAC_Update(ctx, message.data(), message.size());

  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int len = 0;
  HMAC_Final(ctx, md, &len);

  HMAC_CTX_free(ctx);

  Bytes result;
  result.reserve(len);
  std::copy_n(md, len, std::back_inserter(result));

  return result;
}

bool SPEKE::ConfirmHmacSignature(const Bytes& hmac_signature,
                                 const Bytes& message) {
  return hmac_signature == HmacSign(message);
}

void SPEKE::ensure_keying_material() {
  if (0 == privkey_) {
    throw std::logic_error("SPEKE uninitialized: Can't get the private key");
  }
  if (0 == remote_pubkey_) {
    throw std::logic_error("SPEKE: Remote public key is not set");
  }
  if (not keying_material_.empty()) {
    return;
  }

  keying_material_ = remote_pubkey_.ModExp(privkey_, p_).to_bytes();

  EVP_DigestInit_ex(mdctx_, LRM_SPEKE_HASHFUNC, nullptr);

  const std::string& first_id =
      std::min(id_numbered_, remote_id_numbered_);
  const std::string& second_id =
      std::max(id_numbered_, remote_id_numbered_);
  EVP_DigestUpdate(mdctx_, first_id.c_str(), first_id.length());
  EVP_DigestUpdate(mdctx_, second_id.c_str(), second_id.length());

  Bytes first_pubkey = std::min(pubkey_, remote_pubkey_).to_bytes();
  Bytes second_pubkey = std::max(pubkey_, remote_pubkey_).to_bytes();
  EVP_DigestUpdate(mdctx_, first_pubkey.data(), first_pubkey.size());
  EVP_DigestUpdate(mdctx_, second_pubkey.data(), second_pubkey.size());

  EVP_DigestUpdate(mdctx_, keying_material_.data(), keying_material_.size());

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len;
  EVP_DigestFinal_ex(mdctx_, md_value, &md_len);

  EVP_MD_CTX_reset(mdctx_);

  keying_material_.clear();
  keying_material_.reserve(md_len);

  std::copy_n(md_value, md_len, std::back_inserter(keying_material_));
}

void SPEKE::ensure_encryption_key() {
  if (not encryption_key_.empty()) {
    return;
  }

  ensure_keying_material();

  EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);

  EVP_PKEY_derive_init(pctx);
  EVP_PKEY_CTX_set_hkdf_md(pctx, LRM_SPEKE_HASHFUNC);

  // generate salt from public keys
  auto keys = std::minmax(pubkey_, remote_pubkey_);
  Bytes key = keys.first.to_bytes();
  Bytes salt;
  salt.insert(salt.end(), key.begin(), key.end());
  key = keys.second.to_bytes();
  salt.insert(salt.end(), key.begin(), key.end());

  EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), salt.size());

  EVP_PKEY_CTX_set1_hkdf_key(pctx,
                             keying_material_.data(),
                             keying_material_.size());

  const char info[] = "Larmo_SPEKE_HKDF";
  // 'sizeof - 1' to drop the last null byte
  EVP_PKEY_CTX_add1_hkdf_info(pctx, info, sizeof(info) - 1);

  size_t key_len = EVP_CIPHER_key_length(LRM_SPEKE_CIPHER_TYPE);
  unsigned char out[key_len];
  EVP_PKEY_derive(pctx, out, &key_len);

  EVP_PKEY_CTX_free(pctx);

  encryption_key_.reserve(key_len);
  std::copy_n(out, key_len, std::back_inserter(encryption_key_));
}

Bytes SPEKE::gen_kcd(std::string_view first_id,
                     std::string_view second_id,
                     const BigNum& first_pubkey,
                     const BigNum& second_pubkey) {
  ensure_encryption_key();

  HMAC_CTX* hmac_ctx = HMAC_CTX_new();

  // HMAC(K, "KC_1_U"|A|B|M|N) -- M and N are pubkeys for A and B respectively
  HMAC_Init_ex(hmac_ctx, encryption_key_.data(), encryption_key_.size(),
               LRM_SPEKE_HASHFUNC, nullptr);

  const unsigned char method[] = "KC_1_U";
  // 'sizeof - 1' to drop the last null byte
  HMAC_Update(hmac_ctx, method, sizeof(method) - 1);

  HMAC_Update(hmac_ctx, (unsigned char*)first_id.data(), first_id.size());
  HMAC_Update(hmac_ctx, (unsigned char*)second_id.data(), second_id.size());

  Bytes pk_bytes = first_pubkey.to_bytes();
  HMAC_Update(hmac_ctx, pk_bytes.data(), pk_bytes.size());
  Bytes rpk_bytes = second_pubkey.to_bytes();
  HMAC_Update(hmac_ctx, rpk_bytes.data(), rpk_bytes.size());

  unsigned char md_value[EVP_MAX_MD_SIZE];
  unsigned int md_len = 0;
  HMAC_Final(hmac_ctx, md_value, &md_len);

  HMAC_CTX_free(hmac_ctx);

  Bytes result;
  result.reserve(md_len);
  std::copy_n(md_value, md_len, std::back_inserter(result));

  return result;
}
}
