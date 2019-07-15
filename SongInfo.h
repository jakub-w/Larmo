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

#ifndef LRM_SONGINFO_H_
#define LRM_SONGINFO_H_

#include <mutex>

class SongInfo {
 public:
  SongInfo()
      : filename_(""),
        total_time_(0),
        current_time_(0),
        remaining_time_(0) {}

  SongInfo(SongInfo& song_info) {
    std::lock_guard<std::mutex> lck(song_info.mtx_);
    filename_ = song_info.Filename();
    total_time_ = song_info.TotalTime();
    current_time_ = song_info.CurrentTime();
    remaining_time_ = song_info.RemainingTime();
  }

  inline const std::string& Filename() {
    std::lock_guard<std::mutex> lck(mtx_);
    return filename_;
  }
  inline void SetFilename(const std::string& filename) {
    std::lock_guard<std::mutex> lck(mtx_);
    filename_ = filename;
  }
  inline void SetFilename(const char* filename) {
    std::lock_guard<std::mutex> lck(mtx_);
    filename_ = filename;
  }

  // inline const std::string& Title() {
  //   std::lock_guard<std::mutex> lck(mtx_);
  //   return title_;
  // }
  // inline void SetTitle(const std::string& title) {
  //   std::lock_guard<std::mutex> lck(mtx_);
  //   title_ = title;
  // }

  // inline const std::string& Album() {
  //   std::lock_guard<std::mutex> lck(mtx_);
  //   return album_;
  // }
  // inline void SetAlbum(const std::string& album) {
  //   std::lock_guard<std::mutex> lck(mtx_);
  //   album_ = album;
  // }

  // inline const std::string& Artist() {
  //   std::lock_guard<std::mutex> lck(mtx_);
  //   return artist_;
  // }
  // inline void SetArtist(const std::string& artist) {
  //   std::lock_guard<std::mutex> lck(mtx_);
  //   artist_ = artist;
  // }

  inline const float& TotalTime() {
    std::lock_guard<std::mutex> lck(mtx_);
    return total_time_;
  }
  inline void SetTotalTime(const float& total_time) {
    std::lock_guard<std::mutex> lck(mtx_);
    total_time_ = total_time;
  }

  inline const float& CurrentTime() {
    std::lock_guard<std::mutex> lck(mtx_);
    return current_time_;
  }
  inline void SetCurrentTime(const float& current_time) {
    std::lock_guard<std::mutex> lck(mtx_);
    current_time_ = current_time;
  }

  inline const float& RemainingTime() {
    std::lock_guard<std::mutex> lck(mtx_);
    return remaining_time_;
  }
  inline void SetRemainingTime(const float& remaining_time) {
    std::lock_guard<std::mutex> lck(mtx_);
    remaining_time_ = remaining_time;
  }

 private:
  std::string filename_;
  // std::string title_;
  // std::string album_;
  // std::string artist_;
  float total_time_;
  float current_time_;
  float remaining_time_;

  std::mutex mtx_;
};

#endif  // LRM_SONGINFO_H_
