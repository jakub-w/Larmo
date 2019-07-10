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

#include <filesystem>
#include <random>

#include <argp.h>
#include <sys/stat.h>

#include "mpv/client.h"

#include "grpcpp/server.h"
#include "grpcpp/server_builder.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "Config.h"
#include "PlayerServiceImpl.h"
#include "Util.h"

const char* argp_program_version = "lrm-server 0.1";
const char* argp_program_bug_address = "<doesnt@exist.addr>";
static char doc[] =
    "Lelo Remote Music Player -- Server that plays music client provides";

static argp_option options[] = {
  {"port", 'p', "NUM", 0, "Port for gRPC", 0},
  {"config", 'c', "FILE", 0, "Use an alternative config file", 0},
  {"pass", 'P', "PASSPHRASE", 0, "Passphrase for client queries", 0},
  {0, 0, 0, 0, 0, 0}
};

struct arguments {
  std::string grpc_port;
  std::string config_file;
  std::string passphrase;
};

static error_t parse_opt(int key, char* arg, argp_state* state) {
  arguments* args = (arguments*)state->input;

  switch(key) {
    case 'p':
      try {
        int port = std::stoi(arg);
        if (port <= IPPORT_USERRESERVED or port > 65535) {
          throw std::invalid_argument("Invalid port.");
        }
        args->grpc_port = std::to_string(port);
      } catch (const std::invalid_argument& e) {
        argp_error(state, "Wrong port: %s", arg);
        return EINVAL;
      }
      break;
    case 'c':
      if (lrm::file_exists(arg)) {
        args->config_file = arg;
      } else {
        argp_error(state, "File doesn't exist: %s", arg);
        return EINVAL;
      }
      break;
    case 'P':
      args->passphrase = arg;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static argp argp = { options, parse_opt, 0, doc, 0, 0, 0 };

int initialize_config(arguments* args) {
  // This will throw if the config file cannot be loaded.
  if (args->config_file.empty()) {
    // TODO: load config from /etc/ or XDG_CONFIG_HOME
    Config::Load();
  } else {
    Config::Load(args->config_file);
  }

  if (not args->grpc_port.empty()) {
    Config::Set("grpc_port", args->grpc_port);
  }
  if (Config::Get("grpc_port").empty()) {
    throw std::logic_error("Port for gRPC not provided.");
  }

  if (not args->passphrase.empty()) {
    Config::Set("passphrase", args->passphrase);
  }
  if (Config::Get("passphrase").empty()) {
    throw std::logic_error("Passphrase not provided.");
  }

  return 0;
}

void init_logging(const std::filesystem::path& log_file) {
  try {
  std::filesystem::create_directories(log_file.parent_path());

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      log_file, /* 5 MB */ 1024 * 1024 * 5, 3, true));

  auto combined_logger = std::make_shared<spdlog::logger>(
      "remote-player", std::begin(sinks), std::end(sinks));

  spdlog::set_default_logger(combined_logger);

#ifndef NDEBUG
  spdlog::set_level(spdlog::level::debug);
  spdlog::flush_on(spdlog::level::debug);
#else
  char* debug_val = std::getenv("DEBUG");
  if (debug_val and (std::strcmp(debug_val, "true") or
                     std::strcmp(debug_val, "1"))) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
  } else {
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
  }
#endif // DNDEBUG
  } catch (const spdlog::spdlog_ex& ex) {
    std::cerr << "Log initialization failed: " << ex.what();
  }
}

int main(int argc, char** argv) {
  arguments args;
  argp_parse(&argp, argc, argv, 0, 0, &args);

  try {
  initialize_config(&args);

  // TODO: change it to "log_file"
  std::filesystem::path log_file{Config::Get("player_log_file")};
  // TODO: Change the default logging location to something better
  if (log_file.empty()) {
    log_file = std::filesystem::temp_directory_path()
               .append("lrm/player.log");
  }
  std::cout << "log file: " << log_file.string() << '\n';

  init_logging(log_file);

  std::stringstream settings;
  settings << "Settings:";
  for(auto& setting : Config::GetMap()) {
    if (setting.first.empty()) continue;
    settings << "\n\t" << setting.first << " = " << setting.second;
  }
  spdlog::info(settings.str());

  grpc::ServerBuilder builder;
  PlayerServiceImpl player_service;

  // TODO: get it from the config file or some default directory
  Config::Set("cert_file", "server.crt");
  Config::Set("key_file", "server.key");

  std::string ssl_cert = lrm::file_to_str(Config::Get("cert_file"));
  std::string ssl_key = lrm::file_to_str(Config::Get("key_file"));
  if (ssl_cert.empty()) {
    spdlog::error("Error: Certificate file '{}' is empty",
                  Config::Get("cert_file"));
    return EXIT_FAILURE;
  }
  if (ssl_key.empty()) {
    spdlog::error("Error: Encryption key file '{}' is empty",
                  Config::Get("key_file"));
    return EXIT_FAILURE;
  }

  // spdlog::debug("Certificate:\n{}\n\nKey:\n{}", ssl_cert, ssl_key);

  grpc::SslServerCredentialsOptions ssl_options;
  ssl_options.pem_key_cert_pairs.push_back({ssl_key, ssl_cert});
  // ssl_options.force_client_auth = true;
  // ssl_options.force_client_auth =
      // GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;

  auto creds = grpc::SslServerCredentials(ssl_options);

  grpc::string address("0.0.0.0:" + Config::Get("grpc_port"));
  builder.AddListeningPort(address, creds);

  builder.RegisterService((Service*) &player_service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  spdlog::info("gRPC listening on: '{}'...", address);

  server->Wait();
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return 0;
}
