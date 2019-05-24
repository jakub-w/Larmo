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

#include "Config.h"

#include <fstream>
#include <iostream>
#include <regex>

const std::string Config::default_conf_file_ = "lrm.conf";
std::map<std::string, std::string> Config::config_ = {};
Config::State Config::state_ = NOT_LOADED;

// Define methods
void Config::Load(std::string_view filename) {
  if (LOADED == state_) {
    return;
  }

  std::ifstream fs(filename.data());
  if (!fs) {
    state_ = ERROR;
    throw std::logic_error(std::string("Config file '")
                           + filename.data() + "' could not be loaded.");
  }

  for (std::string line; std::getline(fs, line);) {
    std::smatch smatch;
    if (false == std::regex_match(line, smatch,
                                  std::regex("(\\w+)\\s?=\\s?(\\S+)"))) {
      std::cerr << "Line: '" << line << "' could not be parsed.\n";
      continue;
    }

    config_[smatch[1]] = smatch[2];
  }

  state_ = LOADED;
}

const std::string& Config::Get(std::string_view variable) {
  switch(state_) {
    case LOADED:
      // ensure that it won't add unnecessary entries to a map
      try {
        return config_.at(variable.data());
      } catch (const std::out_of_range& e) {
        return config_[""];
      }
    case NOT_LOADED:
      Load();
      return Get(variable);
    default:
      // if someone chooses to ignore error thrown by Load(), he will get an
      // empty string every time
      return config_[""];
  }
}

void Config::Set(std::string_view variable, std::string_view value) {
  config_[variable.data()] = value;
}
