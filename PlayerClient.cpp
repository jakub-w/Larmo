#include "PlayerClient.h"

#include <filesystem>
#include <fstream>

#include <mpv/client.h>

#include "grpcpp/channel.h"

#include "TempFile.h"

std::vector<char> PlayerClient::read_file(std::string_view filename) {
  std::ifstream ifs(filename.data(), std::ios::binary | std::ios::in);
  if (not ifs.is_open()) {
    throw std::invalid_argument(
        std::string("Couldn't open the file: ") + filename.data());
  }
  ifs.seekg(0, std::ios::end);
  size_t length = ifs.tellg();

  ifs.seekg(0, std::ios::beg);

  std::vector<char> output(length);
  ifs.read(output.data(), length);
  ifs.close();

  return output;
}

// This is a dangerous function. It creates a fork of the process and gives
// it it's own sid. It's sole purpose is to send a file over the port.
// Everything that's inside a child proces is protected by try-catch block's
// scope. The std::_Exit() is called after the cleanup when exiting the
// try-catch scope, so everything it allocated, it deallocates.
// std::_Exit() ensures that the child process doesn't touch anything created
// by the parent.
void PlayerClient::stream_file(const std::string filename,
                               const unsigned short port) {
  std::cout << "Forking the process...\n";
  pid_t pid = fork();
  if (pid < 0) {
    throw std::runtime_error(std::string("Couldn't stream a file: ") +
                             strerror(errno));
  } else if (pid == 0) {  // child
    int exit_code = EXIT_SUCCESS;

    {
      std::ofstream log("/tmp/lrm.log");
      std::unitbuf(log);

      // This file will exist as long as this child process is running.
      TempFile pid_file;
      try {
        pid_t sid = setsid();
        if (sid < 0) {
          throw std::runtime_error("Couldn't set sid for the child process.");
        }

        log << "sid: " << sid << '\n';

        pid_file.Create("lrm.pid");
        pid_file.Stream() << sid;  // sid is guaranteed to be the same as pid

        log << "Saved pid in a temp file\n";

        std::vector<char> bytes = read_file(filename);
        log << "Read a music file.\n";

        asio::io_context context;
        log << "Creating signal_set...\n";
        asio::signal_set signal(context, SIGINT);
        signal.async_wait([&context, &log]
                          (const asio::error_code& error, int signal_number){
                            log << "End streaming by SignalHandler.\n";
                            context.stop();
                          });
        log << "signal_set created.\n";

        tcp::acceptor acceptor(context, tcp::endpoint(tcp::v4(), port));
        log << "Waiting for a server to connect...\n";
        tcp::socket socket = acceptor.accept(context);
        log << "Server connected. Sending the file...\n";

        asio::async_write(socket, asio::buffer(bytes),
                          [&context, &log](const asio::error_code& error,
                                           size_t transferred){
                            if (!error) {
                              log << "File uploaded. Bytes sent: "
                                  << transferred << '\n';
                            } else {
                              log << "ASIO error while uploading the file: "
                                  << error.message() << '\n';
                            }
                            context.stop();
                          });

        context.run();

      } catch (const std::exception& e) {
        log << "Error: " << e.what() << '\n';
        exit_code = EXIT_FAILURE;
      } catch (const asio::system_error& e) {
        log << "ASIO error: " << e.what() << '\n';
      } catch (...) {
        exit_code = EXIT_FAILURE;
      }

      log << "Exiting.\n";
    }

    std::_Exit(exit_code);
  } else {  // parent
    std::cout << "Child pid: " << pid << '\n';
    return;
  }
}

PlayerClient::PlayerClient(int streaming_port,
                           std::shared_ptr<Channel> channel)
    : stub_(PlayerService::NewStub(channel))
      // stream_acceptor_(io_context_,
      //                  tcp::endpoint(tcp::v4(), streaming_port))
{
  // tcp::endopoint's constructor will randomize the port if it's 0
  // that's why we take it from there, and not from this constructor's arg
  // port_.set_port(stream_acceptor_.local_endpoint().port());
  port_.set_port(streaming_port);
  std::cout << "Port in a constructor: " << port_.port() << '\n';
}

int PlayerClient::Play(std::string_view filename) {
  // std::vector<char> bytes;
  // try {
    // bytes = read_file(filename);
  // } catch (const std::invalid_argument& e) {
    // std::cerr << e.what() << '\n';
    // return MPV_ERROR_NOTHING_TO_PLAY;
  // }

  // if (stream_socket_.is_open()) {
    // stream_socket_.cancel();
    // stream_socket_.close();
  // }

  stream_file(filename.data(), port_.port());

  ClientContext context;
  MpvResponse response;
  grpc::Status status = stub_->PlayFrom(&context, port_, &response);

  if (status.ok()) {
    std::cout << "Status: ok\n";
    // stream_socket_ = stream_acceptor_.accept(io_context_);
    // stream_socket_.async_send(asio::buffer(bytes), handle_write);

    return response.response();
  } else {
    std::cerr << "gRPC error: " << status.error_message() << " ("
              << status.error_details() << ')' << '\n';
    return -100;
  }
}

int PlayerClient::Stop() {
  ClientContext context;
  MpvResponse response;

  grpc::Status status = stub_->Stop(&context, Empty(), &response);
  return response.response();
}

int PlayerClient::TogglePause() {
  ClientContext context;
  MpvResponse response;

  grpc::Status status = stub_->TogglePause(&context, Empty(), &response);
  return response.response();
}
