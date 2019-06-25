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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <future>
#include <mutex>

#include <argp.h>

#include "grpcpp/create_channel.h"

#include "asio.hpp"

#include "Config.h"
#include "Daemon.h"
#include "PlayerClient.h"
#include "Util.h"

const char* argp_program_version = "lrm-client 0.1";
const char* argp_program_bug_address = "<doesnt@exist.addr>";
static char doc[] =
    "Lelo Remote Music Control -- Client for Lelo Remote Music Player\v"
    "Commands:\n"
    "  play FILE\n"
    "  stop\n"
    "  toggle-pause\n"
    "  volume VOL\t" "Absolute (e.g. 50) or relative (e.g. +10)\n"
    "  ping\n"
    "  daemon";

static argp_option options[] = {
  {"config", 'c', "FILE", 0, "Use an alternative config file", 0},
  {"host", 'h', "ADDRESS", 0, "Address of the gRPC server", 0},
  {"port", 'p', "NUM", 0, "Port for gRPC", 0},
  {"streaming-port", 's', "NUM", 0, "Port for streaming music", 0},
  {"pass", 'P', "PASSPHRASE", 0, "Passphrase for queries to the server", 0},
  {"daemon", 1, 0, 0, "Start a daemon", 0},
  {0, 0, 0, 0, 0, 0}
};

static const int DAEMON_TIMEOUT = 3;

struct arguments {
  std::string command;
  std::string command_arg;
  std::string config_file;
  std::string grpc_host;
  std::string grpc_port;
  std::string streaming_port;
  std::string passphrase;
  std::string cert_file;
};

// bool value is true if the command requires an argument
static const std::unordered_map<std::string, bool> commands = {
  {"play", true},
  {"stop", false},
  {"toggle-pause", false},
  {"volume", true},
  {"ping", false},
  {"daemon", false}
};

static const char args_doc[] = "COMMAND [ARG]";

static error_t parse_opt(int key, char* arg, argp_state* state) {
  arguments* args = (arguments*)state->input;

  switch(key) {
    case ARGP_KEY_INIT:
      // Check if "volume" command is used, in that case check its argument
      // and if it's a negative number, replace '-' in front of it with 'm'.
      // Otherwise argp would interpret this argument as an option which
      // doesn't exist.
      for (auto i = 0; i < state->argc; ++i) {
        if ((std::strcmp(state->argv[i], "volume") == 0) and
            (i + 1 < state->argc) and
            (std::strlen(state->argv[i + 1]) > 1) and
            (state->argv[i + 1][0] == '-')) {
          state->argv[i + 1][0] = 'm';
          break;
        }
      }
      break;
    case ARGP_KEY_SUCCESS:
      if (auto cmd = commands.find(args->command);
          (cmd != commands.end()) and
          (cmd->second == true) and
          (args->command_arg.empty())) {
        argp_error(state, "Command '%s' requires an argument",
                   cmd->first.c_str());
        return EINVAL;
      }
      break;
    case 'p': case 's':
      try {
        int port = std::stoi(arg);
        if (port <= IPPORT_USERRESERVED or port > 65535) {
          throw std::invalid_argument("Invalid port.");
        }
        if ('p' == key) {
          args->grpc_port = std::to_string(port);
        } else {
          args->streaming_port = std::to_string(port);
        }
      } catch (const std::invalid_argument& e) {
        argp_error(state, "Wrong port: %s", arg);
        return EINVAL;
      }
      break;
    case 'c':
      struct stat buffer;
      if (stat(arg, &buffer) == 0) {
        args->config_file = arg;
      } else {
        argp_error(state, "File doesn't exist: %s", arg);
        return EINVAL;
      }
      break;
    case 'h':
      args->grpc_host = arg;
      break;
    case 'P':
      args->passphrase = arg;
      break;
    case ARGP_KEY_ARG:
      if (commands.find(arg) == commands.end()) {
        if (not args->command_arg.empty()) {
          return ARGP_ERR_UNKNOWN;
        }
        if (args->command == "play") {
          if (lrm::file_exists(arg)) {
            args->command_arg = arg;
          } else {
            argp_error(state, "File doesn't exist: %s", arg);
          }
        } else if (args->command == "volume") {
          int length = std::strlen(arg);
          if (length > 1 and arg[0] == 'm') {
            arg[0] = '-';
          }

          args->command_arg = arg;
        } else {
          argp_error(state, "Unknown command: %s", arg);
          return EINVAL;
        }
      } else {
        args->command = arg;
      }
      break;
    case ARGP_KEY_NO_ARGS:
      argp_usage(state);
      // argp_state_help(state, state->err_stream,
                      // ARGP_HELP_STD_HELP | ARGP_HELP_EXIT_ERR);
      return EINVAL;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static argp argp = { options, parse_opt, args_doc,
                     doc, 0, 0, 0 };

namespace lrm {
class daemon_init_error : public std::logic_error {
 public:
  daemon_init_error() : std::logic_error("Couldn't create a daemon") {}
};
}

pid_t start_daemon(std::unique_ptr<lrm::Daemon::daemon_info> dinfo) {
  if (dinfo->config_file.empty()) {
    Config::Load();
  } else {
    Config::Load(dinfo->config_file.string());
  }
  assert(Config::GetState() == Config::LOADED);

  pid_t pid = fork();
  switch (pid) {
    case -1:
      throw std::system_error(errno, std::system_category());
    case 0:
      {
        int exit_code = EXIT_SUCCESS;
        {
          std::ofstream log;

          try {
            // Initialize a daemon process
            umask(0);

            std::filesystem::path log_file{Config::Get("log_file")};

            // TODO: Change the default logging location to something better
            if (log_file.empty()) log_file = "/tmp/lrm/log";
            std::filesystem::create_directories(log_file.parent_path());

            log.open(log_file);
            std::unitbuf(log);

            dinfo->log_file = log_file.string();

            // if ((setsid() < 0) or
                // (chdir("/") < 0)) {
            if (setsid() < 0) {
              throw std::system_error(errno, std::system_category());
            }

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            // End of initialization

            lrm::Daemon daemon(std::move(dinfo));
            daemon.Run();
          } catch (const std::exception& e) {
            log << e.what() << std::endl;
            exit_code = EXIT_FAILURE;
          }
        }
        std::_Exit(exit_code);
      }
    default:
      return pid;
  }
}

int main(int argc, char** argv) {
  arguments args;
  argp_parse(&argp, argc, argv, 0, 0, &args);

  asio::io_context context;
  asio::local::stream_protocol::socket socket(context);
  asio::local::stream_protocol::endpoint endpoint(lrm::Daemon::socket_path);

  try {
    socket.connect(endpoint);
  } catch (const asio::system_error& e) {
    if (e.code().value() == ENOENT) {
      // Daemon is not running.
      try {
        if (args.command != "daemon") {
          throw lrm::daemon_init_error();
        }

        auto dinfo = std::make_unique<lrm::Daemon::daemon_info>();
        dinfo->config_file = args.config_file;
        dinfo->grpc_host = args.grpc_host;
        dinfo->grpc_port = args.grpc_port;
        dinfo->streaming_port = args.streaming_port;
        dinfo->passphrase = args.passphrase;
        dinfo->cert_file = args.cert_file;

        start_daemon(std::move(dinfo));

        // Try for DAEMON_TIMEOUT seconds to connect to a daemon
        int count = 0;
        for(;; ++count) {
          try {
            socket.connect(endpoint);
            break;
          } catch (const asio::system_error&) {
            if (count > DAEMON_TIMEOUT) {
              throw std::runtime_error(
                  "Timeout reached. Couldn't connect to a daemon");
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
        }
        std::cout << "Daemon started with settings:\n"
                  << "\tconfig_file: " << Config::Get("config_file") << '\n'
                  << "\tgrpc_host: " << Config::Get("grpc_host") << '\n'
                  << "\tgrpc_port: " << Config::Get("grpc_port") << '\n'
                  << "\tstreaming_port: " << Config::Get("streaming_port")
                  << '\n'
                  << "\tpassphrase: " << Config::Get("passphrase") << '\n'
                  << "\tcert_file: " << Config::Get("cert_file") << "\n";

        return EXIT_SUCCESS;
      } catch (const lrm::daemon_init_error&) {
        std::cerr << "Daemon not running. Use 'daemon' command.\n";
        return EXIT_FAILURE;
      } catch (const std::system_error& e) {
        std::cerr << "System error (" << e.code() << "): "
                  << e.what() << '\n';
        return EXIT_FAILURE;
      } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
      }
    } else {
      std::cerr << e.what() << '\n';
      return EXIT_FAILURE;
    }
  }

  if (args.command == "daemon") {
    std::cout << "Daemon already running.\n";
    return EXIT_SUCCESS;
  }

  DaemonArguments cmd;
  cmd.set_command(args.command);
  cmd.set_command_arg(args.command_arg);

  cmd.SerializeToFileDescriptor(socket.native_handle());
  socket.shutdown(socket.shutdown_send);
  std::cout << "Sent the command\n";

  DaemonResponse response;
  response.ParseFromFileDescriptor(socket.native_handle());

  std::cout << response.exit_status() << ": " << response.response() << '\n';

  return response.exit_status();

  // std::cout << args.command << ' ' << args.command_arg << '\n';
  // return EXIT_SUCCESS;
}
