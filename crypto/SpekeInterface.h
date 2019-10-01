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

#ifndef LRM_SPEKEINTERFACE_H_
#define LRM_SPEKEINTERFACE_H_

#include <vector>
#include <string>

namespace lrm::crypto {
typedef std::vector<unsigned char> Bytes;
/// \brief Abstract class for SPEKE implementation.
///
/// More info in \ref SPEKE.
class SpekeInterface {
 public:
  virtual ~SpekeInterface() {};

  virtual Bytes GetPublicKey() const = 0;

  virtual const std::string& GetId() const = 0;

  virtual void ProvideRemotePublicKeyIdPair(
      const Bytes& remote_pubkey,
      const std::string& remote_id) = 0;

  virtual const Bytes& GetEncryptionKey() = 0;

  virtual const Bytes& GetKeyConfirmationData() = 0;

  virtual bool ConfirmKey(const Bytes& remote_kcd) = 0;

  virtual Bytes HmacSign(const Bytes& message) = 0;

  virtual bool ConfirmHmacSignature(
      const Bytes& hmac_signature,
      const Bytes& message) = 0;
};
}

#endif  // LRM_SPEKEINTERFACE_H_
