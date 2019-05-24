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

#ifndef LRM_TEMPFILE_H
#define LRM_TEMPFILE_H

#include <filesystem>
#include <fstream>
#include <string_view>

class TempFile {
  std::filesystem::path path_;
  std::fstream stream_;

  void cleanup();
 public:
  TempFile();

  // It can take just the name or an absolute path. In the first case it will
  // create a file in the system's temp dir, in the latter it will create a
  // file in the given absolute path.
  TempFile(std::string_view name);
  ~TempFile();

  inline std::fstream& Stream() {
    return stream_;
  }

  void Create(std::string_view name);

  inline std::string Path() const {
    return path_.string();
  }
};

#endif // LRM_TEMPFILE_H
