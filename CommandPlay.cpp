#include "CommandPlay.h"

#include <iostream>

#include "cppcodec/base64_default_rfc4648.hpp"

namespace remctl {
  CommandPlay::CommandPlay(std::string_view filename) {
    filename_ = std::string(filename);
    filestream_ = std::ifstream(filename_, std::ios::binary);
  }

  CommandPlay::~CommandPlay() {}

  void CommandPlay::Execute() {
    bytes_.push_back(command_type::PLAY);
    {
      std::string encoded = base64::encode(filename_);
      std::cout << "base64:\n" << encoded << "\n\n";
      std::move(encoded.begin(), encoded.end(), std::back_inserter(bytes_));
    }
    // \0 means beggining of the file
    bytes_.push_back(0);

    // TEMP - print binary representation of bytes_
    std::string result;
    result.reserve(bytes_.size() * 8);
    for (auto it = bytes_.begin(); it != bytes_.end(); ++it) {
      for (long unsigned int i = 0; i < sizeof(unsigned char) * 8; ++i) {
	result.append(std::to_string((bool)(*it & (1 << i))));
      }
      result.push_back(' ');
    }
    std::cout << "binary:\n" << result << '\n';
    // END TEMP

    // Send bytes_ through a socket and then read from filestream_ into
    // the socket
  }
}
