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

#include "crypto/SPEKE.h"
#include "crypto/SpekeSession.h"

#include <asio.hpp>

using namespace lrm::crypto;
using tcp = asio::ip::tcp;
using stream_protocol = asio::local::stream_protocol;

// TEST(SpekeSessionTest, ConstructSpekeSession) {
  // ASSERT_NO_THROW(SpekeSession session{std::make_shared<asio::io_context>()});
// }

asio::io_context context;

TEST(SpekeSessionTest, Construct_ThrowSocketNotConnectedSpekeGood) {
  stream_protocol::socket socket(context);
  auto speke = std::make_unique<SPEKE>("id", "password", 7);

  EXPECT_THROW(SpekeSession(std::move(socket), std::move(speke)),
               std::logic_error)
      << "Should throw when the given socket is not connected to anything";

  socket.close();
}

TEST(SpekeSessionTest, Construct_ThrowSocketConnectedSpekeNullptr) {
  stream_protocol::socket socket1(context);
  stream_protocol::socket socket2(context);
  asio::local::connect_pair(socket1, socket2);

  auto speke = std::unique_ptr<SPEKE>(nullptr);

  EXPECT_THROW(SpekeSession(std::move(socket1), std::move(speke)),
               std::logic_error)
      << "Should throw when speke is nullptr";

  socket1.close();
  socket2.close();
}

TEST(SpekeSessionTest, Construct_NoThrowSocketConnectedSpekeGood) {
  stream_protocol::socket socket1(context);
  stream_protocol::socket socket2(context);
  asio::local::connect_pair(socket1, socket2);

  auto speke = std::make_unique<SPEKE>("id", "password", 7);

  EXPECT_NO_THROW(SpekeSession(std::move(socket1), std::move(speke)))
      << "Should not throw when the given socket is connected to another and "
      "speke is not nullptr";

  socket1.close();
  socket2.close();
}

// TEST(SpekeSessionTest, Construct_ThrowSpekeNotInstantiated) {
//   local::socket socket1(context);
//   local::socket socket2(context);
//   asio::local::connect_pair(socket1, socket1);

//   tcp::socket socket(context);
//   auto speke = std::make_unique<SPEKE>("id", "password", 7);

//   EXPECT_THROW(SpekeSession(std::move(socket), std::move(speke)),
//                std::logic_error)
//       << "Should throw when the given socket is not connected to anything";
// }

// TEST(SpekeSessionTest, ConnectTwoSessions) {
//   auto context = std::make_shared<asio::io_context>();

//   SpekeSession session1{context};
//   SpekeSession session2{context};

//   session1.Listen(42342, "password", 2692367);
//   session2.Connect("localhost", 42342, "password", 2692367);
// }
