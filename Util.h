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
#include <fstream>
#include <functional>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/stat.h>
#include <netinet/in.h>

namespace lrm::Util {
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

/// Wait for the \b pred to return \b true or until the \b timeout was
/// reached.
/// /return \b true if the \b pred finally returned \b true, or \b false if
/// \b timeout was reached.
template<class Predicate, class Rep, class Period>
bool wait_predicate(Predicate&& pred,
                    const std::chrono::duration<Rep, Period>& timeout){
  std::chrono::time_point wake_time =
      std::chrono::high_resolution_clock::now() + timeout;

  while(not std::invoke(pred)) {
    auto wait_time = wake_time - std::chrono::high_resolution_clock::now();
    if (wait_time.count() < 0) return false;
    if (wait_time > std::chrono::milliseconds(100)) {
      wait_time = std::chrono::milliseconds(100);
    }
    std::this_thread::sleep_for(wait_time);
  }
  return true;
}

/// Check if the port is valid. Throw an exception if it's not.
///
/// /exception std::logic_error The exception thrown when the given port isn't
/// valid.
void check_port(std::string_view port_str);
}

#endif // LRM_UTIL_H
