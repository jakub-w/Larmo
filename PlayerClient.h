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

#ifndef INCLUDE_GRPCPLUSPLUS
#include "grpcpp/channel.h"
#else
#include "grpc++/channel.h"
#endif  // INCLUDE_GRPCPLUSPLUS

#include "player_service.grpc.pb.h"

#include "spdlog/spdlog.h"

#include "PlaybackSynchronizer.h"
#include "crypto/CryptoUtil.h"

using namespace grpc;

namespace lrm {
class PlayerClient {
  std::vector<char> read_file(std::string_view filename);

  /// Start a thread that will continuously update song_info_, taking
  /// information from the remote server.
  void start_updating_info();
  /// Stop the thread started by start_updating_info()
  void stop_updating_info();

  std::string info_get(
      std::string_view token,
      const PlaybackSynchronizer::PlaybackInfo* playback_info);

 public:
  explicit PlayerClient(std::shared_ptr<grpc::Channel> channel) noexcept;
  virtual ~PlayerClient();

  bool Authenticate();

  int Play(std::string_view filename);
  int Stop();
  int TogglePause();
  int Volume(std::string_view volume);
  int Seek(std::string_view seconds);
  PlaybackSynchronizer::PlaybackInfo GetPlaybackInfo();
  bool Ping();
  std::string Info(std::string_view format);

  inline void StreamInfoStart() {
    start_updating_info();
  }

  using SongFinishedCallback = std::function<void(PlaybackState::State)>;
  void SetSongFinishedCallback(SongFinishedCallback&& callback);

 private:
  std::unique_ptr<PlayerService::Stub> stub_;

  std::vector<char> streaming_file_;

  PlaybackSynchronizer synchronizer_;

  std::shared_ptr<spdlog::logger> log_;

  SongFinishedCallback song_finished_callback_;
  std::mutex song_finished_mtx_;

  std::string session_key_ = std::string(crypto::LRM_SESSION_KEY_SIZE, ' ');
};
}

#endif // LRM_PLAYERCLIENT_H
