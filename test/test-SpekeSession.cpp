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

#include "crypto/SpekeSession.h"

#include <asio.hpp>

using namespace lrm::crypto;

// TEST(SpekeSessionTest, ConstructSpekeSession) {
  // ASSERT_NO_THROW(SpekeSession session{std::make_shared<asio::io_context>()});
// }

// TEST(SpekeSessionTest, ConnectTwoSessions) {
//   auto context = std::make_shared<asio::io_context>();

//   SpekeSession session1{context};
//   SpekeSession session2{context};

//   session1.Listen(42342, "password", 2692367);
//   session2.Connect("localhost", 42342, "password", 2692367);
// }
