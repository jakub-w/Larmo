#include "PlayerServiceImpl.h"

#include "grpcpp/security/credentials.h"
#include "grpcpp/server.h"

#include "Config.h"
#include "Util.h"

#define CHECK_PASSPHRASE(context)                                       \
  if(not check_passphrase_(context)) {                                  \
    return Status(StatusCode::PERMISSION_DENIED, "Wrong passphrase.");  \
  }

using namespace grpc;

bool
PlayerServiceImpl::check_passphrase_(const ServerContext* context) const {
  auto pass = context->client_metadata().find("x-custom-passphrase");
  if(context->client_metadata().end() == pass) {
    return false;
  }
  return pass->second == Config::Get("passphrase");
}

Status
PlayerServiceImpl::PlayFrom(ServerContext* context,
                            const StreamingPort* port,
                            MpvResponse* response) {
  CHECK_PASSPHRASE(context)

      std::vector<std::string> peer = lrm::tokenize(context->peer(), ":");

  if (peer.size() != 3 or not lrm::is_ipv4(peer[1])) {
    std::cerr << "Couldn't retrieve client's address.\n";
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
  CHECK_PASSPHRASE(context)

      response->set_response(player.Stop());

  return Status::OK;
}

Status
PlayerServiceImpl::TogglePause(ServerContext* context,
                               const Empty*,
                               MpvResponse* response) {
  CHECK_PASSPHRASE(context)

      response->set_response(player.TogglePause());

  return Status::OK;
}
