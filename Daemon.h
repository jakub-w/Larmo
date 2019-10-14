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

#ifndef LRM_DAEMON_H_
#define LRM_DAEMON_H_

#include <fstream>
#include "filesystem.h"

#include <asio.hpp>

#include "daemon_arguments.pb.h"

#include "spdlog/spdlog.h"

#include "PlayerClient.h"

using namespace asio::local;

namespace lrm {
class Daemon {
 public:
  // TODO: make this struct be the argument for the constructor
  struct daemon_info {
    fs::path config_file;
    fs::path cert_file;
    fs::path log_file;
    std::string grpc_host;
    std::string grpc_port;
    std::string streaming_port;
    std::string cert_port;
    std::string passphrase;
  };

  enum State {
    UNINITIALIZED,
    CONFIG_INITIALIZED,
    GRPC_CLIENT_INITIALIZED
  };

  Daemon() = delete;
  explicit Daemon(std::unique_ptr<daemon_info>&& dinfo);
  virtual ~Daemon();

  void Run() noexcept(false);
  void Initialize();

  static const fs::path socket_path;

 private:
  void initialize_config();
  void initialize_grpc_client();
  void start_accept();
  void connection_handler(std::unique_ptr<stream_protocol::socket>&& socket);

  void trace_grpc_channel_state(std::shared_ptr<grpc::Channel> channel);

  std::unique_ptr<daemon_info> dinfo_;

  State state_ = UNINITIALIZED;

  asio::io_context context_;
  stream_protocol::endpoint endpoint_;
  stream_protocol::acceptor acceptor_;
  std::unique_ptr<stream_protocol::socket> connection_;

  std::unique_ptr<PlayerClient> remote_;

  std::thread grpc_channel_state_thread_;
  std::atomic<bool> grpc_channel_state_run_ = true;

  std::shared_ptr<spdlog::logger> log_;
};
}

#endif  // LRM_DAEMON_H_
