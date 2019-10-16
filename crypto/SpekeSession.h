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

#include "crypto/SPEKE.pb.h"

#include "crypto/SPEKE.h"

namespace lrm::crypto {
/// The states are arranged in a way that everything >= STOPPED means that
/// the SpekeSession was closed.
enum class SpekeSessionState {
  /// Active before a call to \ref SpekeSession::Run().
  IDLE,
  /// Active after the session was started with \ref SpekeSession::Run() and
  /// there were no problems.
  RUNNING,
  STOPPED,
  /// An error has occurred (i.e. network error). Should be logged.
  STOPPED_ERROR,
  /// Peer didn't pass the key confirmation challenge.
  STOPPED_KEY_CONFIRMATION_FAILED,
  /// Disconnected from peer because of too many wrong HMAC signatures or
  /// something else that seemed fishy. The limit on how many times peer can
  /// show bad behavior is set by \ref SpekeSession::BAD_BEHAVIOR_LIMIT.
  STOPPED_PEER_BAD_BEHAVIOR,
  /// The session was stopped because the peer disconnected.
  STOPPED_PEER_DISCONNECTED,
  /// Peer sent an invalid public key or an invalid id.
  STOPPED_PEER_PUBLIC_KEY_OR_ID_INVALID
};

/// \brief Network session authenticated by SPEKE.
///
/// First method to call is \ref Run() to establish a working session with
/// the peer. If everyting went alright the session state, obtained by calling
/// \ref GetState() should be \ref SpekeSessionState::RUNNING. In that case
/// HMAC-signed messages can be send with \ref SendMessage().
/// For the description of various failed states refer to
/// \ref SpekeSessionState.
///
/// The SpekeSession class uses asynchronous asio calls, so the context
/// tied to the socket that is given in the constructor needs to be running.
template <typename Protocol>
class SpekeSession {
 public:
  static constexpr int BAD_BEHAVIOR_LIMIT = 3;

  /// The \e Bytes param is a plain message in bytes, without HMAC signature.
  using MessageHandler = std::function<void(Bytes&&)>;

  /// \param socket An already connected tcp socket.
  /// \param speke A pointer to an already constructed \ref SpekeInterface
  /// object. I.e. \ref SPEKE object.
  SpekeSession(asio::basic_stream_socket<Protocol>&& socket,
               std::shared_ptr<SpekeInterface>&& speke);

  virtual ~SpekeSession();

  /// \brief Establish SPEKE session and start listening for incoming
  /// messages asynchronously.
  ///
  /// \param handler A function for handling messages. More info in
  /// \ref SetMessageHandler() and \ref MessageHandler documentation.
  virtual void Run(MessageHandler&& handler);

  /// \brief Close the session and severe the connection.
  ///
  /// \param state \ref SpekeSessionState to set while closing the connection.
  virtual void Close(SpekeSessionState state) noexcept;

  /// \brief Set a handler to handle incoming HMAC-signed messages.
  ///
  /// The HMAC signature is already confirmed when the handler is called.
  ///
  /// Example:
  /// \code{.cpp}
  /// speke_session.SetMessageHandler([](Bytes&& msg) {
  ///                                   // Handle msg...
  ///                                 }
  /// \endcode
  ///
  /// \param handler A function that will handle messages.
  virtual void SetMessageHandler(MessageHandler&& handler);

  /// \brief Get the session state
  virtual SpekeSessionState GetState() const;

  /// \brief Send a \e message to peer. The message will be signed with HMAC.
  virtual void SendMessage(const Bytes& message);

 protected:
  // This is synchronous
  static SpekeMessage ReceiveMessage(
      asio::basic_stream_socket<Protocol>& socket);

  // This is asynchronous
  static void SendMessage(
      const SpekeMessage& message,
      asio::basic_stream_socket<Protocol>& socket);

 private:
  void start_reading();
  void handle_read(const asio::error_code& ec);
  void handle_message(Bytes&& message);
  void send_key_confirmation();

  void send_message(const SpekeMessage& message);
  std::optional<SpekeMessage> receive_message();

  void increase_bad_behavior_count();

  asio::basic_stream_socket<Protocol> socket_;

  std::shared_ptr<SpekeInterface> speke_;

  std::atomic<SpekeSessionState> state_;

  int bad_behavior_count_ = 0;
  std::atomic_bool closed_ = false;

  std::mutex message_handler_mtx_;
  MessageHandler message_handler_;
};
}

#endif  // LRM_SPEKESESSION_H_
