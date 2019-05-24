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

#ifndef LRM_CONFIG_H
#define LRM_CONFIG_H

#include <string>
#include <map>

class Config {
  static const std::string default_conf_file_;
  static std::map<std::string, std::string> config_;

  static enum State {
    NOT_LOADED,
    LOADED,
    ERROR
  } state_;

public:
  static void Load(std::string_view filename = default_conf_file_);

  static const std::string& Get(std::string_view variable);
  static inline State GetState() {
    return state_;
  }
  static void Set(std::string_view variable, std::string_view value);
};

#endif // LRM_CONFIG_H
