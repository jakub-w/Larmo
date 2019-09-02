#ifndef LRM_SPEKESESSION_H_
#define LRM_SPEKESESSION_H_

#include <asio.hpp>

#include "crypto/SPEKE.h"

namespace lrm::crypto {

class SpekeSession {
  using tcp = asio::ip::tcp;

  SpekeSession(std::shared_ptr<asio::io_context> io_context)
      : context_(std::move(io_context)) {}

  ~SpekeSession();

  void Connect(std::string_view host, uint16_t port,
               std::string_view password, const BigNum& safe_prime);
  void Disconnect();

 private:
  /// \brief Make id out of the public key and the timestamp.
  ///
  /// \return Newly generated id.
  std::string make_id() const;

  void start_reading();
  void handle_read(const asio::error_code& ec);

  tcp::iostream stream_;
  std::shared_ptr<asio::io_context> context_;

  std::unique_ptr<SPEKE> speke_;
  std::string id_;

  Bytes other_pubkey_;
  std::string other_id_;
};
}

#endif  // LRM_SPEKESESSION_H_
