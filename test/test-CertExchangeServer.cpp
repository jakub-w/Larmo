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

#include <memory>

#include <asio.hpp>
#include <asio/local/stream_protocol.hpp>

#include "crypto/certs/KeyPair.h"
#include "CertExchangeServer.h"
#include "Util.h"

using namespace lrm::crypto::certs;
using namespace lrm;

using stream_protocol = asio::local::stream_protocol;
using ExchangeServer = CertExchangeServer<stream_protocol>;
using SpekeSession = crypto::SpekeSession<stream_protocol>;

class CertExchangeServerTest : public ::testing::Test {
 protected:
  inline static auto CA = std::make_shared<CertificateAuthority>(
      Map{{"countryName", "PL"},
          {"stateOrProvinceName", "Larmoland"},
          {"organizationName", "Larmo"},
          {"commonName", "LarmoCN"}},
      KeyPair::Generate(KeyPair::ED25519()));
  inline static auto password = "password";
  inline static unsigned short port = 43243;

  CertExchangeServerTest()
      : endpoint("lrm-test.socket"),
        context{},
        context_thread{[this](){
                         auto context_guard = asio::make_work_guard(context);
                         try {
                           context.run();
                         } catch (const asio::system_error& e) {}
                       }} {}
  ~CertExchangeServerTest() {
    context.stop();
    context_thread.join();
    fs::remove(endpoint.path());
  }

  stream_protocol::endpoint endpoint;
  asio::io_context context;
  std::thread context_thread;
};

TEST_F(CertExchangeServerTest, Construct) {
  auto endpoint = stream_protocol::endpoint();
  EXPECT_NO_THROW(ExchangeServer(endpoint, password, CA));
}

TEST_F(CertExchangeServerTest, CreateSession) {
  auto server = ExchangeServer(endpoint, password, CA);
  server.Start();

  auto socket = stream_protocol::socket(context, stream_protocol());
  socket.connect(endpoint);
  auto session = SpekeSession(
      std::move(socket),
      std::make_shared<crypto::SPEKE>(
          "id", password, crypto::BigNum(crypto::LRM_SPEKE_SAFE_PRIME)));

  std::atomic<bool> response_received = false;
  session.Run([&](auto, auto&){ response_received = true; });

  Util::wait_predicate(
      [&session]{
        return crypto::SpekeSessionState::RUNNING != session.GetState(); },
      std::chrono::milliseconds(30));

  EXPECT_EQ(crypto::SpekeSessionState::RUNNING, session.GetState());

  session.SendMessage(crypto::Bytes(1));

  Util::wait_predicate(
      [&]{ return response_received == true; },
      std::chrono::milliseconds(10));

  EXPECT_TRUE(response_received);
}
