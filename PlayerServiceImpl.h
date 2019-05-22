#ifndef LRM_PLAYERSERVICEIMPL_H
#define LRM_PLAYERSERVICEIMPL_H

#include "player_service.grpc.pb.h"

#include "Player.h"

using namespace grpc;

class PlayerServiceImpl : public PlayerService::Service {
  lrm::Player player;

  bool check_passphrase_(const ServerContext* context) const;

 public:
  Status PlayFrom(ServerContext* context,
                  const StreamingPort* port,
                  MpvResponse* response);

  Status Stop(ServerContext* context,
              const Empty*,
              MpvResponse* response);

  Status TogglePause(ServerContext* context,
                     const Empty*,
                     MpvResponse* response);
};

#endif // LRM_PLAYERSERVICEIMPL_H
