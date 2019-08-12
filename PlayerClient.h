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

#include "grpcpp/channel.h"
#include "player_service.grpc.pb.h"

#include "spdlog/spdlog.h"

#include "PlaybackSynchronizer.h"

using namespace grpc;
using namespace asio::ip;

class PlayerClient {
  std::vector<char> read_file(std::string_view filename);

  /// Start streaming asynchronously.
  /// Return port (it's randomized if port arg is 0)
  int start_streaming(const std::string& filename, unsigned short port);

  /// Start a thread that will continuously update song_info_, taking
  /// information from the remote server.
  void start_updating_info();
  /// Stop the thread started by start_updating_info()
  void stop_updating_info();

  unsigned short set_port(unsigned short port);

  std::string info_get(
      std::string_view token,
      const lrm::PlaybackSynchronizer::PlaybackInfo* playback_info);

  PlayerClient(std::shared_ptr<grpc_impl::Channel> channel);

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
  lrm::PlaybackSynchronizer::PlaybackInfo GetPlaybackInfo();
  bool Ping();
  std::string Info(std::string_view format);

  inline void StreamInfoStart() {
    start_updating_info();
  }

  using SongFinishedCallback = std::function<void(lrm::PlaybackState::State)>;
  void SetSongFinishedCallback(SongFinishedCallback&& callback);

 private:
  std::unique_ptr<PlayerService::Stub> stub_;
  StreamingPort port_;

  asio::io_context context_;
  std::vector<char> streaming_file_;
  tcp::acceptor streaming_acceptor_;
  tcp::endpoint streaming_endpoint_;
  tcp::socket streaming_socket_;

  lrm::PlaybackSynchronizer synchronizer_;

  std::shared_ptr<spdlog::logger> log_;

  SongFinishedCallback song_finished_callback_;
  std::mutex song_finished_mtx_;
};

#endif // LRM_PLAYERCLIENT_H
