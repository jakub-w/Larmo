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

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <numeric>

#include "spdlog/spdlog.h"

#include "daemon_arguments.pb.h"

#ifndef INCLUDE_GRPCPLUSPLUS
#include "grpcpp/create_channel.h"
#include "grpcpp/channel.h"
#else
#include "grpc++/create_channel.h"
#include "grpc++/channel.h"
#endif  // INCLUDE_GRPCPLUSPLUS


#include "filesystem.h"
#include "Config.h"
#include "PlayerClient.h"
#include "Util.h"

using namespace asio::local;

namespace lrm {
const fs::path Daemon::socket_path =
    fs::temp_directory_path().append("lrm/socket");

Daemon::Daemon(std::unique_ptr<daemon_info>&& dinfo) noexcept
    : dinfo_(std::move(dinfo)), endpoint_(socket_path), acceptor_(context_),
      log_(spdlog::get("Daemon")) {
  assert(socket_path.is_absolute());

  try {
    fs::create_directories(socket_path.parent_path());
    acceptor_ = stream_protocol::acceptor(context_, endpoint_, true);
  } catch (const fs::filesystem_error& e) {
    log_->error("Couldn't create path ({}): {}",
                e.path1().string(), e.what());
  } catch (const asio::system_error& e) {
    log_->error("Couldn't create the local socket: {}", e.what());
  }
}

Daemon::~Daemon() {
  try {
    grpc_channel_state_run_ = false;
    fs::remove(socket_path);

    log_->flush();
    grpc_channel_state_thread_.join();
  } catch (const fs::filesystem_error& e) {
    if (e.code() != std::errc::no_such_file_or_directory) {
      log_->error("While removing a file '{}': {}",
                  e.path1().string(), e.what());
    }
  } catch (...) {}
}

void Daemon::Run() {
  Initialize();
  // The above may throw

  remote_->StreamInfoStart();

  auto signals = asio::signal_set(context_, SIGINT, SIGTERM);
  signals.async_wait([this](const asio::error_code& error,
                            int signal_number) {
                       if (not error) {
                         log_->info("Exiting on {}...",
                                    strsignal(signal_number));
                         context_.stop();
                       } else {
                         log_->error(error.message());
                       }
                     });

  log_->info("Starting to accept connections");
  start_accept();

  context_.run();
}

void Daemon::Initialize() {
  switch (state_) {
    case UNINITIALIZED:
      initialize_config();
      initialize_grpc_client();
      break;
    case CONFIG_INITIALIZED:
      initialize_grpc_client();
      break;
    case GRPC_CLIENT_INITIALIZED:
      authenticate();
      break;
    case AUTHENTICATED:
      break;
    default:
      throw std::logic_error("Unknown daemon state. This should never"
                             " happen");
  }
}

void Daemon::initialize_config() {
  if (state_ != UNINITIALIZED) {
    // TODO: Probably throw
    return;
  }

  // This will throw if the config file cannot be loaded.
  const fs::path conf = dinfo_->config_file.empty() ?
                        Config::default_conf_file :
                        dinfo_->config_file;

  log_->info("Initializing the configuration from file: '{}'...",
               conf.string());
  Config::Load(conf);

  // This will set config variables only if the arguments are not empty.
  // So it will overwrite only values that are set in dinfo_.
  Config::Set("grpc_port", dinfo_->grpc_port);
  Config::Set("grpc_host", dinfo_->grpc_host);
  Config::Set("cert_port", dinfo_->cert_port);
  Config::Set("passphrase", dinfo_->passphrase);
  Config::Set("cert_file", dinfo_->cert_file.string());

  std::stringstream settings;
  settings << "Settings:";
  for(const auto& [key, value] : Config::GetMap()) {
    if (key.empty()) continue;
    settings << "\n\t" << key << " = " << value;
  }
  log_->info(settings.str());

  Config::Require({"grpc_port", "grpc_host", "passphrase", "cert_file"});

  auto missing = Config::CheckMissing();
  if (not missing.empty()) {
    auto error_message =
        std::accumulate(std::begin(missing), std::end(missing),
                        std::string("Missing config settings: "),
                        [](const auto& result, auto* str) {
                          return result + *str + ", "; });
    error_message.erase(error_message.length() - 2);
    throw std::logic_error(error_message);
  }

  const auto grpc_port = Config::Get("grpc_port");
  Util::check_port(grpc_port);

  auto cert_port = Config::Get("cert_port");
  if (cert_port.empty()) {
    cert_port = "0";
    Config::Set("cert_port", "0");
  }
  Util::check_port(cert_port);

  state_ = CONFIG_INITIALIZED;
  log_->info("Configuration initialized.");
}

using namespace grpc;

void Daemon::initialize_grpc_client() {
  if (state_ != CONFIG_INITIALIZED) {
    log_->warn("Called Daemon::initialize_grpc_client() while config was "
               "not initialized");
    // TODO: probably throw
    return;
  }

  log_->info("Initializing gRPC client...");

  const grpc::string grpc_address(Config::Get("grpc_host") + ':' +
                                  Config::Get("grpc_port"));

  log_->info("Connecting to gRPC remote at: {}", grpc_address);


  const auto creds = grpc::SslCredentials([this](){
    grpc::SslCredentialsOptions options;

    options.pem_root_certs = Util::file_to_str(Config::Get("cert_file"));

    if (options.pem_root_certs.empty()) {
      const std::string error_message{
        "Certificate file '" + Config::Get("cert_file") + "' is empty"};
      log_->error(error_message);
      throw std::logic_error(error_message);
    }

    return options;
  }());

  {
    const auto channel = grpc::CreateChannel(grpc_address, creds);

    // Trace the channel state
    grpc_channel_state_thread_ =
        std::thread(&Daemon::trace_grpc_channel_state, this, channel);

    // Wait for channel to connect to the server (5 seconds)
    for (int i = 0;
         channel->GetState(false) != GRPC_CHANNEL_READY and i < 5;
         ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    if (channel->GetState(false) != GRPC_CHANNEL_READY) {
      throw std::system_error(std::make_error_code(std::errc::timed_out),
                              "Connecting to a server");
    }

    remote_ = std::make_unique<PlayerClient>(channel);
  }

  state_ = GRPC_CLIENT_INITIALIZED;



  log_->info("gRPC client initialized");
}

void Daemon::authenticate() {
  if (remote_->Authenticate()) {
    state_ = AUTHENTICATED;
    log_->info("Authentication completed");
    return;
  }

  // log_->error("Authentication unsuccessful");
  throw std::logic_error("Authentification unsuccessful");
}

void Daemon::start_accept() {
  assert(not connection_);
  connection_ = std::make_unique<stream_protocol::socket>(context_);

  acceptor_.async_accept(
      *connection_,
      endpoint_,
      [this](const asio::error_code& error){
        if (not error) {
          auto conn = std::move(connection_);
          start_accept();
          connection_handler(std::move(conn));
        } else {
          log_->error("In accept handler: {}", error.message());
        }
      });
}

void Daemon::connection_handler(
    std::unique_ptr<stream_protocol::socket>&& socket) {
  DaemonArguments args;

  // Wait for some time for the args, if the socket doesn't send them,
  // just drop the connection
  if (not Util::wait_predicate([&socket]{return socket->available();},
                               std::chrono::seconds(1))) {
    socket->close();
    return;
  }

  args.ParseFromFileDescriptor(socket->native_handle());

  log_->info("Request received: {} {}", args.command(), args.command_arg());

  DaemonResponse response;

  switch (state_) {
    case State::UNINITIALIZED:
      response.set_exit_status(EXIT_FAILURE);
      response.set_response("Daemon uninitialized. Use 'daemon' command.");
      break;
    case State::GRPC_CLIENT_INITIALIZED:
      response.set_exit_status(EXIT_FAILURE);
      response.set_response("Daemon not authenticated with the server.");
      break;
    case State::AUTHENTICATED:
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
        } else if (args.command() == "info") {
          response.set_response(remote_->Info(args.command_arg()));
          result = 0;
        } else if (args.command() == "seek") {
          result = remote_->Seek(args.command_arg());
        }
        response.set_exit_status(result);
      } catch (const std::exception& e) {
        response.set_exit_status(EXIT_FAILURE);
        response.set_response(std::string("[Error] ") + e.what());
      } catch (const grpc::Status& s) {
        response.set_exit_status(EXIT_FAILURE);
        response.set_response(
            "gRPC error: " + s.error_message() +
            (s.error_details().empty() ? "" : ", " + s.error_details()));
      }
      break;
    default:
      response.set_exit_status(EXIT_FAILURE);
      response.set_response("Daemon in the limbo state.");
      break;
  }

  response.SerializeToFileDescriptor(socket->native_handle());
  socket->close();

  log_->info("Response sent: ({}) {}",
             response.exit_status(), response.response());
}

void Daemon::trace_grpc_channel_state(
    std::shared_ptr<grpc::Channel> channel) {
  grpc_connectivity_state state = channel->GetState(true);
  while (grpc_channel_state_run_) {
    // An error is generated every time the timeout is
    // reached, so the gRPC logs can be spammy
    if (channel->WaitForStateChange(
            state,
            std::chrono::system_clock::time_point(
                std::chrono::system_clock::now() +
                std::chrono::seconds(5)))) {
      state = channel->GetState(false);

      const auto log_info =
          [&](){
            switch (state) {
              case GRPC_CHANNEL_IDLE:
                return std::make_pair(spdlog::level::debug, "is idle");
              case GRPC_CHANNEL_CONNECTING:
                return std::make_pair(spdlog::level::info, "is connecting");
              case GRPC_CHANNEL_READY:
                return std::make_pair(
                    spdlog::level::info,"is ready for work");
              case GRPC_CHANNEL_TRANSIENT_FAILURE:
                return std::make_pair(
                    spdlog::level::warn,
                    "has seen a failure but expects to recover");
              case GRPC_CHANNEL_SHUTDOWN:
                return std::make_pair(
                    spdlog::level::err,
                    "has seen a failure that it cannot recover from");
              default:
                return std::make_pair(
                    spdlog::level::warn,
                    "is in an unknown state (not implemented)");
            }}();

      log_->log(log_info.first, "gRPC channel {}", log_info.second);
    }
  }
}
}
