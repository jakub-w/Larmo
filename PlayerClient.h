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

#include "spdlog/spdlog.h"

using namespace grpc;
using namespace asio::ip;

class PlayerClient {
  std::vector<char> read_file(std::string_view filename);
  std::ofstream make_temp_file(std::string_view filename);

  // timeout - in milliseconds
  int wait_for_port(int pipe_fd, unsigned int timeout = 5000);
  int stream_file(const std::string filename, unsigned short port);

  PlayerClient(std::shared_ptr<grpc_impl::Channel> channel);
  unsigned short set_port(unsigned short port);

 public:
  explicit PlayerClient(unsigned short streaming_port,
                        std::shared_ptr<grpc_impl::Channel> channel) noexcept;
  explicit PlayerClient(const std::string& streaming_port,
                        std::shared_ptr<grpc_impl::Channel> channel) noexcept;
  virtual ~PlayerClient() noexcept;

  int Play(std::string_view filename);
  int Stop();
  int TogglePause();
  int Volume(std::string_view volume);
  bool Ping();

 private:
  std::unique_ptr<PlayerService::Stub> stub_;
  StreamingPort port_;
  asio::io_context context_;
  std::vector<char> streaming_file_;
  tcp::acceptor streaming_acceptor_;
  tcp::endpoint streaming_endpoint_;
  tcp::socket streaming_socket_;

  std::shared_ptr<spdlog::logger> log_;
};

#endif // LRM_PLAYERCLIENT_H
