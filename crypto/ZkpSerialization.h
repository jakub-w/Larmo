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

#ifndef LRM_ZKPSERIALIZATION_H_
#define LRM_ZKPSERIALIZATION_H_

#include <openssl/bn.h>
#include <openssl/ec.h>

#include "crypto/CryptoUtil.h"
#include "player_service.pb.h"

namespace lrm::crypto {
zkp zkp_deserialize(const ZkpMessage& message);
ZkpMessage zkp_serialize(const zkp& zkp);
}

#endif  // LRM_ZKPSERIALIZATION_H_
