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

#include <iostream>

#include "Config.h"

int main() {
  Config::Load("/tmp/conf");

  Config::SetMaybe("foo", "lol");
  Config::SetMaybe("lol", "lol");

  std::cout << Config::Get("lol") << ' ' << Config::Get("foo") << '\n';

  Config::Require({"foo", "lol", "baz", "lel"});

  std::string error_message{"Missing config settings: "};
  auto missing = Config::CheckMissing();
  for(const std::string* var : missing) {
    error_message += *var + ", ";
  }
  error_message.erase(error_message.length() - 2);

  try {
  if (not missing.empty()) {
    throw std::logic_error(error_message);
  }
  } catch (const std::logic_error& e) {
    std::cerr << e.what() << '\n';
  }

  return 0;
}
