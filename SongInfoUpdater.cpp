#include "SongInfoUpdater.h"

#include <algorithm>
#include <thread>

#include "spdlog/spdlog.h"

#include "Util.h"

SongInfoUpdater::~SongInfoUpdater() {
  Stop();
}

void SongInfoUpdater::Start(std::chrono::milliseconds update_interval) {
  {
    std::lock_guard<std::mutex> lck(is_updating_mtx_);
    if (is_updating_) {
      return;
    }
  }

  thread_ = std::thread(&SongInfoUpdater::continuous_update,
                        this,
                        std::move(update_interval));
}

void SongInfoUpdater::Stop() {
  {
    std::lock_guard<std::mutex> lck(is_updating_mtx_);
    is_updating_ = false;
  }
  is_updating_cv_.notify_all();

  thread_.join();
}

void SongInfoUpdater::continuous_update(std::chrono::milliseconds
                                        update_interval) {
  is_updating_ = true;

  grpc::ClientContext context;
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
  while (stream->Read(&time_info)) {
    if (time_info.playback_state() != TimeInfo::NOT_CHANGED) {
      // Copy the callback to prevent data races and to not lock the mutex
      // while executing unknown code.
      StateChangeCallback callback;
      {
        std::lock_guard<std::mutex> lck(state_callback_mtx_);
        callback = state_change_callback_;
      }
      if (callback) {
        try {
          callback(lrm::time_info_playback_state_translation_map
                   .at(time_info.playback_state()));
        } catch (const std::out_of_range& e) {
          spdlog::error("In state change callback: Playback state not in the "
                        "translation map: {}",
                        std::to_string(time_info.playback_state()));
        } catch (const std::exception& e) {
          spdlog::error("Error in state change callback: {}", e.what());
        } catch (...) {
          spdlog::error("Unknown error in state change callback");
        }
      }
    }

    song_info_->SetTotalTime(time_info.total_time());
    song_info_->SetCurrentTime(time_info.current_time());
    song_info_->SetRemainingTime(time_info.remaining_time());

    spdlog::debug("total_time: {}, current_time: {}, remaining_time: {}",
                  song_info_->TotalTime(),
                  song_info_->CurrentTime(),
                  song_info_->RemainingTime());
  }

  writer.join();
  auto status = stream->Finish();
  if (status.ok()) {
    spdlog::debug("The info stream has closed");
  } else {
    spdlog::error("The info stream has closed with an error: {}",
                  status.error_message());
  }
}
