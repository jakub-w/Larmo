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

#include "PlaybackSynchronizer.h"

#include <thread>

#include "ClientContexts.h"
#include "Util.h"

namespace lrm {
namespace {
const static std::map<TimeInfo::PlaybackState, lrm::PlaybackState::State>
time_info_playback_state_translation_map =
{{TimeInfo::NOT_CHANGED, lrm::PlaybackState::UNDEFINED},
 {TimeInfo::PLAYING, lrm::PlaybackState::PLAYING},
 {TimeInfo::PAUSED, lrm::PlaybackState::PAUSED},
 {TimeInfo::STOPPED, lrm::PlaybackState::STOPPED},
 {TimeInfo::FINISHED, lrm::PlaybackState::FINISHED},
 {TimeInfo::FINISHED_ERROR, lrm::PlaybackState::FINISHED_ERROR}};
}

PlaybackSynchronizer::~PlaybackSynchronizer() {
  Stop();
}
void PlaybackSynchronizer::Start(std::chrono::milliseconds update_interval) {
  {
    std::lock_guard<std::mutex> lck(is_updating_mtx_);
    if (is_updating_) {
      return;
    }
  }

  updating_thread_ = std::thread(&PlaybackSynchronizer::continuous_update,
                        this,
                        std::move(update_interval));
}

void PlaybackSynchronizer::Stop() noexcept {
  try {
    {
      std::lock_guard<std::mutex> lck(is_updating_mtx_);
      is_updating_ = false;
    }
    is_updating_cv_.notify_all();

    if (updating_thread_.joinable())
      updating_thread_.join();
  } catch (const std::system_error& e) {
    spdlog::error("Trying to stop PlaybackSynchronizer: {}", e.what());
  }
}

PlaybackSynchronizer::PlaybackInfo
PlaybackSynchronizer::GetPlaybackInfo() const {
  PlaybackInfo result;

  {
    std::lock_guard lck(base_playback_info.mtx);
    result.title = base_playback_info.info.title;
    result.album = base_playback_info.info.album;
    result.artist = base_playback_info.info.artist;
    result.total_time = base_playback_info.info.total_time;
    result.playback_state = base_playback_info.info.playback_state;
    result.volume = base_playback_info.info.volume;

    if (base_playback_info.info.playback_state == PlaybackState::PLAYING) {
      const auto time_difference =
          std::chrono::steady_clock::now() - base_playback_info.last_update;

      result.elapsed_time =
          base_playback_info.info.elapsed_time + time_difference;
      result.remaining_time =
          base_playback_info.info.remaining_time - time_difference;
    } else {
      result.elapsed_time = base_playback_info.info.elapsed_time;
      result.remaining_time = base_playback_info.info.remaining_time;
    }
  }

  return result;
}

void PlaybackSynchronizer::continuous_update(std::chrono::milliseconds
                                        update_interval) {
  is_updating_ = true;

  std::shared_ptr<grpc::ClientReaderWriter<TimeInterval, TimeInfo>> stream(
      stub_->TimeInfoStream(&context));

  TimeInterval interval;
  interval.set_milliseconds(update_interval.count());

  spdlog::debug("Setting the info stream interval to {}s",
                update_interval.count() / (float) 1000);
  stream->Write(interval);

  // Writer will just wait until SongInfoUpdater::Stop() is called (it's
  // called by the destructor).
  std::thread writer(
      [this, stream]{
        {
          std::unique_lock<std::mutex> lck(is_updating_mtx_);
          is_updating_cv_.wait(lck, [&]{return !is_updating_;});
        }

        stream->WritesDone();
        spdlog::debug(
            "Requesting info stream cancellation...");
      });

  TimeInfo time_info;
  PlaybackState::State current_state = PlaybackState::UNDEFINED;
  bool state_changed = false;

  while (stream->Read(&time_info)) {
    if (time_info.playback_state() != TimeInfo::NOT_CHANGED) {
      state_changed = true;
      current_state = time_info_playback_state_translation_map
                      .at(time_info.playback_state());
    } else {
      state_changed = false;
    }

    {
      std::lock_guard lck(base_playback_info.mtx);
      base_playback_info.last_update = std::chrono::steady_clock::now();

      base_playback_info.info.playback_state = current_state;

      base_playback_info.info.volume = time_info.volume();

      base_playback_info.info.total_time =
          std::chrono::duration<double>(time_info.total_time());
      base_playback_info.info.elapsed_time =
          std::chrono::duration<double>(time_info.current_time());
      base_playback_info.info.remaining_time =
          std::chrono::duration<double>(time_info.remaining_time());

      spdlog::debug("total_time: {}, elapsed_time: {}, remaining_time: {}",
                    base_playback_info.info.total_time.count(),
                    base_playback_info.info.elapsed_time.count(),
                    base_playback_info.info.remaining_time.count());
    AuthenticatedContext context{session_key_};
    }

    // This must be run after base_playback_info has been updated because it
    // calls the callback.
    if (state_changed) {
      playback_state_.SetState(current_state);
    }
  }

  writer.join();
  const auto status = stream->Finish();
  if (status.ok()) {
    spdlog::debug("The info stream has closed");
  } else {
    spdlog::error("The info stream has closed with an error: {}",
                  status.error_message());
  }
}
}
