#ifndef LRM_UTIL_H
#define LRM_UTIL_H

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace lrm {
std::vector<std::string> tokenize(std::string_view str,
                                  std::string_view delimiters = " ");
bool is_ipv4(std::string_view ip);
std::string file_to_str(std::string_view filename);
}

#endif // LRM_UTIL_H
