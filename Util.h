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

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <sys/stat.h>

namespace lrm {
std::vector<std::string> tokenize(std::string_view str,
                                  std::string_view delimiters = " ");
bool is_ipv4(std::string_view ip);
std::string file_to_str(std::string_view filename);
inline bool file_exists(std::string_view filename) {
  struct stat buffer;
  return stat(filename.data(), &buffer) == 0;
}
}

#endif // LRM_UTIL_H
