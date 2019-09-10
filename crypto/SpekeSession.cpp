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

#include "crypto/SpekeSession.h"

#include "crypto/SPEKE.pb.h"
#include "openssl/md5.h"

namespace lrm::crypto {
SpekeSession::SpekeSession(tcp::socket&& socket,
                           std::string_view id,
                           std::string_view password,
                           const BigNum& safe_prime)
    : stream_{std::move(socket)} {
  if (not stream_.socket().is_open()) {
    throw std::logic_error(
        __PRETTY_FUNCTION__ +
        std::string(": 'socket' must be already connected"));
  }

  speke_ = std::make_unique<SPEKE>(std::forward<std::string_view>(id),
                                   std::forward<std::string_view>(password),
                                   std::forward<const BigNum&>(safe_prime));
}

SpekeSession::~SpekeSession() {
  Close();
}

void SpekeSession::Run(MessageHandler&& handler) {
  SetMessageHandler(std::move(handler));

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();

  init_data->set_id(speke_->GetId());

  auto pubkey = speke_->GetPublicKey();
  init_data->set_public_key(pubkey.data(), pubkey.size());

  start_reading();

  message.SerializeToOstream(&stream_);
}

// TODO: Maybe add an argument to notify the peer why are we disconnecting
void SpekeSession::Close() {
  if (stream_.socket().is_open()) {
    stream_.socket().shutdown(tcp::socket::shutdown_both);
  }
  stream_.close();

  speke_.release();
}

void SpekeSession::start_reading() {
  stream_.socket().async_wait(tcp::socket::wait_read,
                              std::bind(&SpekeSession::handle_read, this,
                                        std::placeholders::_1));
}

void SpekeSession::handle_read(const asio::error_code& ec) {
  if (ec) {
    // TODO: log it
    return;
  }

  SpekeMessage message;

  message.ParseFromIstream(&stream_);

  start_reading();

  if (message.has_signed_data()) {
    Bytes hmac{message.signed_data().hmac_signature().begin(),
               message.signed_data().hmac_signature().end()};
    Bytes data{message.signed_data().data().begin(),
               message.signed_data().data().end()};

    speke_->ConfirmHmacSignature(hmac, data);

    handle_message(std::move(data));
  } else if (message.has_init_data()) {
    std::string id = message.init_data().id();
    Bytes pubkey{message.init_data().public_key().begin(),
                 message.init_data().public_key().end()};

    speke_->ProvideRemotePublicKeyIdPair(pubkey, id);
  } else if (message.has_key_confirmation()) {
    Bytes kcd{message.key_confirmation().data().begin(),
              message.key_confirmation().data().end()};

    speke_->ConfirmKey(kcd);
  }
}

void SpekeSession::handle_message(Bytes&& message) {
  // Add The message to a queue if the handler is not set. Otherwise
  // just call the handler.
  MessageHandler handler;
  {
    std::lock_guard lck{message_handler_mtx_};

    if (message_handler_) {
      handler = message_handler_;
    } else {
      message_queue_.push(std::move(message));
      return;
    }
  }

  if (handler) {
    handler(std::move(message));
  }
}

void SpekeSession::SetMessageHandler(MessageHandler&& handler) {
  {
    std::lock_guard lck{message_handler_mtx_};
    message_handler_ = std::move(handler);
  }

  Bytes message;
  // this shouldn't need a lock because there should be no way the queue is
  // modified while the handler is set
  while (not message_queue_.empty()) {
    handle_message(std::move(message_queue_.front()));
    message_queue_.pop();
  }
}
}
