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

#ifndef LRM_GRPCCALLAUTHENTICATOR_H_
#define LRM_GRPCCALLAUTHENTICATOR_H_

#include "grpcpp/security/credentials.h"

namespace lrm {
class GrpcCallAuthenticator : public grpc::MetadataCredentialsPlugin {
 public:
  explicit GrpcCallAuthenticator(std::string_view passphrase);

  grpc::Status GetMetadata(
      grpc::string_ref service_url, grpc::string_ref method_name,
      const grpc::AuthContext& channel_auth_context,
      std::multimap<grpc::string, grpc::string>* metadata) override;

 private:
  grpc::string passphrase_;
};
}

#endif  // LRM_GRPCCALLAUTHENTICATOR_H_
