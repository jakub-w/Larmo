#include "GrpcCallAuthenticator.h"

namespace lrm {
GrpcCallAuthenticator::GrpcCallAuthenticator(std::string_view passphrase)
    : passphrase_(passphrase) {}

grpc::Status GrpcCallAuthenticator::GetMetadata(
      grpc::string_ref service_url, grpc::string_ref method_name,
      const grpc::AuthContext& channel_auth_context,
      std::multimap<grpc::string, grpc::string>* metadata) {
    metadata->insert(std::make_pair("x-custom-passphrase", passphrase_));

    return grpc::Status::OK;
  }
}
