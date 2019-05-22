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
