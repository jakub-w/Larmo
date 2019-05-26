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

int PlayerClient::wait_for_port(int pipe_fd, unsigned int timeout) {
  // if (-1 == fcntl(pipe_fd, F_SETFD, O_NONBLOCK | fcntl(pipe_fd, F_GETFD))) {
    // std::logic_error(std::string("In wait_for_port: ") + strerror(errno));
  // }

  struct timeval time = {timeout / 1000,  // tv_sec
                         (timeout % 1000) * 1000};  // tv_usec

  FILE* port_stream;
  if (port_stream = fdopen(pipe_fd, "r");
      nullptr == port_stream) {
    throw std::runtime_error("In wait_for_port: Couldn't open the file "
                             "descriptor");
  }

  fd_set set;
  FD_ZERO(&set);
  FD_SET(pipe_fd, &set);

  // std::cout << "Calling select...\n";
  int select_result = TEMP_FAILURE_RETRY(
      select(FD_SETSIZE, &set, nullptr, nullptr, &time));
  // std::cout << "Select returned with: " << select_result << ".\n";

  switch (select_result) {
    case 1:
      char port_buffer[10]; // max port number is 5-digit
      // std::cout << "fgets...\n";
      fgets(port_buffer, 10, port_stream);
      // std::cout << "fgets returns: " << port_buffer << '\n';
      fclose(port_stream);
      return (unsigned short) std::stoi(port_buffer);
    case 0:
      throw std::runtime_error("In wait_for_port: Streaming process didn't "
                               "respond. Timeout.");
      return 0;
    case EBADF:
      throw std::invalid_argument("In wait_for_port: The pipe_fd argument is "
                                  "invalid");
    case EINVAL:
      throw std::invalid_argument("In wait_for_port: The timeout argument is "
                                  "invalid");
    default:
      throw std::logic_error("In wait_for_port: This shouldn't ever run.");
  }

  return 0;
}

// This is a dangerous function. It creates a fork of the process and gives
// it it's own sid. It's sole purpose is to send a file over the port.
// Everything that's inside a child proces is protected by try-catch block's
// scope. The std::_Exit() is called after the cleanup when exiting the
// try-catch scope, so everything it allocated, it deallocates.
// std::_Exit() ensures that the child process doesn't touch anything created
// by the parent.
int PlayerClient::stream_file(const std::string filename,
                              const unsigned short port) {
  // std::cout << "Forking the process...\n";
  int port_pipe[2];

  if (0 != pipe(port_pipe)) {
    throw std::runtime_error("In stream_file: Pipe couldn't be created");
  }

  pid_t pid = fork();
  if (pid < 0) {
    throw std::runtime_error(std::string("In stream_file: "
                                         "Couldn't stream a file: ") +
                             strerror(errno));
  } else if (pid == 0) {  // child
    int exit_code = EXIT_SUCCESS;
    close(port_pipe[0]);

    {
      std::ofstream log("/tmp/lrm.log", std::ios::app);
      std::unitbuf(log);

      // This file will exist as long as this child process is running.
      TempFile pid_file;
      try {
        pid_t sid = setsid();
        if (sid < 0) {
          throw std::runtime_error("Couldn't set sid for the child process.");
        }

        log << "sid: " << sid << '\n';

        try {
          pid_file.Create(STREAMING_PID_FILE);
        } catch (const std::filesystem::filesystem_error& e) {
          // This means that another streaming process is running
          if (e.code() == std::errc::file_exists) {
            std::ifstream old_pid_file(pid_file.Path());
            pid_t old_pid;
            old_pid_file >> old_pid;
            kill(old_pid, SIGINT);

            // Wait for the process to be terminated.
            while (0 == kill(old_pid, 0)) {
              std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            // If this doesn't succeed, just let the throw do its thing.
            pid_file.Create(STREAMING_PID_FILE);
          } else {
            throw e;
          }
        }
        // sid is guaranteed to be the same as pid
        pid_file.Stream() << sid << std::flush;

        log << "Saved pid in a temp file\n";

        asio::io_context context;
        log << "Creating signal_set...\n";
        asio::signal_set signal(context, SIGINT, SIGTERM);
        signal.async_wait([&context, &log]
                          (const asio::error_code& error, int signal_number){
                            log << "End streaming by SignalHandler.\n";
                            context.stop();
                          });
        log << "signal_set created.\n";

        tcp::acceptor acceptor(context, tcp::endpoint(tcp::v4(), port));

        // Send the port number to a parent process
        {
          FILE* port_stream;
          if (port_stream = fdopen(port_pipe[1], "w");
              nullptr == port_stream) {
            throw std::runtime_error("Couldn't open the pipe for writing.");
          }

          if ((0 > fprintf(port_stream, "%hu\n",
                           acceptor.local_endpoint().port()))
              or
              (0 != fclose(port_stream))) {
            throw std::runtime_error("Couldn't write to a pipe.");
          }
        }

        std::vector<char> bytes = read_file(filename);
        log << "Read a music file.\n";

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
    // std::cout << "Child pid: " << pid << '\n';

    close(port_pipe[1]);

    return wait_for_port(port_pipe[0]);
  }
}

PlayerClient::PlayerClient(unsigned short streaming_port,
                           std::shared_ptr<Channel> channel)
    : stub_(PlayerService::NewStub(channel)) {
  if (streaming_port <= IPPORT_USERRESERVED and streaming_port != 0) {
    throw std::invalid_argument(
        "PlayerClient: Invalid 'streaming_port' argument: " +
        std::to_string(streaming_port));
  }
  port_.set_port(streaming_port);
}

PlayerClient::PlayerClient(const std::string& streaming_port,
                           std::shared_ptr<Channel> channel)
    : stub_(PlayerService::NewStub(channel)) {
  int port;

  try {
    port = std::stoi(streaming_port.data());
    if ((port <= IPPORT_USERRESERVED or port > USHRT_MAX)
        and port != 0) {
      throw;
    }
  } catch (...) {
    throw std::invalid_argument(
        "PlayerClient: Invalid 'streaming_port' argument: " + streaming_port);
  }

  port_.set_port(port);
}

int PlayerClient::Play(std::string_view filename) {
  // std::cout << "Opening a stream.\n";
  try {
    unsigned short port = stream_file(filename.data(), port_.port());
    port_.set_port(port);
  } catch (const std::exception& e) {
    // TODO: Rethrow
    std::cerr << "Error while trying to open a stream: "
              << e.what() << '\n';
    return -101;
  }

  ClientContext context;
  MpvResponse response;
  // std::cout << "Sending an RPC request.\n";
  grpc::Status status = stub_->PlayFrom(&context, port_, &response);

  if (status.ok()) {
    // std::cout << "Status: ok\n";
    // stream_socket_ = stream_acceptor_.accept(io_context_);
    // stream_socket_.async_send(asio::buffer(bytes), handle_write);

    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::Stop() {
  ClientContext context;
  MpvResponse response;

  grpc::Status status = stub_->Stop(&context, Empty(), &response);
  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::TogglePause() {
  ClientContext context;
  MpvResponse response;

  grpc::Status status = stub_->TogglePause(&context, Empty(), &response);
  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::Volume(std::string_view volume) {
  ClientContext context;
  MpvResponse response;

  VolumeMessage vol_msg;
  vol_msg.set_volume(volume.data());

  grpc::Status status = stub_->Volume(&context, vol_msg, &response);
  if (status.ok()) {
    return response.response();
  } else {
    throw status;
  }
}

bool PlayerClient::Ping() {
  ClientContext context;
  Empty empty;

  grpc::Status status = stub_->Ping(&context, empty, &empty);

  return status.ok();
}
