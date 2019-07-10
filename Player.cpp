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

#include "spdlog/spdlog.h"

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
  mpv_set_property_string(ctx_.get(), "video", "no");
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

int Player::Volume(std::string_view volume) {
  try {
    if (volume.empty()) {
      throw std::invalid_argument("Volume argument is empty");
    }

    int64_t vol;

    if (volume[0] == '+' or volume[0] == '-') {
      int relative_volume;

      // if just '+' or '-', increment or decrement by 5
      if (volume.length() < 2) {
        relative_volume = 5;
      } else {
        relative_volume = std::stoi(volume.data() + 1);
      }

      mpv_get_property(ctx_.get(), "volume", MPV_FORMAT_INT64, &vol);

      if (volume[0] == '+') {
        vol += relative_volume;
      } else {
        vol -= relative_volume;
      }
    } else {
      vol = std::stoi(volume.data());
    }
    vol = std::clamp(vol, (int64_t)0, (int64_t)100);

    return check_result(
          mpv_set_property(ctx_.get(), "volume", MPV_FORMAT_INT64, &vol));
  } catch (const std::invalid_argument& e) {
    return MPV_ERROR_INVALID_PARAMETER;
  }
}

int Player::PlayFrom(std::string_view host, std::string_view port) {
  try {
  spdlog::debug("Closing the socket...");
  tcp_sock_.close();
  spdlog::debug("Socket closed");

  // Resolve the address from 'host' argument
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
  spdlog::info("Playing from: {}:{}",
               endpoint_.address().to_string(), endpoint_.port());

  std::this_thread::sleep_for(std::chrono::seconds(1));
  tcp_sock_.connect(endpoint_);
  spdlog::debug("Connected");

  spdlog::debug("Sockfd: {}", tcp_sock_.native_handle());

  Input("fd://" + std::to_string(tcp_sock_.native_handle()));

  return Play();
  } catch (const std::exception& e) {
    // TODO: proper error handling
    spdlog::error(e.what());
    return -1;
  }
}

int64_t Player::get_property_int64_(const std::string_view prop_name) const {
  int64_t prop_value = 0;
  int result = mpv_get_property(ctx_.get(), prop_name.data(),
                                MPV_FORMAT_INT64, &prop_value);
  if (MPV_ERROR_SUCCESS != result) {
    throw lrm::MpvException((mpv_error)result);
  }

  return prop_value;
}

double Player::get_property_double_(const std::string_view prop_name) const {
  double prop_value = 0;
  int result = mpv_get_property(ctx_.get(), prop_name.data(),
                                MPV_FORMAT_DOUBLE, &prop_value);
  if (MPV_ERROR_SUCCESS != result) {
    throw lrm::MpvException((mpv_error)result);
  }

  return prop_value;
}
}
