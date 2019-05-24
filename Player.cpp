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

#include "Player.h"

#include <iostream>

#include "mpv/client.h"

namespace lrm {
int Player::send_command_(const std::vector<std::string>&& args) {
  const char* command[16];
  int arg_count = std::min<size_t>(args.size(), 15);

  for(auto i = 0; i < arg_count; ++i) {
    command[i] = args[i].c_str();
  }
  command[arg_count] = nullptr;

  return check_result(mpv_command(ctx_.get(), command));
}

Player::Player() : ctx_(mpv_create(), &mpv_terminate_destroy) {
  mpv_initialize(ctx_.get());
  mpv_set_property_string(ctx_.get(), "log-file", "mpv.log");
  mpv_request_log_messages(ctx_.get(), "debug");
}

Player::~Player() {
  tcp_sock_.close();
}

int Player::Play() {
  return send_command_({"loadfile", input_});
}

int Player::TogglePause() {
  int is_paused;
  check_result(mpv_get_property(ctx_.get(), "pause",
                                MPV_FORMAT_FLAG, &is_paused));

  int pause_flag = is_paused == 0 ? 1 : 0;

  return check_result(mpv_set_property(ctx_.get(), "pause",
                                       MPV_FORMAT_FLAG, &pause_flag));
}

int Player::Stop() {
  return send_command_({"stop"});
}

int Player::PlayFrom(std::string_view host, std::string_view port) {
  std::cout << "Closing the socket...\n";
  tcp_sock_.close();
  std::cout << "Socket closed.\n";

  std::cout << "Created a socket, connecting...\n";
  std::cout << "Playing from: " << host << ", " << port << '\n';

  endpoint_.address(address());
  endpoint_.port(0);
  tcp::resolver::query query(host.data(), port.data());
  auto endpoints = tcp_resolver_.resolve(query);

  for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
      if (it->endpoint().protocol() == tcp::v4()) {
        endpoint_ = it->endpoint();
        break;
      }
  }
  tcp_sock_.connect(endpoint_);
  std::cout << "Connected.\n";

  std::cout << "Sockfd: " << tcp_sock_.native_handle() << "\n";

  Input("fd://" + std::to_string(tcp_sock_.native_handle()));

  return Play();
}
}
