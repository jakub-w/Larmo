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

#ifndef LRM_PLAYERCLIENT_H
#define LRM_PLAYERCLIENT_H

#include <asio.hpp>

#include "player_service.grpc.pb.h"

using namespace grpc;
using namespace asio::ip;

class PlayerClient {
  const std::string STREAMING_PID_FILE = "lrm.pid";
  std::unique_ptr<PlayerService::Stub> stub_;
  StreamingPort port_;

  std::vector<char> read_file(std::string_view filename);
  std::ofstream make_temp_file(std::string_view filename);

  // timeout - in milliseconds
  int wait_for_port(int pipe_fd, unsigned int timeout = 5000);
  int stream_file(const std::string filename, const unsigned short port);

 public:
  PlayerClient(unsigned short streaming_port,
               std::shared_ptr<Channel> channel);
  PlayerClient(const std::string& streaming_port,
               std::shared_ptr<Channel> channel);
  int Play(std::string_view filename);
  int Stop();
  int TogglePause();
};

#endif // LRM_PLAYERCLIENT_H
