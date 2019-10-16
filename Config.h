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

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "filesystem.h"

namespace lrm {
class Config {
public:
  enum State {
    NOT_LOADED,
    LOADED,
    ERROR
  };

  Config() = delete;

  static void
  Load(const fs::path& file_path = default_conf_file);

  static const std::string& Get(std::string_view variable);
  static inline State GetState() {
    return state_;
  }

  static inline const std::unordered_map<std::string, std::string>&
  GetMap() {
    return config_;
  }

  // Set only if the 'value' is not empty. To unset use Config::Unset().
  static void Set(std::string_view variable, std::string_view value);

  // Set only if the variable is not yet set (or is empty).
  static void SetMaybe(std::string_view variable, std::string_view value);

  static void Unset(std::string_view variable);

  static void Require(std::string_view variable);

  template<class InputIt>
  static void Require(InputIt first, InputIt last);

  static void Require(
      std::initializer_list<std::unordered_set<std::string>::value_type>
      variables);

  // Returns a vector of names of the missing required variables from the
  // config
  static std::vector<const std::string*> CheckMissing();

  static const fs::path default_conf_file;

 private:
  static std::unordered_map<std::string, std::string> config_;
  static std::unordered_set<std::string> required_;
  static State state_;
};
}

#endif // LRM_CONFIG_H
