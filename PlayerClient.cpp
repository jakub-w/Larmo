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

#include "PlayerClient.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>
#include <netinet/in.h>

#include <openssl/ec.h>

#ifndef INCLUDE_GRPCPLUSPLUS
#include "grpcpp/channel.h"
#else
#include "grpc++/channel.h"
#endif  // INCLUDE_GRPCPLUSPLUS

#include "spdlog/spdlog.h"

#include "ClientContexts.h"
#include "Config.h"
#include "crypto/CryptoUtil.h"
#include "crypto/ZkpSerialization.h"

namespace lrm {
std::vector<char> PlayerClient::read_file(std::string_view filename) {
  std::ifstream ifs(filename.data(), std::ios::binary | std::ios::in);
  if (not ifs.is_open()) {
    throw std::invalid_argument(
        std::string("Couldn't open the file: ") + filename.data());
  }
  ifs.seekg(0, std::ios::end);
  const size_t length = ifs.tellg();

  ifs.seekg(0, std::ios::beg);

  std::vector<char> output(length);
  ifs.read(output.data(), length);
  ifs.close();

  return output;
}

void PlayerClient::start_updating_info() {
  assert(std::any_of(std::begin(session_key_), std::end(session_key_),
                     [](char c){ return c != ' '; })
         && "Session key is not initialized! Use Authenticate() before "
         "StreamInfoStart().");

  spdlog::info("Starting song info stream...");
  try {
    synchronizer_.Start();
  } catch (const std::system_error& e) {
    spdlog::error("Couldn't start song info stream: {}", e.what());
  }
}
void PlayerClient::stop_updating_info() {
  synchronizer_.Stop();
}

PlayerClient::PlayerClient(std::shared_ptr<grpc::Channel> channel) noexcept
    : stub_(PlayerService::NewStub(channel)),
      synchronizer_(stub_.get(), session_key_),
      log_(spdlog::get("PlayerClient")) {
  try {
    synchronizer_.SetCallbackOnStatusChange(
        [this](PlaybackState::State state){
          if (PlaybackState::FINISHED == state or
              PlaybackState::FINISHED_ERROR == state) {
            // Copy the callback to prevent data races and to not lock the
            // mutex while executing unknown code.
            SongFinishedCallback callback;
            {
              std::lock_guard<std::mutex> lck(song_finished_mtx_);
              callback = song_finished_callback_;
            }
            if (callback) {
              callback(state);
            }
          }

          spdlog::debug("Received playback state change from server: {}",
                        PlaybackState::StateName(state));
        });
  } catch (const std::system_error& e) {
    spdlog::error("Couldn't register the state change callback: {}",
                  e.what());
  }
}

PlayerClient::~PlayerClient() {
  try {
    log_->flush();
  } catch (...) {}
}

bool PlayerClient::Authenticate() {
  UnauthenticatedContext context;

  std::shared_ptr<grpc::ClientReaderWriter<AuthData, AuthData>> stream{
    stub_->Authenticate(&context)};

  AuthData data;

  const auto generator = crypto::make_generator(Config::Get("passphrase"));
  const auto [privkey, pubkey] = crypto::generate_key_pair(generator.get());

  const auto pubkey_vect =
      crypto::EcPointToBytes(pubkey.get(), POINT_CONVERSION_COMPRESSED);

  try {
    ZkpMessage* zkp = new ZkpMessage(
        crypto::zkp_serialize(
            crypto::make_zkp(crypto::generate_random_hex(16),
                             privkey.get(),
                             pubkey.get(), generator.get())));
    data.set_allocated_zkp(zkp);
  } catch (const std::exception& e) {
    spdlog::error("Error when creating or serializing ZKP:\n\t{}",
                  e.what());
    return false;
  }

  data.set_public_key(pubkey_vect.data(), pubkey_vect.size());
  stream->Write(data);
  stream->WritesDone();

  data.Clear();

  stream->Read(&data);

  const auto status = stream->Finish();
  if (status.ok()) {
    spdlog::debug("Authentification stream has closed");
  } else {
    spdlog::error("Authentification stream has closed with an error: {}",
                  status.error_message());
  }

  if (data.denied()) {
    spdlog::error("Authentication unsuccessful. Wrong password?");
    return false;
  } else if (data.data().empty()) {
    spdlog::error("Authentication ended successfully but no session key "
                  "received");
    return false;
  }

  assert(data.data().size() == session_key_.size());
  std::copy(data.data().begin(), data.data().end(), session_key_.begin());

  spdlog::info("Authentication successful");

  return true;
}

int PlayerClient::Play(std::string_view filename) {
  log_->debug("PlayerClient::Play(\"{}\")", filename);

  AuthenticatedContext context(session_key_);
  MpvResponse response;

  auto writer = stub_->AudioStream(&context, &response);

  AudioData data;
  size_t written = 0;
  streaming_file_ = read_file(filename);

  constexpr size_t PACKAGE_BYTES = 1024;

  while (written < streaming_file_.size()) {
    const size_t to_send = std::min(PACKAGE_BYTES,
                                    streaming_file_.size() - written);
    data.set_data(streaming_file_.data() + written,
                  to_send);

    if (not writer->Write(data)) {
      break;
    }

    written += to_send;
  }

  writer->WritesDone();
  auto status = writer->Finish();

  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::Stop() {
  log_->debug("PlayerClient::Stop()");

  AuthenticatedContext context(session_key_);
  MpvResponse response;

  const grpc::Status status = stub_->Stop(&context, Empty(), &response);
  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::TogglePause() {
  log_->debug("PlayerClient::TogglePause()");

  AuthenticatedContext context(session_key_);
  MpvResponse response;

  const grpc::Status status = stub_->TogglePause(&context, Empty(),
                                                 &response);
  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::Volume(std::string_view volume) {
  log_->debug("PlayerClient::Volume(\"{}\")", volume);

  AuthenticatedContext context(session_key_);
  MpvResponse response;

  VolumeMessage vol_msg;
  vol_msg.set_volume(volume.data());

  const grpc::Status status = stub_->Volume(&context, vol_msg, &response);
  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::Seek(std::string_view seconds) {
  log_->debug("PlayerClient::Seek()");

  AuthenticatedContext context(session_key_);
  MpvResponse response;

  SeekMessage seek_msg;
  const int usecs =
      std::clamp(std::stoi(seconds.data()), INT32_MIN, INT32_MAX);

  seek_msg.set_seconds(usecs);

  const grpc::Status status = stub_->Seek(&context, seek_msg, &response);
  if (status.ok()) {
    return status.ok();
  } else {
    throw status;
  }
}

PlaybackSynchronizer::PlaybackInfo PlayerClient::GetPlaybackInfo() {
  return synchronizer_.GetPlaybackInfo();
}

bool PlayerClient::Ping() {
  log_->debug("PlayerClient::Ping()");

  AuthenticatedContext context(session_key_);
  Empty empty;

  const grpc::Status status = stub_->Ping(&context, empty, &empty);
  if (status.ok()) {
    return status.ok();
  } else {
    throw status;
  }
}

std::string PlayerClient::info_get(
    std::string_view token,
    const PlaybackSynchronizer::PlaybackInfo* playback_info) {
  if (token == "artist") {
    return playback_info->artist;
  } else if (token == "album") {
    return playback_info->album;
  } else if (token == "state") {
    switch (playback_info->playback_state) {
      case PlaybackState::PLAYING:
        return "PLAYING";
      case PlaybackState::PAUSED:
        return "PAUSED";
      case PlaybackState::STOPPED:
        return "STOPPED";
      default:
        return "UNDEFINED";
    }
  } else if (token == "title") {
    return playback_info->title;
  } else if (token == "volume") {
    return std::to_string(playback_info->volume);
  } else if (token == "tt") {
    return std::to_string(playback_info->total_time.count());
  } else if (token == "et") {
    return std::to_string(playback_info->elapsed_time.count());
  } else if (token == "rt") {
    return std::to_string(playback_info->remaining_time.count());
  } else {
    return "";
  }
}

std::string PlayerClient::Info(std::string_view format) {
  PlaybackSynchronizer::PlaybackInfo playback_info = GetPlaybackInfo();

  std::stringstream result_stream;

  auto begin = format.cbegin();
  const char* tok_beg;
  while ((tok_beg = std::find(begin, format.cend(), '%')) != format.cend()) {
    result_stream.write(begin, tok_beg - begin);

    ++tok_beg;
    const auto tok_end = std::find_if(tok_beg, format.cend(),
                                      [](char c) {
                                        return not std::iswalpha(c);
                                      });

    const std::string token(tok_beg, tok_end - tok_beg);
    const std::string replacement{info_get(token, &playback_info)};
    if (replacement.empty()) {
      result_stream.write(tok_beg - 1, (tok_end - tok_beg) + 1);
    } else {
      result_stream << replacement;
    }

    begin = tok_end;
  }

  return result_stream.str();
}

void PlayerClient::SetSongFinishedCallback(SongFinishedCallback&& callback) {
  std::lock_guard<std::mutex> lck(song_finished_mtx_);
  song_finished_callback_ = callback;
}
}
