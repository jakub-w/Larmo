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

#include "PlayerServiceImpl.h"

#include "grpcpp/security/credentials.h"
#include "grpcpp/server.h"

#include "spdlog/spdlog.h"

#include "Config.h"
#include "PlaybackState.h"
#include "Util.h"

#define CHECK_PASSPHRASE(context)                                       \
  if(not check_passphrase_(context)) {                                  \
    return Status(StatusCode::PERMISSION_DENIED, "Wrong passphrase.");  \
  }

using namespace grpc;

namespace lrm {
bool
PlayerServiceImpl::check_passphrase_(const ServerContext* context) const {
  auto pass = context->client_metadata().find("x-custom-passphrase");
  if(context->client_metadata().end() == pass) {
    return false;
  }
  return pass->second == Config::Get("passphrase");
}

PlayerServiceImpl::PlayerServiceImpl() : PlayerService::Service() {
  player.SetStateChangeCallback(
      [&](const lrm::PlaybackState::State& state) {
        std::lock_guard<std::mutex> lck(playback_state_mtx_);
        playback_state_ = state;
        playback_state_cv_.notify_all();
      });
}

PlayerServiceImpl::~PlayerServiceImpl() {
  // Break TimeInfoStream threads' sleep.
  playback_state_cv_.notify_all();
}

Status
PlayerServiceImpl::PlayFrom(ServerContext* context,
                            const StreamingPort* port,
                            MpvResponse* response) {
  CHECK_PASSPHRASE(context);

  spdlog::debug("Peer: {}", context->peer());
  std::vector<std::string> peer = lrm::tokenize(context->peer(), ":");

  if (peer.size() != 3 or not lrm::is_ipv4(peer[1])) {
    spdlog::error("Couldn't retrieve IPv4 client address from '{}'",
                  context->peer());
    return Status(StatusCode::ABORTED,
                  "Couldn't retrieve client's address '" + peer[1] + "'.");
  }

  response->set_response(
      player.PlayFrom(peer[1], std::to_string(port->port())));

  return Status::OK;
}

Status
PlayerServiceImpl::Stop(ServerContext* context,
                        const Empty*,
                        MpvResponse* response) {
  CHECK_PASSPHRASE(context);

  response->set_response(player.Stop());

  return Status::OK;
}

Status
PlayerServiceImpl::TogglePause(ServerContext* context,
                               const Empty*,
                               MpvResponse* response) {
  CHECK_PASSPHRASE(context);

  response->set_response(player.TogglePause());

  return Status::OK;
}

Status
PlayerServiceImpl::Volume(ServerContext* context,
                          const VolumeMessage* volume,
                          MpvResponse* response) {
  CHECK_PASSPHRASE(context);

  response->set_response(player.Volume(volume->volume()));
  // We want to update clients whenever volume changes
  playback_state_cv_.notify_all();

  return Status::OK;
}

Status
PlayerServiceImpl::Seek(ServerContext* context,
                        const SeekMessage* seek,
                        MpvResponse* response) {
  CHECK_PASSPHRASE(context);

  response->set_response(player.Seek(seek->seconds()));

  return Status::OK;
}

Status
PlayerServiceImpl::Ping(ServerContext* context,
                        const Empty*,
                        Empty*) {
  CHECK_PASSPHRASE(context);

  return Status::OK;
}

Status PlayerServiceImpl::TimeInfoStream(
    ServerContext* context,
    ServerReaderWriter<TimeInfo, TimeInterval>* stream) {
  CHECK_PASSPHRASE(context);

  TimeInterval time_interval;
  if (not stream->Read(&time_interval)) {
    return grpc::Status(
        StatusCode::ABORTED,
        "Couldn't get streaming interval time from the client");
  }

  std::mutex interval_mtx;
  std::chrono::milliseconds interval{time_interval.milliseconds()};
  spdlog::debug("Info stream interval set to {}s",
                interval.count() / (float)1000);

  std::atomic<bool> close_stream = false;

  auto reader = std::thread(
      [&]{
        while (stream->Read(&time_interval)) {
          spdlog::debug(
              "Client requested to change the update interval to {} s",
              time_interval.milliseconds() / (float)1000);

          std::lock_guard<std::mutex> lck(interval_mtx);
          interval = std::chrono::milliseconds(time_interval.milliseconds());
        }
        spdlog::debug(
            "Client requested the cancelletion of the info stream.");
        close_stream = true;

        playback_state_cv_.notify_all();
      });

  std::chrono::time_point<std::chrono::system_clock> time =
      std::chrono::system_clock::now();

  TimeInfo time_info;

  lrm::PlaybackState::State
      new_state = player.GetPlaybackState(),
      old_state = lrm::PlaybackState::UNDEFINED;
  bool state_changed = false;
  bool force_update = false;
  int old_volume = 0, new_volume = 0;

  // Runs at intervals, the playback state change makes it run early
  while (not close_stream) {
    try {
      state_changed = (new_state != old_state);

      if (state_changed) {
        std::string state_str;
        switch (new_state) {
          case lrm::PlaybackState::PLAYING:
            time_info.set_playback_state(TimeInfo::PLAYING);
            break;
          case lrm::PlaybackState::PAUSED:
            time_info.set_playback_state(TimeInfo::PAUSED);
            break;
          case lrm::PlaybackState::STOPPED:
            time_info.set_playback_state(TimeInfo::STOPPED);
            break;
          case lrm::PlaybackState::FINISHED:
            time_info.set_playback_state(TimeInfo::FINISHED);
            break;
          case lrm::PlaybackState::FINISHED_ERROR:
            time_info.set_playback_state(TimeInfo::FINISHED_ERROR);
            break;
          case lrm::PlaybackState::UNDEFINED:
            break;
        }
        spdlog::debug("Sending playback state to the client: {}",
                      lrm::PlaybackState::StateName(new_state));

        old_state = new_state;
      } else {
        time_info.clear_playback_state();
      }

      // Update time_info only if PLAYING or just switched to PAUSED
      bool clear = false;
      if (lrm::PlaybackState::PLAYING == new_state or
          (lrm::PlaybackState::PAUSED == new_state and state_changed)) {
        try {
          time_info.set_current_time(player.TimePosition());
          time_info.set_remaining_time(player.TimeRemaining());
          time_info.set_total_time(player.TotalTime());
          time_info.set_remaining_playtime(player.PlayTimeRemaining());
        } catch (const lrm::MpvException& e) {
          spdlog::warn("mpv property '{}' couldn't be retrieved: {}",
                       e.details(), e.what());
          clear = true;
        }
      } else {
        clear = true;
      }

      if (clear) {
        // Don't run time_info.Clear() because the time_info.playback_state
        // may be set.
        time_info.clear_current_time();
        time_info.clear_total_time();
        time_info.clear_remaining_time();
        time_info.clear_remaining_playtime();
      }

      new_volume = player.Volume();
      time_info.set_volume(new_volume);

      force_update = state_changed | (new_volume != old_volume);

      if ((lrm::PlaybackState::PLAYING == new_state) or force_update) {
        if (not stream->Write(time_info)) {
          close_stream = true;
          break;
        }
      }

      {
        std::lock_guard<std::mutex> lck(interval_mtx);
        time += interval;
      }
      {
        std::unique_lock<std::mutex> lck(playback_state_mtx_);
        playback_state_cv_.wait_until(lck, time);
        // playback_state_ is updated by the callback defined in the
        // constructor
        new_state = playback_state_;
      }
      old_volume = new_volume;
    } catch (const std::exception& e) {
      spdlog::error("Info stream loop: {}", e.what());
    }
  }

    spdlog::debug("Closing the info stream.");
    reader.join();

    return Status::OK;
}
}
