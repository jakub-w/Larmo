#ifndef LRM_REMOTECONTROLARGP_H_
#define LRM_REMOTECONTROLARGP_H_

#include <cstring>
#include <unordered_map>
#include <string>

#include <argp.h>

#include "Util.h"

const char* argp_program_version = "lrm-client 0.1";
const char* argp_program_bug_address = "<doesnt@exist.addr>";
static char doc[] =
    "Lelo Remote Music Control -- Client for Lelo Remote Music Player\v"
    "Commands:\n"
    "  daemon\t\t" "Start a daemon\n"
    "  info FORMAT\t\t" "Print an info about the currently playing file\n"
    "  ping\t\t\t" "Ping the server\n"
    "  play FILE\t\t" "Play the FILE\n"
    "  seek SECONDS\t\t" "Seek forward or backward in the playing file (unreliable)\n"
    "  stop\t\t\t" "Stop the playback\n"
    "  toggle-pause\t\t" "Pause or unpause the playback\n"
    "  volume VOL\t\t" "Absolute (e.g. 50) or relative (e.g. +10)\n"
    "\nDAEMON\n"
    "  For more info about creating a daemon invoke 'daemon --help'.";

static argp_option global_options[] = {
  {0, 0, 0, 0, 0, 0}
};

struct arguments {
  std::string command;
  std::string command_arg;
  std::string config_file;
  std::string grpc_host;
  std::string grpc_port;
  std::string streaming_port;
  std::string cert_port;
  std::string passphrase;
  std::string cert_file;
};

// bool value is true if the command requires an argument
static const std::unordered_map<std::string, bool> commands = {
  {"play", true},
  {"seek", true},
  {"stop", false},
  {"toggle-pause", false},
  {"volume", true},
  {"ping", false},
  {"daemon", false},
  {"info", true}
};

static const char args_doc[] = "COMMAND [ARG]";

static argp_option daemon_options[] = {
  {"config", 'c', "FILE", 0, "Use an alternative config file", 0},
  {"host", 'h', "ADDRESS", 0, "Address of the gRPC server", 0},
  {"port", 'p', "NUM", 0, "Port for gRPC", 0},
  {"streaming-port", 's', "NUM", 0, "Port for streaming music", 0},
  {"cert-port", 'r', "NUM", 0, "Port for the certificate exchange", 0},
  {"pass", 'P', "PASSPHRASE", 0, "Passphrase for queries to the server", 0},
  {0, 0, 0, 0, 0, 0}
};

static error_t daemon_parse_opt(int key, char* arg, argp_state* state) {
  arguments* args = (arguments*)state->input;

  switch(key) {
    case 'p': case 's': case 'r':
      try {
        int port = std::stoi(arg);
        if (port <= IPPORT_USERRESERVED or port > 65535) {
          throw std::invalid_argument("Invalid port.");
        }
        if ('p' == key) {
          args->grpc_port = std::to_string(port);
        } else if ('s' == key) {
          args->streaming_port = std::to_string(port);
        } else if ('r' == key) {
          args->cert_port = std::to_string(port);
        }
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
    case 'h':
      args->grpc_host = arg;
      break;
    case 'P':
      args->passphrase = arg;
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static argp daemon_argp = { daemon_options, daemon_parse_opt, 0, 0, 0, 0, 0};

static error_t global_parse_opt(int key, char* arg, argp_state* state) {
  arguments* args = static_cast<arguments*>(state->input);

  switch (key) {
    case ARGP_KEY_INIT:
      // Check if "volume" command is used, in that case check its argument
      // and if it's a negative number, replace '-' in front of it with 'm'.
      // Otherwise argp would interpret this argument as an option which
      // doesn't exist.
      for (auto i = 0; i < state->argc; ++i) {
        if ((std::strcmp(state->argv[i], "volume") == 0 or
             std::strcmp(state->argv[i], "seek") == 0) and
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
    case ARGP_KEY_ARG:
      if (commands.find(arg) == commands.end()) {
        // Parsing non-option args that are not commands
        if (not args->command_arg.empty()) {
          return ARGP_ERR_UNKNOWN;
        }
        if (args->command == "play") {
          if (lrm::file_exists(arg)) {
            args->command_arg = arg;
          } else {
            argp_error(state, "File doesn't exist: %s", arg);
          }
        } else if (args->command == "volume" or
                   args->command == "seek") {
          int length = std::strlen(arg);
          if (length > 1 and arg[0] == 'm') {
            arg[0] = '-';
          }
          args->command_arg = arg;
        } else if (args->command == "info") {
          args->command_arg = arg;
        } else {
          argp_error(state, "Unknown command: %s", arg);
          return EINVAL;
        }
      } else {
        args->command = arg;

        if (args->command == "daemon") {
          // Replace current arg in state->argv with argv[0] + current arg.
          // This is for showing help. By default it would just use "daemon"
          // without the program's name in front.
          // FIXME: Minor memory leak
          // When help is shown, the program exits not calling child_arg0's
          // destructor.
          char* old_arg = state->argv[state->next - 1];

          std::string child_arg0 = std::string(state->argv[0]) + " " +
                                   state->argv[state->next - 1];
          state->argv[state->next - 1] = child_arg0.data();

          argp_parse(&daemon_argp, state->argc - state->next + 1,
                     &state->argv[state->next - 1], 0, 0, args);

          // Revert changes to the current arg
          state->argv[state->next - 1] = old_arg;

          state->next = state->argc;
        }
      }
      break;
    case ARGP_KEY_NO_ARGS:
      argp_usage(state);
      return EINVAL;
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

static argp global_argp = { global_options, global_parse_opt, args_doc,
                            doc, 0, 0, 0 };

#endif  // LRM_REMOTECONTROLARGP_H_
