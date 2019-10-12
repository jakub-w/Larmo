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
#include <fstream>
#include <tuple>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <future>
#include <mutex>

#ifndef INCLUDE_GRPCPLUSPLUS
#include "grpcpp/create_channel.h"
#else
#include "grpc++/create_channel.h"
#endif  // INCLUDE_GRPCPLUSPLUS

#include "grpc/support/log.h"

#include "asio.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#include "filesystem.h"
#include "Config.h"
#include "Daemon.h"
#include "PlayerClient.h"
#include "Util.h"

#include "remote-control-argp.h"

static const int DAEMON_TIMEOUT = 3;

namespace lrm {
class daemon_init_error : public std::logic_error {
 public:
  daemon_init_error() : std::logic_error("Couldn't create a daemon") {}
};
}

using namespace lrm;

void gpr_default_log(gpr_log_func_args* args);
void gpr_log_function(gpr_log_func_args* args) {
  if (args->severity == GPR_LOG_SEVERITY_ERROR) {
    spdlog::get("gRPC")->error(args->message);
  }
  if (args->severity == GPR_LOG_SEVERITY_DEBUG) {
    spdlog::get("gRPC")->debug(args->message);
  }
  gpr_default_log(args);
}

void init_logging(const fs::path& log_file) {
  fs::create_directories(log_file.parent_path());

  auto file_sink =
      std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file.string(),
                                                          true);

  auto daemon_logger =
      std::make_shared<spdlog::logger>("Daemon", file_sink);
  spdlog::register_logger(daemon_logger);

  auto playerclient_logger =
      std::make_shared<spdlog::logger>("PlayerClient", file_sink);
  spdlog::register_logger(playerclient_logger);

  auto grpc_logger = std::make_shared<spdlog::logger>("gRPC", file_sink);
  spdlog::register_logger(grpc_logger);

  // Global settings
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

  // This sets the logger globally for this process
  spdlog::set_default_logger(daemon_logger);

  // Redirect gRPC error logs to the default logger
  gpr_set_log_function(gpr_log_function);
}

pid_t start_daemon(std::unique_ptr<lrm::Daemon::daemon_info>&& dinfo) {
  if (dinfo->config_file.empty()) {
    Config::Load();
  } else {
    Config::Load(dinfo->config_file.string());
  }
  assert(Config::GetState() == Config::LOADED);

  int fds[2];
  if (pipe(fds)) {
    throw std::system_error(errno, std::system_category());
  }

  pid_t pid = fork();
  switch (pid) {
    case -1:
      throw std::system_error(errno, std::system_category());
    case 0:
      {
        int exit_code = EXIT_SUCCESS;
        {
          close(fds[0]);
          FILE* pipe_write;
          pipe_write = fdopen(fds[1], "w");
          try {
            // Initialize a daemon process
            umask(0);

            fs::path log_file{Config::Get("log_file")};
            // TODO: Change the default logging location to something better
            if (log_file.empty()) {
              log_file = fs::temp_directory_path().append("lrm/daemon.log");
            }
            std::cout << "log_file = " << log_file.string() << '\n';

            try {
              init_logging(log_file);
            } catch (const spdlog::spdlog_ex& ex) {
              std::cerr << "Log initialization failed: " << ex.what();
              throw;
            }

            std::cout << "Logging initialized\n";

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

            {
              lrm::Daemon daemon(std::move(dinfo));
              daemon.Initialize();

              // Pass the exit code to the client
              std::fprintf(pipe_write, "%i\n", EXIT_SUCCESS);
              std::fflush(pipe_write);

              daemon.Run();
            }
            // The daemon fell out of scope without any crashes.
            spdlog::info("Daemon exited gracefully");
          } catch (const std::exception& e) {
            spdlog::error(e.what());
            exit_code = EXIT_FAILURE;
            // Write the exit code and error message to the client
            std::fprintf(pipe_write, "%i\n%s\n", exit_code, e.what());
          }
          fclose(pipe_write);
        }
        std::_Exit(exit_code);
      }
    default:
      close(fds[1]);
      FILE* pipe_read = fdopen(fds[0], "r");

      // If the daemon initialization went well, it will send 0 through a
      // pipe.
      // If not, it will send non-0 and then the error message on a new line.
      char buf[10];
      std::fgets(buf, sizeof(buf), pipe_read);
      int exit_code = std::stoi(buf);
      if (exit_code != EXIT_SUCCESS) {
        std::string error_message;
        while (std::fgets(buf, sizeof(buf), pipe_read) != NULL) {
          error_message += buf;
        }
        error_message.pop_back(); // remove trailing newline
        fclose(pipe_read);
        throw std::runtime_error(error_message);
      } else {
        fclose(pipe_read);
      }

      return pid;
  }
}

int main(int argc, char** argv) {
  arguments args;
  argp_parse(&global_argp, argc, argv, ARGP_IN_ORDER, 0, &args);

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
        dinfo->cert_port = args.cert_port;
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
                  << "\tcert_port: " << Config::Get("cert_port")
                  << '\n'
                  << "\tpassphrase: " << Config::Get("passphrase") << '\n'
                  << "\tcert_file: " << Config::Get("cert_file") << "\n";

        return EXIT_SUCCESS;
      } catch (const lrm::daemon_init_error&) {
        std::cerr << "Daemon not running. Use 'daemon' command.\n";
        return EXIT_FAILURE;
      } catch (const std::system_error& err) {
        std::cerr << "System error (" << err.code() << "): "
                  << err.what() << '\n';
        return EXIT_FAILURE;
      } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << '\n';
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

  DaemonResponse response;
  response.ParseFromFileDescriptor(socket.native_handle());

  if (not response.response().empty()) {
    std::cout << response.response() << '\n';
  }

  return response.exit_status();
}
