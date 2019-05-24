#include <algorithm>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <string_view>
#include <thread>
#include <future>
#include <mutex>

#include <argp.h>

#include "grpcpp/create_channel.h"

#include "Config.h"
#include "PlayerClient.h"
#include "Util.h"

const char* argp_program_version = "lrm-client 0.1";
const char* argp_program_bug_address = "<doesnt@exist.addr>";
static char doc[] =
    "Lelo Remote Music Control -- Client for Lelo Remote Music Player";

static argp_option options[] = {
  {"config", 'c', "FILE", 0, "Use an alternative config file"},
  {"host", 'h', "ADDRESS", 0, "Address of the gRPC server"},
  {"port", 'p', "NUM", 0, "Port for gRPC"},
  {"streaming-port", 's', "NUM", 0, "Port for streaming music"},
  {0}
};

struct arguments {
  std::string config_file;
  std::string grpc_host;
  std::string grpc_port;
  std::string streaming_port;
};

static error_t parse_opt(int key, char* arg, argp_state* state) {
  arguments* args = (arguments*)state->input;

  switch(key) {
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
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static argp argp = { options, parse_opt, 0, doc };

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
  auto grpc_port = Config::Get("grpc_port");
  if (grpc_port.empty()) {
    throw std::logic_error("Port for gRPC not provided.");
  } else if (int port = std::stoi(grpc_port);
             port <= IPPORT_USERRESERVED or port > USHRT_MAX) {
    throw std::logic_error("Port for gRPC (" + grpc_port + ") is invalid");
  }

  if (not args->grpc_host.empty()) {
    Config::Set("grpc_host", args->grpc_host);
  }
  if (Config::Get("grpc_host").empty()) {
    throw std::logic_error("Host for gRPC not provided.");
  }

  if (not args->streaming_port.empty()) {
    Config::Set("streaming_port", args->streaming_port);
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

  return 0;
}

using namespace grpc;
using namespace asio::ip;

class CallAuthenticator : public grpc::MetadataCredentialsPlugin {
  grpc::string passphrase_;

 public:
  CallAuthenticator(std::string_view passphrase) : passphrase_(passphrase) {}

  grpc::Status GetMetadata(
      grpc::string_ref service_url, grpc::string_ref method_name,
      const grpc::AuthContext& channel_auth_context,
      std::multimap<grpc::string, grpc::string>* metadata) override {
    metadata->insert(std::make_pair("x-custom-passphrase", passphrase_));

    return grpc::Status::OK;
  }
};

int main (int argc, char** argv) {
  arguments args;
  argp_parse(&argp, argc, argv, 0, 0, &args);

  initialize_config(&args);

  grpc::string grpc_address(Config::Get("grpc_host") + ':' +
                            Config::Get("grpc_port"));

  std::shared_ptr<grpc::ChannelCredentials> creds;
  {
    auto call_creds = grpc::MetadataCredentialsFromPlugin(
        std::make_unique<CallAuthenticator>(Config::Get("passphrase")));
    grpc::SslCredentialsOptions options;
    std::string ssl_cert = lrm::file_to_str("server.crt");
    options.pem_root_certs = ssl_cert;
    auto channel_creds = grpc::SslCredentials(options);

    creds = grpc::CompositeChannelCredentials(channel_creds, call_creds);
  }

  // TODO: check if this cast is safe
  auto remote = PlayerClient{Config::Get("streaming_port"),
                             grpc::CreateChannel(grpc_address, creds)};

  // remote.Play("/tmp/file.mp3");
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  remote.Play("/tmp/file.flac");
  // std::this_thread::sleep_for(std::chrono::seconds(10));
  // // remote.TogglePause();
  // // std::this_thread::sleep_for(std::chrono::seconds(1));
  // // remote.TogglePause();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  remote.Stop();
  // remote.TogglePause();
}
