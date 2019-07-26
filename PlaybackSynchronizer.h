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

#ifndef LRM_PLAYBACKSYNCHRONIZER_H_
#define LRM_PLAYBACKSYNCHRONIZER_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "spdlog/spdlog.h"

#include "player_service.grpc.pb.h"

#include "PlaybackState.h"

namespace lrm {
class PlaybackSynchronizer {
 public:
  using StateChangeCallback = PlaybackState::StateChangeCallback;

  struct PlaybackInfo {
    std::string title;
    std::string album;
    std::string artist;

    std::chrono::duration<double> total_time;
    std::chrono::duration<double> elapsed_time;
    std::chrono::duration<double> remaining_time;

    PlaybackState::State playback_state = PlaybackState::UNDEFINED;
  };

  PlaybackSynchronizer(PlayerService::Stub* stub) noexcept : stub_(stub) {}

  virtual ~PlaybackSynchronizer();

  /// If \b update_interval is set to \b std::chrono::duration::max() the
  /// update will occur only on the playback status change.
  void Start(std::chrono::milliseconds update_interval =
             std::chrono::milliseconds(1000));
  void Stop();

  PlaybackInfo GetPlaybackInfo() const;

  inline void SetCallbackOnStatusChange(StateChangeCallback&& callback) {
    playback_state_.SetStateChangeCallback(
        std::forward<StateChangeCallback>(callback));
  }

 private:
  void continuous_update(std::chrono::milliseconds update_interval);

  PlayerService::Stub* stub_;

  std::condition_variable is_updating_cv_;
  std::mutex is_updating_mtx_;
  bool is_updating_ = false;
  std::thread updating_thread_;

  struct BasePlaybackInfo {
    mutable std::mutex mtx;
    PlaybackInfo info;
    std::chrono::time_point<std::chrono::steady_clock> last_update;
  } base_playback_info;

  PlaybackState playback_state_;
};
}

#endif  // LRM_PLAYBACKSYNCHRONIZER_H_
