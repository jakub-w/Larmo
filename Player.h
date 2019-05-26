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

#ifndef LRM_PLAYER_H
#define LRM_PLAYER_H

#include <iostream>

#include "mpv/client.h"

#include "asio.hpp"

namespace lrm {
using namespace asio::ip;

class Player {
  std::string input_;
  std::unique_ptr<mpv_handle, decltype(&mpv_terminate_destroy)> ctx_;

  asio::io_context io_context_;
  tcp::socket tcp_sock_ = tcp::socket(io_context_);
  tcp::endpoint endpoint_;
  tcp::resolver tcp_resolver_ = tcp::resolver(io_context_);

  inline int check_result(int result) {
    if (0 != result) {
      std::cerr << mpv_error_string(result) << '\n';
    }
    return result;
  }

  int send_command_(const std::vector<std::string>&& args);

 public:
  Player();
  ~Player();

  inline void Input(std::string_view input) {
    input_ = input;
  }

  int Play();
  int TogglePause();
  int Stop();

  // volume - can be a number from [0; 100] or +/-number, e.g. +10 or 75
  int Volume(std::string_view volume);
  int PlayFrom(std::string_view host, std::string_view port);
};
}

#endif // LRM_PLAYER_H
