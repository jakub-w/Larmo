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

#include "Util.h"

using namespace lrm::crypto;
using tcp = asio::ip::tcp;
using stream_protocol = asio::local::stream_protocol;

namespace {
class TestSpekeSession : public SpekeSession<stream_protocol> {
 public:
  TestSpekeSession(asio::basic_stream_socket<stream_protocol>&& socket,
                   std::shared_ptr<SpekeInterface>&& speke)
      : SpekeSession(std::move(socket), std::move(speke)) {}
  virtual ~TestSpekeSession(){}

  static void TestSendMessage(
      const SpekeMessage& message,
      asio::basic_stream_socket<stream_protocol>& socket) {
    SpekeSession<stream_protocol>::SendMessage(
        std::forward<const SpekeMessage&>(message),
        std::forward<asio::basic_stream_socket<stream_protocol>&>(socket));
  }
  static SpekeMessage TestReceiveMessage(
      asio::basic_stream_socket<stream_protocol>& socket) {
    return SpekeSession<stream_protocol>::ReceiveMessage(
        std::forward<asio::basic_stream_socket<stream_protocol>&>(socket));
  }
};

class FakeSpeke : public SpekeInterface {
 private:
  Bytes pkey_ = lrm::Util::str_to_bytes("pkey");
  std::string id_{"id"};
  Bytes enc_key_ = lrm::Util::str_to_bytes("enckey");
  Bytes kcd_ = lrm::Util::str_to_bytes("kcd");

  Bytes bad_bytes_ = lrm::Util::str_to_bytes("bad");
  std::string bad_str_{"bad"};

  bool init_data_already_sent_ = false;

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
    if (init_data_already_sent_) {
      throw std::logic_error("Init data already here");
    }
    init_data_already_sent_ = true;
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
    return lrm::Util::str_to_bytes("hmac");
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

  std::shared_ptr<TestSpekeSession> session;

 protected:
  stream_protocol::socket& GetSocket() {
    return peer_socket;
  }

  std::shared_ptr<TestSpekeSession> GetSession() {
    return session;
  }

  void SendInitData() {
    SpekeMessage message;
    SpekeMessage::InitData* init_data = message.mutable_init_data();
    init_data->set_id("id");
    init_data->set_public_key("pkey");
    TestSpekeSession::TestSendMessage(message, peer_socket);
  }

 public:
  SpekeSessionTestF()
      : ::testing::Test(), priv_socket(context), peer_socket(context) {
    asio::local::connect_pair(priv_socket, peer_socket);

    session = std::make_unique<TestSpekeSession>(
        std::move(priv_socket), std::make_unique<FakeSpeke>());

    context_thread = std::thread(
        [this](){
          auto context_guard = asio::make_work_guard(context);
          try {
            context.run();
          } catch (const asio::system_error& e) {}
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

std::pair<stream_protocol::socket, stream_protocol::socket>
get_local_socketpair(asio::io_context& ctx) {
  int fds[2];
  socketpair(AF_LOCAL, SOCK_STREAM, 0, fds);

  return std::pair(
      stream_protocol::socket(ctx, stream_protocol(), fds[0]),
      stream_protocol::socket(ctx, stream_protocol(), fds[1]));
}

template<typename Predicate, class Rep, class Period>
bool wait_predicate(Predicate&& pred,
                    const std::chrono::duration<Rep, Period>& timeout){
  std::chrono::time_point wake_time =
      std::chrono::high_resolution_clock::now() + timeout;

  while(not std::invoke(pred)) {
    auto wait_time = wake_time - std::chrono::high_resolution_clock::now();
    if (wait_time.count() < 0) return false;
    if (wait_time > std::chrono::milliseconds(1)) {
      wait_time = std::chrono::milliseconds(1);
    }
    std::this_thread::sleep_for(wait_time);
  }
  return true;
}
}

TEST(SpekeSessionTest, Construct_ThrowSocketNotConnectedSpekeGood) {
  stream_protocol::socket socket(context_glob);
  auto speke = std::make_unique<SPEKE>("id", "password", 7);

  EXPECT_THROW(TestSpekeSession(std::move(socket), std::move(speke)),
               std::invalid_argument)
      << "Should throw when the given socket is not connected to anything";
}

TEST(SpekeSessionTest, Construct_ThrowSocketConnectedSpekeNullptr) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::unique_ptr<FakeSpeke>(nullptr);

  EXPECT_THROW(TestSpekeSession(std::move(sockets.first), std::move(speke)),
               std::invalid_argument)
      << "Should throw when speke is nullptr";
}

TEST(SpekeSessionTest, Construct_NoThrowSocketConnectedSpekeGood) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::make_unique<FakeSpeke>();

  EXPECT_NO_THROW(TestSpekeSession(std::move(sockets.first),
                                   std::move(speke)))
      << "Should not throw when the given socket is connected to another and "
      "speke is not nullptr";
}

TEST(SpekeSessionTest, Construct_InIdleState) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::make_unique<FakeSpeke>();

  auto session = TestSpekeSession(std::move(sockets.first), std::move(speke));

  EXPECT_EQ(SpekeSessionState::IDLE, session.GetState());
}

TEST(SpekeSessionTest, Run_InitDataIsSent) {
  auto sockets = get_local_socketpair(context_glob);
  auto speke = std::make_unique<FakeSpeke>();
  auto session = TestSpekeSession(std::move(sockets.first), std::move(speke));

  ASSERT_NO_THROW(session.Run([](auto, auto&){}));

  SpekeMessage peer_data =
      TestSpekeSession::TestReceiveMessage(sockets.second);

  ASSERT_TRUE(peer_data.has_init_data());
  EXPECT_EQ("id", peer_data.init_data().id());
  EXPECT_EQ("pkey", peer_data.init_data().public_key());
}

TEST_F(SpekeSessionTestF, ConnectionDroppedOnIncorrectPublicKey) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();
  init_data->set_id("bad");
  init_data->set_public_key("pkey");

  TestSpekeSession::TestSendMessage(message, GetSocket());

  wait_predicate(
      [&session]{
        return SpekeSessionState::RUNNING != session->GetState(); },
      std::chrono::milliseconds(2));

  EXPECT_EQ(SpekeSessionState::STOPPED_PEER_PUBLIC_KEY_OR_ID_INVALID,
            session->GetState())
      << "Socket should be closed after the server closed the connection";
}

TEST_F(SpekeSessionTestF, ConnectionNotDroppedOnCorrectPublicKey) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SpekeMessage message;
  SpekeMessage::InitData* init_data = message.mutable_init_data();
  init_data->set_id("id");
  init_data->set_public_key("pkey");

  TestSpekeSession::TestSendMessage(message, GetSocket());

  wait_predicate(
      [&session]{
        return SpekeSessionState::RUNNING != session->GetState(); },
      std::chrono::milliseconds(2));

  EXPECT_EQ(SpekeSessionState::RUNNING,
            session->GetState())
      << "Connection should be alive when the correct public key was sent";
}

TEST_F(SpekeSessionTestF, SendsKeyConfirmation) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SendInitData();

  auto& socket = GetSocket();

  // Receive init data
  SpekeMessage message =
      TestSpekeSession::TestReceiveMessage(socket);

  // Receive the next message, which should be key confirmation data
  message = TestSpekeSession::TestReceiveMessage(socket);

  EXPECT_EQ(SpekeSessionState::RUNNING, session->GetState());
  ASSERT_TRUE(message.has_key_confirmation());
  EXPECT_EQ("kcd", message.key_confirmation().data());
}

TEST_F(SpekeSessionTestF, ConnectionDroppedOnBadKeyConfirmation) {
  auto session = GetSession();
  session->Run([](auto, auto&){});
  auto& socket = GetSocket();

  SendInitData();

  SpekeMessage message;
  SpekeMessage::KeyConfirmation* kcd = message.mutable_key_confirmation();
  kcd->set_data("bad");

  ASSERT_EQ(SpekeSessionState::RUNNING, session->GetState())
      << "The connection was severed before key confirmation was sent";

  TestSpekeSession::TestSendMessage(message, socket);

  wait_predicate(
      [&session]{
        return SpekeSessionState::STOPPED_KEY_CONFIRMATION_FAILED ==
            session->GetState(); },
      std::chrono::milliseconds(10));

  EXPECT_EQ(SpekeSessionState::STOPPED_KEY_CONFIRMATION_FAILED,
            session->GetState())
      << "The connection should sever after sending wrong key confirmation";
}

TEST_F(SpekeSessionTestF, ConnectionNotDroppedOnCorrectKeyConfirmation) {
  auto session = GetSession();
  session->Run([](auto, auto&){});
  auto& socket = GetSocket();

  SendInitData();

  SpekeMessage message;
  SpekeMessage::KeyConfirmation* kcd = message.mutable_key_confirmation();
  kcd->set_data("kcd");

  ASSERT_EQ(SpekeSessionState::RUNNING, session->GetState())
      << "The connection was severed before key confirmation was sent";

  TestSpekeSession::TestSendMessage(message, socket);

  wait_predicate(
      [&session]{
        return SpekeSessionState::RUNNING != session->GetState(); },
      std::chrono::milliseconds(2));

  EXPECT_EQ(SpekeSessionState::RUNNING, session->GetState())
      << "Key confirmation succeeded but the connection was severed";
}

TEST_F(SpekeSessionTestF, ConnectionNotDroppedBadHmac) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SendInitData();

  SpekeMessage message;
  SpekeMessage::SignedData* sd = message.mutable_signed_data();
  sd->set_hmac_signature("bad");
  sd->set_data("test");

  TestSpekeSession::TestSendMessage(message, GetSocket());

  wait_predicate(
      [&session]{
        return SpekeSessionState::RUNNING != session->GetState(); },
      std::chrono::milliseconds(2));

  EXPECT_EQ(SpekeSessionState::RUNNING, session->GetState())
      << "The connection shouldn't be severed on one bad HMAC";
}

TEST_F(SpekeSessionTestF, ConnectionDroppedMultipleBadHMACs) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SendInitData();

  SpekeMessage message;
  SpekeMessage::SignedData* sd = message.mutable_signed_data();
  sd->set_hmac_signature("bad");
  sd->set_data("test");

  for(int i = 0; i < TestSpekeSession::BAD_BEHAVIOR_LIMIT; ++i) {
    TestSpekeSession::TestSendMessage(message, GetSocket());
  }

  wait_predicate(
      [&session]{
        return SpekeSessionState::RUNNING != session->GetState(); },
      std::chrono::milliseconds(2));

  EXPECT_EQ(SpekeSessionState::STOPPED_PEER_BAD_BEHAVIOR, session->GetState())
      << "The connection should be severed on receiving bad HMACs in the "
      "number exceeding SpekeSession::BAD_BEHAVIOR_LIMIT";
}

TEST_F(SpekeSessionTestF, ConnectionNotDroppedMultipleGoodHMACs) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SendInitData();

  SpekeMessage message;
  SpekeMessage::SignedData* sd = message.mutable_signed_data();
  sd->set_hmac_signature("hmac");
  sd->set_data("test");

  for(int i = 0; i < TestSpekeSession::BAD_BEHAVIOR_LIMIT; ++i) {
    TestSpekeSession::TestSendMessage(message, GetSocket());
  }

  wait_predicate(
      [&session]{
        return SpekeSessionState::RUNNING != session->GetState(); },
      std::chrono::milliseconds(2));

  EXPECT_EQ(SpekeSessionState::RUNNING, session->GetState());
}

TEST_F(SpekeSessionTestF, MessageHandlerCalledOnHMACmessage) {
  auto session = GetSession();
  std::string result;
  session->Run(
      [&result](Bytes&& message, auto&){
        result.resize(message.size());
        lrm::Util::safe_memcpy(result.data(), message.data(), message.size());
      });

  SendInitData();

  SpekeMessage message;
  SpekeMessage::SignedData* sd = message.mutable_signed_data();
  sd->set_hmac_signature("hmac");
  sd->set_data("test");
  TestSpekeSession::TestSendMessage(message, GetSocket());

  wait_predicate([&result]{return result == "test";},
                 std::chrono::milliseconds(3));

  EXPECT_EQ(result, "test");
}

TEST_F(SpekeSessionTestF, SetMessageHandler) {
  auto session = GetSession();
  session->Run([](auto, auto&){});

  SendInitData();

  std::string result;
  session->SetMessageHandler(
      [&result](Bytes&& message, auto&){
        result.resize(message.size());
        lrm::Util::safe_memcpy(result.data(), message.data(), message.size());
      });

  SpekeMessage message;
  SpekeMessage::SignedData* sd = message.mutable_signed_data();
  sd->set_hmac_signature("hmac");
  sd->set_data("test");
  TestSpekeSession::TestSendMessage(message, GetSocket());

  wait_predicate([&result]{return result == "test";},
                 std::chrono::milliseconds(3));

  EXPECT_EQ(result, "test");
}

TEST_F(SpekeSessionTestF, SendMessage) {
  auto session = GetSession();
  std::string result;
  session->Run([](auto, auto&){});

  SendInitData();

  session->SendMessage(lrm::Util::str_to_bytes("test"));

  SpekeMessage message;
  // 3 messages sent by the session would be: init data, key confirmation and
  // our signed message
  for (int i = 0; i < 3; ++i) {
    message = TestSpekeSession::TestReceiveMessage(GetSocket());
    if (message.has_signed_data()) break;
  }

  EXPECT_EQ("hmac", message.signed_data().hmac_signature());
  EXPECT_EQ("test", message.signed_data().data());
}
