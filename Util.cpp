#include "Util.h"

#include <string>
#include <string_view>
#include <vector>

#include <arpa/inet.h>

namespace lrm {
std::vector<std::string> tokenize(std::string_view str,
                                  std::string_view delimiters) {
  std::vector<std::string> result;

  const char* current = str.begin();
  const char* tk_begin = current;

  while (current < str.end()) {
    for(const char delimiter : delimiters) {
      if (delimiter == *current) {
        result.emplace_back(tk_begin, current);
        tk_begin = current + 1;
        break;
      }
    }
    ++current;
  }
  result.emplace_back(tk_begin, str.end());

  return result;
}

bool is_ipv4(std::string_view ip) {
  in_addr output;
  if (0 < inet_pton(AF_INET, ip.data(), &output)) {
    return true;
  }
  return false;
}

std::string file_to_str(std::string_view filename) {
  std::ifstream in_stream(filename.data());

  if(in_stream.is_open()) {
    in_stream.seekg(0, std::ios::end);
    size_t length = in_stream.tellg();

    in_stream.seekg(0, std::ios::beg);

    char* str = new char[length];
    in_stream.read(str, length);

    std::string result(str);
    delete[] str;

    return result;
  }

  return std::string();
}
}
