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

#include <gtest/gtest.h>

#include "crypto/SPEKE.pb.h"

#include "crypto/SPEKE.h"
#include "crypto/SpekeSession.h"

#include <asio.hpp>

using namespace lrm::crypto;
using tcp = asio::ip::tcp;
using stream_protocol = asio::local::stream_protocol;

class FakeSpeke : public SpekeInterface {
 private:
  Bytes pkey_{'p', 'k', 'e', 'y'};
  std::string id_{"id"};
  Bytes enc_key_{'e', 'n', 'c', 'k', 'e', 'y'};
  Bytes kcd_{'k', 'c', 'd'};

  Bytes bad_bytes_{'b', 'a', 'd'};
  std::string bad_str_{"bad"};

 public:
  virtual ~FakeSpeke() {};

  virtual Bytes GetPublicKey() const override {
    return pkey_;
  }

  virtual const std::string& GetId() const override {
    return id_;
  }

  virtual void ProvideRemotePublicKeyIdPair(
      const Bytes& remote_pubkey,
      const std::string& remote_id) override {
    if (remote_pubkey == bad_bytes_) {
      throw std::runtime_error("Bad pubkey");
    }
    if (remote_id == bad_str_) {
      throw std::runtime_error("Bad id");
    }
  }

  virtual const Bytes& GetEncryptionKey() override {
    return enc_key_;
  }

  virtual const Bytes& GetKeyConfirmationData() override {
    return kcd_;
  }

  virtual bool ConfirmKey(const Bytes& remote_kcd) override {
    return remote_kcd != bad_bytes_;
  }

  virtual Bytes HmacSign(const Bytes& message) override {
    return message;
  }

  virtual bool ConfirmHmacSignature(
      const Bytes& hmac_signature,
      const Bytes& message) override {
    return hmac_signature != bad_bytes_;
  }
};

class SpekeSessionTestF : public ::testing::Test {
  asio::io_context context;

  stream_protocol::socket priv_socket, peer_socket;

  std::thread context_thread;
  asio::error_code ec;

  std::shared_ptr<SpekeSession<stream_protocol>> session;

 protected:
  stream_protocol::socket& GetSocket() {
    return peer_socket;
  }

  std::shared_ptr<SpekeSession<stream_protocol>> GetSession() {
    return session;
  }

 public:
  SpekeSessionTestF()
      : ::testing::Test(), priv_socket(context), peer_socket(context) {
    asio::local::connect_pair(priv_socket, peer_socket);

    session = std::make_unique<SpekeSession<stream_protocol>>(
        std::move(priv_socket), std::make_unique<FakeSpeke>());

    context_thread = std::thread(
        [this](){
          auto context_guard = asio::make_work_guard(context);
          context.run();
        });
  }

  virtual ~SpekeSessionTestF() {
    peer_socket.shutdown(asio::socket_base::shutdown_both);
    peer_socket.close();

    context.stop();
    context_thread.join();
  };
};

asio::io_context context_glob;

bool is_connection_alive(stream_protocol::socket& socket) {
  if (not socket.is_open()) return false;

  asio::error_code ec;
  socket.write_some(asio::buffer(""), ec);
  if (ec) return false;

  return true;
}

std::pair<stream_protocol::socket, stream_protocol::socket>
get_local_socketpair(asio::io_context& ctx) {
  int fds[2];
  socketpair(AF_LOCAL, SOCK_STREAM, 0, fds);

  return std::pair(
      stream_protocol::socket(ctx, stream_protocol(), fds[0]),
      stream_protocol::socket(ctx, stream_protocol(), fds[1]));
}

TEST(SpekeSessionTest, Construct_ThrowSocketNotConnectedSpekeGood) {
  stream_protocol::socket socket(context_glob);
  auto speke = std::make_unique<SPEKE>("id", "password", 7);

  EXPECT_THROW(SpekeSession(std::move(socket), std::move(speke)),
               std::invalid_argument)
      << "Should throw when the given socket is not connected to anything";
}

TEST(SpekeSessionTest, Construct_ThrowSocketConnectedSpekeNullptr) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::unique_ptr<FakeSpeke>(nullptr);

  EXPECT_THROW(SpekeSession(std::move(sockets.first), std::move(speke)),
               std::invalid_argument)
      << "Should throw when speke is nullptr";
}

TEST(SpekeSessionTest, Construct_NoThrowSocketConnectedSpekeGood) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::make_unique<FakeSpeke>();

  EXPECT_NO_THROW(SpekeSession(std::move(sockets.first), std::move(speke)))
      << "Should not throw when the given socket is connected to another and "
      "speke is not nullptr";
}

TEST(SpekeSessionTest, Run_InitDataIsSent) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::make_unique<FakeSpeke>();
  auto session = SpekeSession(std::move(sockets.first), std::move(speke));

  session.Run([](Bytes&& message){ return; });

  SpekeMessage peer_data =
      SpekeSession<stream_protocol>::ReceiveMessage(sockets.second);

  ASSERT_TRUE(peer_data.has_init_data());
  EXPECT_EQ("id", peer_data.init_data().id());
  EXPECT_EQ("pkey", peer_data.init_data().public_key());
}

TEST(SpekeSessionTest, ConnectionDroppedOnIncorrectPublicKey) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::make_unique<FakeSpeke>();
  auto session = SpekeSession(std::move(sockets.first), std::move(speke));

  session.Run([](Bytes&& message){ return; });

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();
  init_data->set_id("bad");
  init_data->set_public_key("pkey");

  SpekeSession<stream_protocol>::SendMessage(message, sockets.second);

  context_glob.run_for(std::chrono::milliseconds(200));

  EXPECT_FALSE(is_connection_alive(sockets.second))
      << "Socket should be closed after the server closed the connection";
}

TEST_F(SpekeSessionTestF, ConnectionNotDroppedOnCorrectPublicKey) {
  auto session = GetSession();
  session->Run([](Bytes&& message){ return; });

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();
  init_data->set_id("id");
  init_data->set_public_key("pkey");

  SpekeSession<stream_protocol>::SendMessage(message, GetSocket());

  EXPECT_TRUE(is_connection_alive(GetSocket()))
      << "Socket should be closed after the server closed the connection";
}

TEST_F(SpekeSessionTestF, SendsKeyConfirmation) {
  auto session = GetSession();
  session->Run([](Bytes&& message){ return; });

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();
  init_data->set_id("id");
  init_data->set_public_key("pkey");

  auto& socket = GetSocket();

  SpekeSession<stream_protocol>::SendMessage(message, socket);

  // Receive init data
  message = SpekeSession<stream_protocol>::ReceiveMessage(socket);

  // Receive the next message, which should be key confirmation data
  message = SpekeSession<stream_protocol>::ReceiveMessage(socket);

  EXPECT_TRUE(is_connection_alive(socket));
  ASSERT_TRUE(message.has_key_confirmation());
  EXPECT_EQ("kcd", message.key_confirmation().data());
}
