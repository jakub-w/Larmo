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

#include "CertExchangeServer.h"

#include <algorithm>

#include "spdlog/spdlog.h"

#include "cert_messages.pb.h"

#include "crypto/config.h"
#include "Util.h"

using namespace lrm::crypto::certs;

namespace lrm {
template class CertExchangeServer<asio::ip::tcp>;
template class CertExchangeServer<asio::local::stream_protocol>;

template <typename Protocol>
const crypto::BigNum
CertExchangeServer<Protocol>::SPEKE_SAFE_PRIME(crypto::LRM_SPEKE_SAFE_PRIME);

template <typename Protocol>
CertExchangeServer<Protocol>::CertExchangeServer(
    typename Protocol::endpoint endpoint,
    std::string_view password,
    CAptr CA)
    : password_{password},
      context_{},
      endpoint_{std::move(endpoint)},
      acceptor_{context_, endpoint_, true},
      CA_{std::move(CA)},
      CA_hash_{CA_->GetRootCertificate().GetHash()} {}

template <typename Protocol>
CertExchangeServer<Protocol>::~CertExchangeServer() {
  Stop();
}

template <typename Protocol>
void CertExchangeServer<Protocol>::Start() {
  if (not running_) running_ = true;
  else return;

  accept();
  context_thread_ = std::thread{[this]{ context_.run(); }};
}

template <typename Protocol>
void CertExchangeServer<Protocol>::Stop() noexcept {
  if (running_) running_ = false;
  else return;

  for(auto& session: sessions_) {
    session.Close(crypto::SpekeSessionState::STOPPED);
  }
  context_.stop();
  context_thread_.join();
}

template <typename Protocol>
void CertExchangeServer<Protocol>::accept() {
  acceptor_.async_accept(
      [this](const asio::error_code& ec, typename Protocol::socket peer) {
        if (not running_) return;

        if (ec) {
          spdlog::error("While accepting on cert server: {}", ec.message());
          return;
        }

        // TODO: This is probably not necessary since we call accept() later
        //       and not before.
        {
          std::lock_guard g{sessions_mtx_};
          sessions_.emplace_back(
              std::move(peer),
              std::make_unique<crypto::SPEKE>("server", password_,
                                              SPEKE_SAFE_PRIME))
              .Run([this](crypto::Bytes&& msg, SpekeSession& session){
                     handle_speke_message(std::move(msg), session);
                   });

          maybe_clean_sessions();
        }

        accept();
      });
}

template <typename Protocol>
void CertExchangeServer<Protocol>::maybe_clean_sessions() {
  if (sessions_.size() % 5 == 0) return;

  sessions_.remove_if(
      [](const SpekeSession& session){
        return session.GetState() >= crypto::SpekeSessionState::STOPPED;
      });
}

template <typename Protocol>
void CertExchangeServer<Protocol>::handle_speke_message(
    const crypto::Bytes& message, SpekeSession& session) {
  CertClientMessage msg;
  msg.ParseFromArray(message.data(), message.size());

  CertServerMessage out;

  if (msg.has_cert_request()) {
    // If the message couldn't be read properly or the certification couldn't
    // go through, the message with error_code 1 will be sent.

    const auto get_req =
        [&]{
          const auto bytes = Util::str_to_bytes(msg.cert_request().request());
          return CertificateRequest::FromDER(bytes);
        };

    try {
      const crypto::Bytes client_cert = CA_->Certify(get_req(), 365).ToDer();
      const crypto::Bytes root_cert_der = CA_->GetRootCertificate().ToDer();

      auto cert_bundle = out.mutable_cert_bundle();
      cert_bundle->set_root_cert(root_cert_der.data(), root_cert_der.size());
      cert_bundle->set_client_cert(client_cert.data(), client_cert.size());
    } catch (const std::runtime_error& e) {
      out.set_error_code(1);
    }
  } else if (msg.has_confirm_request()) {
    const auto hash = Util::str_to_bytes(msg.confirm_request().cert_hash());

    auto response = out.mutable_confirm_response();
    response->set_response(CA_hash_ == hash);
  } else {
    out.set_error_code(2);
    // spdlog::error("CertClientMessage type not implemented");
    // return;
  }

  crypto::Bytes out_bytes(out.ByteSizeLong());
  out.SerializeToArray(out_bytes.data(), out_bytes.size());

  try {
    session.SendMessage(out_bytes);
  } catch (const std::logic_error& e) {
    // Ignore it if the session closed.
    if (session.GetState() == crypto::SpekeSessionState::RUNNING) {
      spdlog::error("While trying to send CertServerMessage: {}", e.what());
    }
  } catch (const std::exception& e) {
    spdlog::error("While trying to send CertServerMessage: {}", e.what());
  }
}
}
