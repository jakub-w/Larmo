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

#ifndef LRM_PLAYERSERVICEIMPL_H
#define LRM_PLAYERSERVICEIMPL_H

#include "player_service.grpc.pb.h"

#include "Player.h"

using namespace grpc;

class PlayerServiceImpl : public PlayerService::Service {
  lrm::Player player;

  bool check_passphrase_(const ServerContext* context) const;

 public:
  Status PlayFrom(ServerContext* context,
                  const StreamingPort* port,
                  MpvResponse* response);

  Status Stop(ServerContext* context,
              const Empty*,
              MpvResponse* response);

  Status TogglePause(ServerContext* context,
                     const Empty*,
                     MpvResponse* response);

  Status Volume(ServerContext* context,
                const VolumeMessage* volume,
                MpvResponse* response);
};

#endif // LRM_PLAYERSERVICEIMPL_H
