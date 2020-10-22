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

#ifndef LRM_CLIENTCONTEXTS_H_
#define LRM_CLIENTCONTEXTS_H_

#include <string>

#include "player_service.grpc.pb.h"

namespace lrm {
using UnauthenticatedContext = grpc::ClientContext;

/// ClientContext with \e x-session-id metadata.
class AuthenticatedContext : public grpc::ClientContext {
 public:
  inline explicit AuthenticatedContext(const std::string& session_key) {
    AddMetadata("x-session-key", session_key);
  }
};
}

#endif  // LRM_CLIENTCONTEXTS_H_
