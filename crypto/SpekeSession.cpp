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
    std::shared_ptr<SpekeInterface>&& speke)
    : socket_(std::move(socket)),
      speke_(std::move(speke)),
      state_(SpekeSessionState::IDLE){
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
  Close(SpekeSessionState::STOPPED);
}

template <typename Protocol>
void SpekeSession<Protocol>::Run(MessageHandler&& handler) {
  if (state_ != SpekeSessionState::IDLE) {
    throw std::logic_error(
        __PRETTY_FUNCTION__ +
        std::string(": You can only start a session in IDLE state"));
  }

  SetMessageHandler(std::move(handler));

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();

  init_data->set_id(speke_->GetId());

  auto pubkey = speke_->GetPublicKey();
  init_data->set_public_key(pubkey.data(), pubkey.size());

  start_reading();

  send_message(message);

  state_ = SpekeSessionState::RUNNING;
}

template <typename Protocol>
void SpekeSession<Protocol>::Close(SpekeSessionState state) noexcept {
  if (not closed_) {
    closed_ = true;
  } else {
    return;
  }

  asio::error_code ec;

  if (socket_.is_open()) {
    socket_.shutdown(asio::socket_base::shutdown_both, ec);
  }
  socket_.close(ec);

  speke_.reset();

  state_ = state;
}

template <typename Protocol>
void SpekeSession<Protocol>::start_reading() {
  socket_.async_wait(asio::socket_base::wait_read,
                     [this](const asio::error_code& ec) {
                       handle_read(std::forward<const asio::error_code>(ec));
                     });
}

template <typename Protocol>
void SpekeSession<Protocol>::handle_read(const asio::error_code& ec) {
  if (ec) {
    // TODO: log it
    Close(SpekeSessionState::STOPPED_ERROR);
    return;
  }

  std::optional<SpekeMessage> message = receive_message();

  if (not message) {
    Close(SpekeSessionState::STOPPED_ERROR);
    return;
  }

  if (message->has_signed_data()) {
    Bytes hmac{message->signed_data().hmac_signature().begin(),
               message->signed_data().hmac_signature().end()};
    Bytes data{message->signed_data().data().begin(),
               message->signed_data().data().end()};

    if (speke_->ConfirmHmacSignature(hmac, data)) {
      handle_message(std::move(data));
    } else {
      // Bad HMAC signature
      increase_bad_behavior_count();
    }
  } else if (message->has_init_data()) {
    std::string id = message->init_data().id();
    Bytes pubkey{message->init_data().public_key().begin(),
                 message->init_data().public_key().end()};

    try {
      speke_->ProvideRemotePublicKeyIdPair(pubkey, id);
    } catch (const std::logic_error& e) {
      // This error will occur if the pubkey and id were already provided.
      // TODO: Log it
      // increase_bad_behavior_count();
    } catch (const std::runtime_error& e) {
      // This will occur if the peer's id or public key is invalid.
      // TODO: Log it
      Close(SpekeSessionState::STOPPED_PEER_PUBLIC_KEY_OR_ID_INVALID);
      return;
    }

    if (not kcd_sent_) {
      send_key_confirmation();
    }
  } else if (message->has_key_confirmation()) {
    Bytes kcd{message->key_confirmation().data().begin(),
              message->key_confirmation().data().end()};

    if (not speke_->ConfirmKey(kcd)) {
      // TODO: Log it
      Close(SpekeSessionState::STOPPED_KEY_CONFIRMATION_FAILED);
      return;
    }
  }

  // This is at the bottom because we want to read messages sequentially
  // since SPEKE and this class are not entirely thread-safe.
  start_reading();
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
  kcd_sent_ = true;

  auto kcd = speke_->GetKeyConfirmationData();

  SpekeMessage kcd_message;
  SpekeMessage::KeyConfirmation* kcd_p =
      kcd_message.mutable_key_confirmation();

  kcd_p->set_data(kcd.data(), kcd.size());

  send_message(kcd_message);
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
SpekeSessionState SpekeSession<Protocol>::GetState() const {
  return state_;
}

template <typename Protocol>
void SpekeSession<Protocol>::SendMessage(const Bytes &message) {
  if (SpekeSessionState::RUNNING != state_) {
    throw std::logic_error(
        __PRETTY_FUNCTION__ +
        std::string(": You can only send a message in RUNNING state"));
  }

  Bytes hmac = speke_->HmacSign(std::forward<const Bytes&>(message));

  SpekeMessage msg;
  SpekeMessage::SignedData* sd = msg.mutable_signed_data();
  sd->set_hmac_signature(hmac.data(), hmac.size());
  sd->set_data(message.data(), message.size());

  send_message(msg);
}

template <typename Protocol>
void SpekeSession<Protocol>::send_message(SpekeMessage& message) {
  try {
    SendMessage(std::forward<SpekeMessage>(message), socket_);
  } catch (const asio::system_error& e) {
    switch (e.code().value()) {
      case asio::error::eof:
      case asio::error::bad_descriptor:
      case asio::error::broken_pipe:
        // TODO: Log it
        Close(SpekeSessionState::STOPPED_PEER_DISCONNECTED);
        return;
      default:
        // TODO: Log it
        Close(SpekeSessionState::STOPPED_ERROR);
    }
  }
}

template <typename Protocol>
std::optional<SpekeMessage> SpekeSession<Protocol>::receive_message() {
  std::optional<SpekeMessage> message;

  try {
    message = ReceiveMessage(socket_);
  } catch (const asio::system_error& e) {
    switch (e.code().value()) {
      case asio::error::eof:
      case asio::error::bad_descriptor:
      case asio::error::broken_pipe:
        // TODO: Log it
        Close(SpekeSessionState::STOPPED_PEER_DISCONNECTED);
        break;
      default:
        // TODO: Log it
        Close(SpekeSessionState::STOPPED_ERROR);
    }
  }

  return message;
}

template <typename Protocol>
void SpekeSession<Protocol>::increase_bad_behavior_count() {
  if (++bad_behavior_count_ >= BAD_BEHAVIOR_LIMIT) {
    Close(SpekeSessionState::STOPPED_PEER_BAD_BEHAVIOR);
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

  // May throw
  asio::read(socket, asio::buffer(&size, sizeof(size)),
             asio::transfer_exactly(sizeof(size)));

  std::vector<std::byte> message_arr(size);
  // May throw
  asio::read(socket, asio::buffer(message_arr.data(), size),
             asio::transfer_exactly(size));

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

  const size_t size = message.ByteSizeLong();

  std::vector<std::byte> buffer(sizeof(size) + size);
  std::memcpy(buffer.data(), &size, sizeof(size));
  message.SerializeToArray(buffer.data() + sizeof(size), size);

  asio::async_write(socket, asio::buffer(buffer),
                    asio::transfer_exactly(buffer.size()));
}

template class SpekeSession<asio::ip::tcp>;
template class SpekeSession<asio::local::stream_protocol>;
}
