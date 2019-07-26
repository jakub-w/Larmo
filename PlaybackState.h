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

#ifndef LRM_PLAYBACKSTATE_H_
#define LRM_PLAYBACKSTATE_H_

#include <atomic>
#include <functional>
#include <map>
#include <mutex>

namespace lrm {
class PlaybackState {
 public:
  /// FINISHED and FINISHED_ERROR are equivalent to STOPPED and are used only
  /// in the \e callback function set by \refitem SetStateChangeCallback().
  enum State {
    UNDEFINED,
    PLAYING,
    PAUSED,
    STOPPED,
    /// playback was stopped because the song has ended
    FINISHED,
    /// playback was stopped because of an error
    FINISHED_ERROR
  };
  typedef std::function<void(const State&)> StateChangeCallback;

  PlaybackState(State state);
  PlaybackState();

  /// \return Value returned can be only PLAYING, PAUSED, STOPPED or
  /// UNDEFINED. More information in \refitem PlaybackState::State and
  /// \refitem SetState().
  inline State GetState() const {
    return state_;
  }

  /// If \e new_state is FINISHED or FINISHED_ERROR, the state changes to
  /// STOPPED instead.
  /// The \e callback set with \refitem SetStateChangeCallback() will run with
  /// \e new_state as an argument, the change to STOPPED will be ignored.
  void SetState(State new_state);

  /// Set the callback to be called when the state changes.
  /// \param callback Callback function with the following specification:
  /// \code void(const PlaybackState::State&) \endcode
  void SetStateChangeCallback(StateChangeCallback&& callback);

  /// \return String representation of \e state.
  /// \exception std::out_of_range Invalid \e state was given.
  static inline const std::string& StateName(State state) {
    try {
    return state_names_.at(state);
    } catch (const std::out_of_range& e) {
      throw std::out_of_range("Invalid playback state value: "
                              + std::to_string(state));
    }
  }

 private:
  std::atomic<State> state_;

  StateChangeCallback state_change_callback_;
  std::mutex state_callback_mutex_;

  static const std::map<State, std::string> state_names_;
};
};

#endif  // LRM_PLAYBACKSTATE_H_
