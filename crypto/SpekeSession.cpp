#include "crypto/SpekeSession.h"

#include "crypto/SPEKE.pb.h"
#include "openssl/md5.h"

namespace lrm::crypto {
SpekeSession::~SpekeSession() {
  Disconnect();
}

/// \param host Hostname or IPv4 of the remote party.
/// \param port Port on which to connect.
void SpekeSession::Connect(std::string_view host, uint16_t port,
                           std::string_view password,
                           const BigNum& safe_prime) {
  Disconnect();
  // Connect to the remote party on host:port
  auto results = tcp::resolver(*context_).resolve(
      host.data(), std::to_string(port));

  tcp::endpoint ep;
  for (auto& result : results) {
    if (result.endpoint().protocol() == tcp::v4()) {
      ep = result.endpoint();
      break;
    }
  }

  // TODO: handle unresolved host
  stream_.connect(ep);

  id_ = make_id();
  speke_ = std::make_unique<SPEKE>(id_, password, safe_prime);

  SPEKEmsg message;
  message.set_type(SPEKEmsg::PUB_KEY);

  // auto pubkey_bytes = pubkey_.to_bytes();
  auto pubkey = speke_->GetPublicKey();
  message.set_data(pubkey.data(), pubkey.size());

  start_reading();

  message.SerializeToOstream(&stream_);
}

// TODO: Maybe add an argument to notify the peer why are we disconnecting
void SpekeSession::Disconnect() {
  if (stream_.socket().is_open()) {
    stream_.socket().shutdown(tcp::socket::shutdown_both);
  }
  stream_.close();

  speke_.release();
  id_.clear();

  other_pubkey_.clear();
  other_id_.clear();
}

std::string SpekeSession::make_id() const {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();

  EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);

  Bytes pkey = speke_->GetPublicKey();
  EVP_DigestUpdate(ctx, pkey.data(), pkey.size());

  auto timestamp = std::chrono::high_resolution_clock::now()
                   .time_since_epoch().count();
  EVP_DigestUpdate(ctx, &timestamp, sizeof(timestamp));

  unsigned char md_val[MD5_DIGEST_LENGTH];
  unsigned int md_len;
  EVP_DigestFinal_ex(ctx, md_val, &md_len);

  EVP_MD_CTX_free(ctx);

  char buffer[MD5_DIGEST_LENGTH * 2];
  for (unsigned int i = 0; i < md_len; ++i) {
    std::sprintf(&buffer[2*i], "%02X", md_val[i]);
  }

  return std::string(buffer);
}

void SpekeSession::start_reading() {
  stream_.socket().async_wait(tcp::socket::wait_read,
                              std::bind(&SpekeSession::handle_read, this,
                                        std::placeholders::_1));
}

void SpekeSession::handle_read(const asio::error_code& ec) {
  if (ec) {
    // TODO: log it
    return;
  }

  SPEKEmsg message;
  message.ParseFromIstream(&stream_);

  start_reading();

  switch (message.type()) {
    case SPEKEmsg::PUB_KEY:
      {
        if (not other_pubkey_.empty()) {
          return;
        }
        other_pubkey_ = Bytes((unsigned char*)message.data().data(),
                              (unsigned char*)message.data().data() +
                              message.data().length());

        if (not other_id_.empty()) {
          speke_->ProvideRemotePublicKeyIdPair(other_pubkey_, other_id_);
        }
      }
      break;
    case SPEKEmsg::ID:
      if (not id_.empty()) {
        return;
      }
      other_id_ = message.data();

      if (not other_pubkey_.empty()) {
        speke_->ProvideRemotePublicKeyIdPair(other_pubkey_, other_id_);
      }
      break;
    case SPEKEmsg::KEY_CONFIRM:
      {
        auto remote_kcd = Bytes{(unsigned char*)message.data().data(),
                                (unsigned char*)message.data().data() +
                                message.data().length()};
        if (not speke_->ConfirmKey(remote_kcd)) {
          Disconnect();
        }
      }
      break;
    case SPEKEmsg::ENC_DATA:
      break;
    default:
      break;
  }
}

}
