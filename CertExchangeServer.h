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

#ifndef LRM_CERTEXCHANGESERVER_H_
#define LRM_CERTEXCHANGESERVER_H_

#include <atomic>
#include <cstdint>
#include <list>
#include <thread>
#include <variant>

#include "cert_messages.pb.h"

#include "crypto/SpekeSession.h"
#include "crypto/certs/CertificateAuthority.h"

namespace lrm {
/// \brief Server for distributing certificates, authorized with password.
///
/// The authorization and authentication is based on the knowledge of common
/// password. The server receives certificate requests and sends back
/// CA's certificate and the newly generated one for the client.
///
/// It's necessary to call \ref Start() to start listening for connections
/// from clients.
template <typename Protocol>
class CertExchangeServer {
  static const crypto::BigNum SPEKE_SAFE_PRIME;

  using SpekeSession = crypto::SpekeSession<Protocol>;
  using CAptr = std::shared_ptr<crypto::certs::CertificateAuthority>;
 public:
  /// \param endpoint Endpoint to listen on for incoming connections.
  /// \param password Password for client authorization.
  /// \param CA \ref CertificateAuthority to use for distributing
  /// certificates.
  explicit CertExchangeServer(typename Protocol::endpoint endpoint,
                              std::string_view password,
                              CAptr CA);
  ~CertExchangeServer();

  /// \brief Asynchronously listen for connections.
  void Start();
  /// \brief Stop listening and close all active sessions.
  void Stop() noexcept;

 private:
  // This is not thread-safe, run with sessions_mtx_ locked.
  void accept();
  void maybe_clean_sessions();
  void handle_speke_message(const crypto::Bytes& message,
                            SpekeSession& session);

  std::atomic_bool running_ = false;
  std::string password_;

  asio::io_context context_;
  std::thread context_thread_;
  typename Protocol::endpoint endpoint_;
  typename Protocol::acceptor acceptor_;

  std::list<SpekeSession> sessions_;
  std::mutex sessions_mtx_;

  CAptr CA_;
  crypto::Bytes CA_hash_;
};
}

#endif  // LRM_CERTEXCHANGESERVER_H_
