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

#ifndef LRM_SONGINFOUPDATER_H_
#define LRM_SONGINFOUPDATER_H_

#include <condition_variable>
#include <mutex>
#include <thread>

#include "player_service.grpc.pb.h"

#include "PlaybackState.h"
#include "SongInfo.h"

class SongInfoUpdater {
 public:
  using StateChangeCallback = std::function<void(lrm::PlaybackState::State)>;

  SongInfoUpdater(PlayerService::Stub* stub,
                  SongInfo* song_info) noexcept
      : stub_(stub), song_info_(song_info) {}

  virtual ~SongInfoUpdater();

  void Start(std::chrono::milliseconds update_interval =
             std::chrono::milliseconds(1000));
  void Stop();

  /// \exception std::system_error May throw what std::mutex::lock() throws.
  inline void SetCallbackOnStatusChange(StateChangeCallback&& callback) {
    std::lock_guard<std::mutex> lck(state_callback_mtx_);
    state_change_callback_ = callback;
  }

 private:
  void continuous_update(std::chrono::milliseconds update_interval);

  std::thread thread_;

  PlayerService::Stub* stub_;
  SongInfo* song_info_;

  std::mutex state_callback_mtx_;

  StateChangeCallback state_change_callback_;

  std::condition_variable is_updating_cv_;
  std::mutex is_updating_mtx_;
  bool is_updating_ = false;
};

#endif  // LRM_SONGINFOUPDATER_H_
