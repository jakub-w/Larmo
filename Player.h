#ifndef LRM_PLAYER_H
#define LRM_PLAYER_H

#include <iostream>

#include "mpv/client.h"

#include "asio.hpp"

namespace lrm {
using namespace asio::ip;

class Player {
  std::string input_;
  std::unique_ptr<mpv_handle, decltype(&mpv_terminate_destroy)> ctx_;

  asio::io_context io_context_;
  tcp::socket tcp_sock_ = tcp::socket(io_context_);
  tcp::endpoint endpoint_;
  tcp::resolver tcp_resolver_ = tcp::resolver(io_context_);

  inline int check_result(int result) {
    if (0 != result) {
      std::cerr << mpv_error_string(result) << '\n';
    }
    return result;
  }

  int send_command_(const std::vector<std::string>&& args);

 public:
  Player();
  ~Player();

  inline void Input(std::string_view input) {
    input_ = input;
  }

  int Play();
  int TogglePause();
  int Stop();
  int PlayFrom(std::string_view host, std::string_view port);
};
}

#endif // LRM_PLAYER_H
