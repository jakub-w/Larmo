#ifndef LRM_GRPCCALLAUTHENTICATOR_H_
#define LRM_GRPCCALLAUTHENTICATOR_H_

#include "grpcpp/security/credentials.h"

namespace lrm {
class GrpcCallAuthenticator : public grpc::MetadataCredentialsPlugin {
 public:
  GrpcCallAuthenticator(std::string_view passphrase);

  grpc::Status GetMetadata(
      grpc::string_ref service_url, grpc::string_ref method_name,
      const grpc::AuthContext& channel_auth_context,
      std::multimap<grpc::string, grpc::string>* metadata) override;

 private:
  grpc::string passphrase_;
};
}

#endif  // LRM_GRPCCALLAUTHENTICATOR_H_
