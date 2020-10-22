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

#include "PlaybackState.h"
#include "spdlog/spdlog.h"

namespace lrm {
const std::map<PlaybackState::State, std::string>
PlaybackState::state_names_ = {{UNDEFINED, "UNDEFINED"},
                               {PLAYING, "PLAYING"},
                               {PAUSED, "PAUSED"},
                               {STOPPED, "STOPPED"},
                               {FINISHED, "FINISHED"},
                               {FINISHED_ERROR, "FINISHED_ERROR"}};

PlaybackState::PlaybackState(State state) : state_(state) {}
PlaybackState::PlaybackState() : PlaybackState(UNDEFINED) {}

void PlaybackState::SetState(State new_state) {
  spdlog::debug("Setting playback state to: {}", state_names_.at(new_state));
  if (FINISHED == state_ or FINISHED_ERROR == state_) {
    state_ = STOPPED;
  }
  else {
    state_ = new_state;
  }

  // Ensure that there is no race to state_change_callback_ and that the
  // unknown code in callback won't block forever (so locking only for the
  // copy)
  StateChangeCallback callback;
  {
    std::lock_guard<std::mutex> lck(state_callback_mutex_);
    callback = state_change_callback_;
  }
  if (callback) {
    callback(new_state);
  }
}

void PlaybackState::SetStateChangeCallback(StateChangeCallback&& callback) {
  std::lock_guard<std::mutex> lck(state_callback_mutex_);
  state_change_callback_ = callback;
}
};
