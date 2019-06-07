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

#include "Daemon.h"

#include <iostream>
#include <filesystem>
#include <fstream>

#include "daemon_arguments.pb.h"
#include "grpcpp/create_channel.h"

#include "Config.h"
#include "GrpcCallAuthenticator.h"
#include "PlayerClient.h"
#include "Util.h"

using namespace asio::local;

namespace lrm {
const std::filesystem::path Daemon::socket_path = "/tmp/lrm/socket";

Daemon::Daemon(std::unique_ptr<daemon_info> dinfo)
    : dinfo_(std::move(dinfo)), endpoint_(socket_path), acceptor_(context_) {
  assert(socket_path.is_absolute());

  std::filesystem::create_directories(socket_path.parent_path());
  acceptor_ = stream_protocol::acceptor(context_, endpoint_, true);
}

Daemon::~Daemon() noexcept {
  try {
    std::filesystem::remove(socket_path);
  } catch (const std::filesystem::filesystem_error& e) {
    if (e.code() != std::errc::no_such_file_or_directory) {
      // throw e;
    }
  }
}

void Daemon::Run() {
  initialize_config();
  initialize_grpc_client();
  // The two above may throw

  auto signals = asio::signal_set(context_, SIGINT, SIGTERM);
  signals.async_wait([this](const asio::error_code& error,
                            int signal_number) {
                       if (not error) {
                         log() << "Exiting on interrupt." << std::endl;
                         context_.stop();
                       } else {
                         log() << "[Error] " << error.message() << std::endl;
                       }
                     });

  start_accept();

  context_.run();
}

void Daemon::initialize_config() {
  if (state_ != UNINITIALIZED) {
    // TODO: Probably throw
    return;
  }
  // This will throw if the config file cannot be loaded.
  if (dinfo_->config_file.empty()) {
    // TODO: load config from /etc/ or XDG_CONFIG_HOME
    Config::Load();
  } else {
    Config::Load(dinfo_->config_file);
  }

  // This will set config variables only if the arguments are not empty.
  // So it will overwrite only values that are set in dinfo_.
  Config::Set("grpc_port", dinfo_->grpc_port);
  Config::Set("grpc_host", dinfo_->grpc_host);
  Config::Set("streaming_port", dinfo_->streaming_port);
  Config::Set("passphrase", dinfo_->passphrase);
  Config::Set("cert_file", dinfo_->cert_file);

  Config::Require({"grpc_port", "grpc_host", "passphrase", "cert_file"});

  std::string error_message{"Missing config settings: "};
  auto missing = Config::CheckMissing();
  for(const std::string* var : missing) {
    error_message += *var + ", ";
  }
  if (not missing.empty()) {
    error_message.erase(error_message.length() - 2);
    throw std::logic_error(error_message);
  }

  auto grpc_port = Config::Get("grpc_port");
  if (int port = std::stoi(grpc_port);
      port <= IPPORT_USERRESERVED or port > USHRT_MAX) {
    throw std::logic_error("Port for gRPC (" + grpc_port + ") is invalid");
  }

  // This does not warrant a throw, we can set it to 0 to randomize the port
  auto streaming_port = Config::Get("streaming_port");
  if (streaming_port.empty()) {
    Config::Set("streaming_port", "0");
  } else if (int port = std::stoi(streaming_port);
             port <= IPPORT_USERRESERVED or port > USHRT_MAX) {
    throw std::logic_error("Port for streaming (" + streaming_port +
                           ") is invalid");
  }

  state_ = CONFIG_INITIALIZED;
}

using namespace grpc;

void Daemon::initialize_grpc_client() {
  if (state_ != CONFIG_INITIALIZED) {
    // TODO: probably throw
    return;
  }
  grpc::string grpc_address(Config::Get("grpc_host") + ':' +
                            Config::Get("grpc_port"));

  std::shared_ptr<grpc::ChannelCredentials> creds;
  {
    auto call_creds = grpc::MetadataCredentialsFromPlugin(
        std::make_unique<GrpcCallAuthenticator>(Config::Get("passphrase")));
    grpc::SslCredentialsOptions options;
    std::string ssl_cert = lrm::file_to_str("server.crt");
    options.pem_root_certs = ssl_cert;
    auto channel_creds = grpc::SslCredentials(options);

    creds = grpc::CompositeChannelCredentials(channel_creds, call_creds);
  }

  remote_ = std::make_unique<PlayerClient>(
      Config::Get("streaming_port"),
      grpc::CreateChannel(grpc_address, creds));

  state_ = GRPC_CLIENT_INITIALIZED;
}

void Daemon::start_accept() {
  assert(not connection_);
  connection_ = std::make_unique<stream_protocol::socket>(context_);

  acceptor_.async_accept(
      *connection_,
      endpoint_,
      [this](const asio::error_code& error){
        if (not error) {
          std::thread(&Daemon::connection_handler,
                      this,
                      std::move(connection_)).detach();
          start_accept();
        } else {
          log() << "[Error] Error in accept handler: " << error.message()
                << std::endl;
        }
      });
}

void Daemon::connection_handler(
    std::unique_ptr<stream_protocol::socket>&& socket) {
  DaemonArguments args;
  log() << "Parsing the message.\n";
  args.ParseFromFileDescriptor(socket->native_handle());

  log() << "Request received: " << args.command() << " " << args.command_arg()
        << std::endl;

  DaemonResponse response;

  switch (state_) {
    case State::UNINITIALIZED:
        response.set_exit_status(EXIT_FAILURE);
        response.set_response("Daemon uninitialized. Use 'daemon' command.");
        break;
    case State::GRPC_CLIENT_INITIALIZED:
      try {
        int result = 0;
        if (args.command() == "play") {
          result = remote_->Play(args.command_arg());
        } else if (args.command() == "stop") {
          result = remote_->Stop();
        } else if (args.command() == "toggle-pause") {
          result = remote_->TogglePause();
        } else if (args.command() == "volume") {
          result = remote_->Volume(args.command_arg());
        } else if (args.command() == "ping") {
          result = (remote_->Ping() ? 0 : 1);
        }
        response.set_exit_status(result);
      } catch (const std::exception& e) {
        response.set_exit_status(EXIT_FAILURE);
        response.set_response(std::string("Error: ") + e.what());
      } catch (const grpc::Status& s) {
        response.set_exit_status(EXIT_FAILURE);
        response.set_response("gRPC error: " + s.error_message());
      }
      break;
    default:
      response.set_exit_status(EXIT_FAILURE);
      response.set_response("Daemon in the limbo state.");
      break;
  }

  response.SerializeToFileDescriptor(socket->native_handle());
  socket->close();

  log() << "Response sent: (" << response.exit_status() << ") "
        << response.response() << std::endl;
}

std::ofstream& Daemon::log() {
  return *(dinfo_->log_file);
}
}
