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

#include "TempFile.h"

#include <filesystem>

void TempFile::cleanup() {
  stream_.close();
  std::filesystem::remove(path_);
}

TempFile::TempFile() {}

TempFile::TempFile(std::string_view name) {
  Create(name);
}

TempFile::~TempFile() {
  cleanup();
}

void TempFile::Create(std::string_view name) {
  cleanup();

  std::filesystem::path temp_path(name);
  if (temp_path.is_absolute()) {
    path_ = std::move(temp_path);
  } else {
    path_ = std::filesystem::temp_directory_path().append(name);
  }

  if (std::filesystem::exists(path_)) {
    throw std::filesystem::filesystem_error(
        "Couldn't create a temporary file", path_,
        std::make_error_code(std::errc::file_exists));
  }

  stream_.open(path_, std::ios::out);
  stream_.close();
  stream_.open(path_);
}
