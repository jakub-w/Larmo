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
