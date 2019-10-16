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

#include "Util.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <arpa/inet.h>

namespace lrm::Util {
std::vector<std::string> tokenize(std::string_view str,
                                  std::string_view delimiters) {
  std::vector<std::string> result;

  const char* current = str.begin();
  const char* tk_begin = current;

  while (current < str.end()) {
    if (std::any_of(std::begin(delimiters), std::end(delimiters),
                    [current](char delim){return delim == *current;})) {
      result.emplace_back(tk_begin, current);
      tk_begin = current + 1;
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
    const size_t length = in_stream.tellg();

    in_stream.seekg(0, std::ios::beg);

    std::string result(length, ' ');
    in_stream.read(result.data(), length);

    return result;
  }

  return std::string();
}

void check_port(std::string_view port_str) {
  try {
    const int port = std::stoi(port_str.data());
    if ((port <= IPPORT_USERRESERVED or port > UINT16_MAX) and
        (port != 0)) {
      throw std::logic_error("Port should be in the range: [" +
                             std::to_string(IPPORT_RESERVED) + "; " +
                             std::to_string(UINT16_MAX) + "]");
    }
  } catch (const std::exception& e) {
    throw std::logic_error(std::string("Port '") + port_str.data() +
                           "' is invalid: " + e.what());
  }
}

std::vector<std::byte> str_to_bytes(std::string_view str) {
  auto result = std::vector<std::byte>(str.size());
  std::memcpy(result.data(), str.data(), str.size());
  return result;
}
}
