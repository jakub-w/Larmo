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
template <typename Protocol>
SpekeSession<Protocol>::SpekeSession(
    asio::basic_stream_socket<Protocol>&& socket,
    std::unique_ptr<SpekeInterface>&& speke)
    : socket_(std::move(socket)),
      speke_(std::move(speke)) {
  if (not socket_.is_open()) {
    throw std::invalid_argument(
        __PRETTY_FUNCTION__ +
        std::string(": 'socket' must be already connected"));
  }
  if (not speke_) {
    throw std::invalid_argument(
        __PRETTY_FUNCTION__ +
        std::string(": 'speke' must be already instantiated"));
  }
}

template <typename Protocol>
SpekeSession<Protocol>::~SpekeSession() {
  Close();
}

template <typename Protocol>
void SpekeSession<Protocol>::Run(MessageHandler&& handler) {
  SetMessageHandler(std::move(handler));

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();

  init_data->set_id(speke_->GetId());

  auto pubkey = speke_->GetPublicKey();
  init_data->set_public_key(pubkey.data(), pubkey.size());

  start_reading();

  SendMessage(message, socket_);
}

// TODO: Maybe add an argument to notify the peer why are we disconnecting
template <typename Protocol>
void SpekeSession<Protocol>::Close() {
  if (socket_.is_open()) {
    socket_.shutdown(tcp::socket::shutdown_both);
  }
  socket_.close();

  speke_.release();
}

template <typename Protocol>
void SpekeSession<Protocol>::start_reading() {
  socket_.async_wait(tcp::socket::wait_read,
                     std::bind(&SpekeSession::handle_read, this,
                               std::placeholders::_1));
}

template <typename Protocol>
void SpekeSession<Protocol>::handle_read(const asio::error_code& ec) {
  if (ec) {
    // TODO: log it
    return;
  }

  SpekeMessage message = ReceiveMessage(socket_);

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

    // Send the key confirmation message
    send_key_confirmation();
  } else if (message.has_key_confirmation()) {
    Bytes kcd{message.key_confirmation().data().begin(),
              message.key_confirmation().data().end()};

    speke_->ConfirmKey(kcd);
  }
}

template <typename Protocol>
void SpekeSession<Protocol>::handle_message(Bytes&& message) {
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

template <typename Protocol>
void SpekeSession<Protocol>::send_key_confirmation() {
  auto kcd = speke_->GetKeyConfirmationData();

  SpekeMessage kcd_message;
  SpekeMessage::KeyConfirmation* kcd_p =
      kcd_message.mutable_key_confirmation();

  kcd_p->set_data(kcd.data(), kcd.size());

  SendMessage(kcd_message, socket_);
}

template <typename Protocol>
void SpekeSession<Protocol>::SetMessageHandler(MessageHandler&& handler) {
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

template <typename Protocol>
SpekeMessage SpekeSession<Protocol>::ReceiveMessage(
    asio::basic_stream_socket<Protocol>& socket) {
  if (not socket.is_open()) {
    throw std::invalid_argument(
        __PRETTY_FUNCTION__ +
        std::string(": 'socket' must be connected"));
  }

  SpekeMessage message;
  size_t size = 0;

  asio::read(socket, asio::buffer(&size, sizeof(size))); // May throw

  std::vector<std::byte> message_arr(size);
  asio::read(socket, asio::buffer(message_arr.data(), size)); // May throw

  message.ParseFromArray(message_arr.data(), size);

  return message;
}

template <typename Protocol>
void SpekeSession<Protocol>::SendMessage(
    const SpekeMessage& message,
    asio::basic_stream_socket<Protocol>& socket) {
  if (not socket.is_open()) {
    throw std::invalid_argument(
        __PRETTY_FUNCTION__ +
        std::string(": 'socket' must be connected"));
  }

  size_t size = message.ByteSizeLong();
  asio::error_code ec;

  asio::write(socket, asio::buffer(&size, sizeof(size))); // May throw

  message.SerializeToFileDescriptor(socket.native_handle());
}

template class SpekeSession<asio::ip::tcp>;
template class SpekeSession<asio::local::stream_protocol>;
}
