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

Player::Player() : ctx_(mpv_create(), &mpv_terminate_destroy),
                   playback_state_(PlaybackState::STOPPED) {
  mpv_initialize(ctx_.get());
  mpv_set_property_string(ctx_.get(), "log-file", "mpv.log");
  mpv_set_property_string(ctx_.get(), "video", "no");
  // NOTE: This allows for cached seeking, but it's pretty unreliable
  mpv_set_property_string(ctx_.get(), "force-seekable", "yes");
  mpv_request_log_messages(ctx_.get(), "debug");

  start_event_loop();
}

Player::~Player() {
  stop_event_loop();
  tcp_sock_.close();
  event_loop_thread_.join();
}

int Player::Play() {
  int result = send_command_({"loadfile", input_});
  return result;
}

int Player::TogglePause() {
  int is_paused;
  check_result(mpv_get_property(ctx_.get(), "pause",
                                MPV_FORMAT_FLAG, &is_paused));

  int pause_flag = is_paused == 0 ? 1 : 0;

  int result =  check_result(mpv_set_property(ctx_.get(), "pause",
                                              MPV_FORMAT_FLAG, &pause_flag));
  if (MPV_ERROR_SUCCESS == result) {
    playback_state_.SetState(
        pause_flag == 1 ? PlaybackState::PAUSED : PlaybackState::PLAYING);
  }

  return result;
}

int Player::Stop() {
  int result = send_command_({"stop"});

  return result;
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

int Player::Seek(int32_t seconds) {
  return send_command_({"seek", std::to_string(seconds)});
}

int Player::PlayFrom(std::string_view host, std::string_view port) {
  try {
  tcp_sock_.close();

  // Resolve the address from 'host' argument
  endpoint_.address(address());
  endpoint_.port(0);
  spdlog::debug("Trying to resolve address: {}:{}...",
                host.data(), port.data());
  auto endpoints = tcp_resolver_.resolve(host.data(), port.data());

  for (auto it = endpoints.begin(); it != endpoints.end(); ++it) {
      if (it->endpoint().protocol() == tcp::v4()) {
        endpoint_ = it->endpoint();
        break;
      }
  }
  spdlog::info("Playing from: {}:{}",
               endpoint_.address().to_string(), endpoint_.port());

  asio::error_code ec;
  const int timer = 1000; // wait for max 1 second
  for (int i = 0; i < timer/250; ++i) {
    tcp_sock_.connect(endpoint_, ec);
    if (ec) {
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    } else {
      break;
    }
  }
  if (ec) {
    spdlog::error("Couldn't connect to a client's streaming port: {}",
                  ec.message());
    return -1;
  }

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
    throw lrm::MpvException((mpv_error)result, prop_name.data());
  }

  return prop_value;
}

double Player::get_property_double_(const std::string_view prop_name) const {
  double prop_value = 0;
  int result = mpv_get_property(ctx_.get(), prop_name.data(),
                                MPV_FORMAT_DOUBLE, &prop_value);
  if (MPV_ERROR_SUCCESS != result) {
    throw lrm::MpvException((mpv_error)result, prop_name.data());
  }

  return prop_value;
}

void Player::start_event_loop() {
  if (event_loop_running_) {
    spdlog::error("Tried to start mpv event loop while it's already running");
    return;
  }
  event_loop_running_ = true;

  try {
    event_loop_thread_ = std::thread(&Player::mpv_event_loop, this);
  } catch (const std::system_error& e) {
    spdlog::error("Couldn't start the mpv event loop thread: {}",
                  e.what());
    event_loop_running_ = false;
  }
}

void Player::stop_event_loop() {
  event_loop_running_ = false;
}

void Player::mpv_event_loop() {
  spdlog::debug("Starting mpv event loop...");
  while (event_loop_running_) {
    mpv_event* event =  mpv_wait_event(ctx_.get(), 1);

    if (event->error != MPV_ERROR_SUCCESS) {
      spdlog::error("mpv event '{}': {}",
                    mpv_event_name(event->event_id),
                    mpv_error_string(event->error));
      continue;
    }

    switch (event->event_id) {
      case MPV_EVENT_NONE:
        continue;
      case MPV_EVENT_END_FILE:
        {
          mpv_event_end_file* end_file_data =
              (mpv_event_end_file*)event->data;

          switch (end_file_data->reason) {
            case MPV_END_FILE_REASON_EOF:
              // playback_state_.SetState(PlaybackState::FINISHED);
              playback_state_.SetState(PlaybackState::STOPPED);
              break;
            case MPV_END_FILE_REASON_ERROR:
              spdlog::error("mpv file ended: {}",
                            mpv_error_string(end_file_data->error));
              // playback_state_.SetState(PlaybackState::FINISHED_ERROR);
              playback_state_.SetState(PlaybackState::STOPPED);
              break;
            case MPV_END_FILE_REASON_STOP: case MPV_END_FILE_REASON_QUIT:
              playback_state_.SetState(PlaybackState::STOPPED);
              break;
            case MPV_END_FILE_REASON_REDIRECT:
              spdlog::warn("MPV_END_FILE_REASON_REDIRECT not implemented");
              break;
            default:
              spdlog::warn("Unknown mpv end file reason");
              break;
          }
        }
        break;
      case MPV_EVENT_FILE_LOADED:
        playback_state_.SetState(PlaybackState::PLAYING);
        break;
      default:
        continue;
    }
  }
  spdlog::debug("Mpv event loop stopped");
}
}
