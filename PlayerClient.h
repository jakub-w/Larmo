#ifndef LRM_PLAYERCLIENT_H
#define LRM_PLAYERCLIENT_H

#include <asio.hpp>

#include "player_service.grpc.pb.h"

using namespace grpc;
using namespace asio::ip;

class PlayerClient {
  const std::string STREAMING_PID_FILE = "lrm.pid";
  std::unique_ptr<PlayerService::Stub> stub_;
  StreamingPort port_;

  std::vector<char> read_file(std::string_view filename);
  std::ofstream make_temp_file(std::string_view filename);

  // timeout - in milliseconds
  int wait_for_port(int pipe_fd, unsigned int timeout = 5000);
  int stream_file(const std::string filename, const unsigned short port);

 public:
  PlayerClient(unsigned short streaming_port,
               std::shared_ptr<Channel> channel);
  PlayerClient(const std::string& streaming_port,
               std::shared_ptr<Channel> channel);
  int Play(std::string_view filename);
  int Stop();
  int TogglePause();
};

#endif // LRM_PLAYERCLIENT_H
