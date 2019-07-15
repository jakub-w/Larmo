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

#ifndef LRM_UTIL_H
#define LRM_UTIL_H

#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/stat.h>

#include "player_service.pb.h"
#include "PlaybackState.h"

namespace lrm {
class InterruptableSleeper {
 public:
  template<class Rep, class Period>
  void SleepFor(const std::chrono::duration<Rep, Period>& sleep_duration) {
    std::unique_lock<std::mutex> lck(cv_mtx_);
    cv_.wait_for(lck, sleep_duration);
  }
  template<class Clock, class Duration>
  void SleepUntil(const std::chrono::time_point<Clock, Duration>&
                  sleep_time) {
    std::unique_lock<std::mutex> lck(cv_mtx_);
    cv_.wait_until(lck, sleep_time);
  }

  void Interrupt() {
    std::lock_guard<std::mutex> lck(cv_mtx_);
    cv_.notify_all();
  }

 private:
  std::condition_variable cv_;
  std::mutex cv_mtx_;
};

std::vector<std::string> tokenize(std::string_view str,
                                  std::string_view delimiters = " ");
bool is_ipv4(std::string_view ip);
std::string file_to_str(std::string_view filename);
inline bool file_exists(std::string_view filename) {
  struct stat buffer;
  return stat(filename.data(), &buffer) == 0;
}

// TODO: Rethink if this should be here
const static std::map<TimeInfo::PlaybackState, lrm::PlaybackState::State>
time_info_playback_state_translation_map =
{{TimeInfo::NOT_CHANGED, lrm::PlaybackState::UNDEFINED},
 {TimeInfo::PLAYING, lrm::PlaybackState::PLAYING},
 {TimeInfo::PAUSED, lrm::PlaybackState::PAUSED},
 {TimeInfo::STOPPED, lrm::PlaybackState::STOPPED},
 {TimeInfo::FINISHED, lrm::PlaybackState::FINISHED},
 {TimeInfo::FINISHED_ERROR, lrm::PlaybackState::FINISHED_ERROR}};
}

#endif // LRM_UTIL_H
