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
SpekeSession::~SpekeSession() {
  Disconnect();
}

/// \param host Hostname or IPv4 of the remote party.
/// \param port Port on which to connect.
void SpekeSession::Connect(std::string_view host, uint16_t port,
                           std::string_view password,
                           const BigNum& safe_prime) {
  Disconnect();
  // Connect to the remote party on host:port
  auto results = tcp::resolver(*context_).resolve(
      host.data(), std::to_string(port));

  tcp::endpoint ep;
  for (auto& result : results) {
    if (result.endpoint().protocol() == tcp::v4()) {
      ep = result.endpoint();
      break;
    }
  }

  // TODO: handle unresolved host
  stream_.connect(ep);

  speke_ = std::make_unique<SPEKE>(id_, password, safe_prime);

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();

  id_ = make_id();
  init_data->set_id(id_);

  auto pubkey = speke_->GetPublicKey();
  init_data->set_public_key(pubkey.data(), pubkey.size());

  start_reading();

  message.SerializeToOstream(&stream_);
}

// TODO: Maybe add an argument to notify the peer why are we disconnecting
void SpekeSession::Disconnect() {
  if (stream_.socket().is_open()) {
    stream_.socket().shutdown(tcp::socket::shutdown_both);
  }
  stream_.close();

  speke_.release();
  id_.clear();
}

std::string SpekeSession::make_id() const {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();

  EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);

  Bytes pkey = speke_->GetPublicKey();
  EVP_DigestUpdate(ctx, pkey.data(), pkey.size());

  auto timestamp = std::chrono::high_resolution_clock::now()
                   .time_since_epoch().count();
  EVP_DigestUpdate(ctx, &timestamp, sizeof(timestamp));

  unsigned char md_val[MD5_DIGEST_LENGTH];
  unsigned int md_len;
  EVP_DigestFinal_ex(ctx, md_val, &md_len);

  EVP_MD_CTX_free(ctx);

  char buffer[MD5_DIGEST_LENGTH * 2];
  for (unsigned int i = 0; i < md_len; ++i) {
    std::sprintf(&buffer[2*i], "%02X", md_val[i]);
  }

  return std::string(buffer);
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
  std::function<void(Bytes&&)> handler;
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

void SpekeSession::SetMessageHandler(
    std::function<void(Bytes&&)>&& handler) {
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
