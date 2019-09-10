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

#ifndef LRM_SPEKESESSION_H_
#define LRM_SPEKESESSION_H_

#include <queue>
#include <mutex>

#include <asio.hpp>

#include "crypto/SPEKE.h"

namespace lrm::crypto {

class SpekeSession {
  using tcp = asio::ip::tcp;

 public:
  SpekeSession(std::shared_ptr<asio::io_context> io_context)
      : context_(std::move(io_context)) {}

  ~SpekeSession();

  void Connect(std::string_view host, uint16_t port,
               std::string_view password, const BigNum& safe_prime);
  void Disconnect();

  /// Set a handler to handle incoming HMAC-signed messages.
  ///
  /// The HMAC signature is already confirmed when the handler is called.
  ///
  /// /param handler A function that will handle messages.
  void SetMessageHandler(std::function<void(Bytes&&)>&& handler);
  // void SendMessage(Bytes message);

 private:
  /// \brief Make an ID out of the public key and the timestamp.
  ///
  /// \return Newly generated id.
  std::string make_id() const;

  void start_reading();
  void handle_read(const asio::error_code& ec);
  void handle_message(Bytes&& message);

  tcp::iostream stream_;
  std::shared_ptr<asio::io_context> context_;

  std::unique_ptr<SPEKE> speke_;
  std::string id_;

  /// Mutex for both message_handler_ and message_queue_
  std::mutex message_handler_mtx_;
  std::function<void(Bytes&&)> message_handler_;

  std::queue<Bytes> message_queue_;
};
}

#endif  // LRM_SPEKESESSION_H_
