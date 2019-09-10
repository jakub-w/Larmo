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

#ifndef LRM_SPEKE_H_
#define LRM_SPEKE_H_

#include <unordered_map>

#include <openssl/evp.h>

#include "BigNum.h"

#define LRM_SPEKE_HASHFUNC EVP_sha3_512()
#define LRM_SPEKE_CIPHER_TYPE EVP_aes_192_gcm()

namespace lrm::crypto {
typedef std::vector<unsigned char> Bytes;

/// \brief Create Simple Password Exponential Key Exchange sessions.
///
/// To create a valid session, construct the SPEKE object with a secret
/// \e password and non-secret \e safe_prime that are shared between both
/// parties (peers).
///
/// The public key, provided by \ref GetPublicKey(), needs to be sent out to
/// the second party along with the \e id obtained by calling \ref GetId().
/// Note that \ref GetId() returns a different id than was given in the
/// constructor. More information is appended to the given id to ensure
/// uniqueness.
///
/// The remote party should send similar \e remote_id and \e remote_pubkey
/// pair, which is intended to be used as arguments for \ref
/// ProvideRemotePublicKeyIdPair().
///
/// After that, the session is valid, although it's wise (but optional) to
/// confirm that both the local session and the remote party's session have
/// the same encryption key. It can be done by calling
/// \ref GetKeyConfirmationData(), sending the result to the remote party
/// and calling \ref ConfirmKey() with the similar data received from the
/// peer.
/// This step is used to confirm that the remote party has the same password,
/// so it acts as an authentication mechanism.
///
/// To combat impersonation attacks a session adds a counter to an id and
/// a remote id provided by the user, so when the session is dropped it can't
/// be restored. The counter is incremented when \ref
/// ProvideRemotePublicKeyIdPair() is called.
class SPEKE {
  static std::unordered_map<std::string, int> id_counts;

 public:
  SPEKE(const SPEKE&) = delete;
  /// \param id Unique identifier.
  /// \param password A password shared with the remote party.
  /// \param safe_prime Big prime number meeting the requirement of
  ///        <tt> p = 2q + 1 </tt> where \c q is also a prime. Shared with the
  ///        remote party.
  SPEKE(std::string_view id, std::string_view password, BigNum safe_prime);
  ~SPEKE();

  Bytes GetPublicKey() const;

  inline const std::string& GetId() const {
    return id_;
  }

  /// Provide the SPEKE session with a public key of the remote party.
  /// \param remote_pubkey Public key of the remote party.
  /// \param remote_id Id of the remote party.
  void ProvideRemotePublicKeyIdPair(const Bytes& remote_pubkey,
                                    const std::string& remote_id);

  /// Return the encryption key created by using HKDF on Diffie-Hellman key
  /// provided by the SPEKE algorithm.
  ///
  /// This is meant to be secret and is the same for the local session and
  /// the remote one.
  ///
  /// The key's length is hardcoded and it's value corresponds to the
  /// key length used with \ref LRM_SPEKE_CIPHER_TYPE.
  const Bytes& GetEncryptionKey();

  /// Return the key confirmation data that can be used by the remote party
  /// to confirm that the encryption keys and ids are the same.
  ///
  /// It's designed to be used as an argument to \ref ConfirmKey() by the
  /// peer.
  ///
  /// Unlike in the default SPEKE standard, the encryption key (created using
  /// HKDF) is used to generate the key confirmation data, not the regular
  /// SPEKE key.
  const Bytes& GetKeyConfirmationData();

  /// Confirm that the remote has the same key.
  /// \param remote_kcd Key confirmation data of the remote party.
  bool ConfirmKey(const Bytes& remote_kcd);

  /// Sign a \e message with HMAC using an encryption key derived from DH
  /// exchange.
  Bytes HmacSign(const Bytes& message);

  /// Confirm a signature created by the remote party with \ref HmacSign()
  /// \return \c true if the signature matches.
  /// \return \c false otherwise.
  bool ConfirmHmacSignature(const Bytes& hmac_signature,
                            const Bytes& message);

 private:
  /// \brief Make an ID out of the public key and the timestamp.
  ///
  /// \param prefix The resulting ID will be prepended with this value.
  ///
  /// \return Newly generated id.
  std::string make_id(const std::string& prefix = "");
  void ensure_keying_material();
  void ensure_encryption_key();
  Bytes gen_kcd(std::string_view first_id, std::string_view second_id,
                const BigNum& first_pubkey, const BigNum& second_pubkey);

  EVP_MD_CTX* mdctx_;

  std::string id_;
  std::string id_numbered_;

  std::string remote_id_numbered_;

  BigNum p_;   // safe prime
  BigNum q_;   // (p_ - 1) / 2
  BigNum gen_; // H(password)^2 mod p_

  // random value in [1; q_ - 1]
  BigNum privkey_;

  // (gen_ ^ privkey_) mod p_
  BigNum pubkey_;

  // public key of the remote party
  BigNum remote_pubkey_;

  // H(min(id_numbered_, remote_id_numbered_),
  //   max(id_numbered_, remote_id_numbered_),
  //   min(pubkey_, remote_pubkey_),
  //   max(pubkey_, remote_pubkey_),
  //   (remote_pubkey_ ^ privkey_) mod p_)
  Bytes keying_material_;

  // a uniform key derived from keying_material_ with HKDF
  Bytes encryption_key_;

  Bytes key_confirmation_data_;
};
}

#endif  // LRM_SPEKE_H_
