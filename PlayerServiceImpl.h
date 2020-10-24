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

#ifndef LRM_PLAYERSERVICEIMPL_H
#define LRM_PLAYERSERVICEIMPL_H

#include "player_service.grpc.pb.h"

#include <array>
#include <set>

#include "Config.h"
#include "Player.h"
#include "Util.h"
#include "crypto/CryptoUtil.h"

using namespace grpc;

namespace lrm {
class PlayerServiceImpl : public PlayerService::Service {
  Player player;

  bool check_auth(const ServerContext* context) const;

  std::string generate_session_key();

 public:
  PlayerServiceImpl();
  virtual ~PlayerServiceImpl();

  Status AudioStream(ServerContext* context,
                     ServerReader<AudioData>* reader,
                     MpvResponse *response);

  Status Stop(ServerContext* context,
              const Empty*,
              MpvResponse* response);

  Status TogglePause(ServerContext* context,
                     const Empty*,
                     MpvResponse* response);

  Status Volume(ServerContext* context,
                const VolumeMessage* volume,
                MpvResponse* response);

  Status Seek(ServerContext* context,
              const SeekMessage* seek,
              MpvResponse* response);

  Status Ping(ServerContext* context,
              const Empty*,
              Empty*);

  Status TimeInfoStream(ServerContext* context,
                        ServerReaderWriter<TimeInfo, TimeInterval>*
                        stream);

  Status Authenticate(ServerContext* context,
                      ServerReaderWriter<AuthData, AuthData>* stream);

 private:
  // TODO: Would be cool if it could be static, but the config isn't being
  //       initialized at static time, so it could return empty passphrase.
  const crypto::EcPoint secret =
      crypto::make_generator(Config::Get("passphrase"));

  const std::string server_id =
      "LRM_SERVER-" + crypto::generate_random_hex(6);

  // TODO: Make sure the old keys are being deleted after the client has
  //       disconnected.
  //       Maybe every time a request is issued from the client the timestamp
  //       for the key is updated and the keys are released periodically if
  //       the timestamp hasn't been touched in a while.
  std::set<std::string> authenticated_sessions_;
  std::mutex sessions_mtx_;

  // Variables for TimeInfoBidiStream
  PlaybackState::State playback_state_ = PlaybackState::UNDEFINED;
  std::mutex playback_state_mtx_;
  std::condition_variable playback_state_cv_;
  // End of variables for TimeInfoBidiStream
};
}

#endif // LRM_PLAYERSERVICEIMPL_H
