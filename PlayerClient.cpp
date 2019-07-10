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

#include "grpcpp/channel.h"

#include "spdlog/spdlog.h"

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

int PlayerClient::start_streaming(const std::string& filename,
                                  unsigned short port) {
  streaming_acceptor_.close();
  streaming_socket_.close();

  streaming_endpoint_.port(port);
  streaming_acceptor_ = tcp::acceptor(context_, streaming_endpoint_);

  port = streaming_acceptor_.local_endpoint().port();
  log_->debug("Opening port {}", port);

  log_->debug("Reading the file to a vector...");
  streaming_file_ = read_file(filename);

  log_->debug("Waiting for the server...");
  std::thread([this](){
                try {
                  streaming_socket_ = streaming_acceptor_.accept();

                  log_->info("Sending the file on port {}...",
                             streaming_socket_.local_endpoint().port());
                  size_t sent = asio::write(streaming_socket_,
                                            asio::buffer(streaming_file_));
                  log_->debug("File uploaded. Bytes sent: {}", sent);
                } catch (const asio::system_error& e) {
                  if (e.code().value() != EBADF) {
                    log_->error("ASIO error while uploading the file: {}",
                                e.what());
                  }
                }
              }).detach();

  return port;
}

PlayerClient::PlayerClient(std::shared_ptr<grpc_impl::Channel> channel)
    : stub_(PlayerService::NewStub(channel)),
      streaming_acceptor_(context_),
      streaming_socket_(context_),
      log_(spdlog::get("PlayerClient")) {}

PlayerClient::PlayerClient(unsigned short streaming_port,
                           std::shared_ptr<Channel> channel)
    noexcept
    : PlayerClient(channel) {
  set_port(streaming_port);
}

PlayerClient::PlayerClient(const std::string& streaming_port,
                           std::shared_ptr<Channel> channel)
    noexcept
    : PlayerClient(channel) {
  int port;

  try {
    port = std::stoi(streaming_port.data());
    if (port > USHRT_MAX) throw;
  } catch (...) {
    port = 0;
  }
  port_.set_port(port);
}

PlayerClient::~PlayerClient() noexcept {
  try {
    log_->flush();
  } catch (...) {}
}

unsigned short PlayerClient::set_port(unsigned short port) {
  if (not (port <= IPPORT_USERRESERVED and port != 0)) {
    port = 0;
  }
  port_.set_port(port);
  return port;
}

int PlayerClient::Play(std::string_view filename)
{
  log_->debug("PlayerClient::Play(\"{}\")", filename);

  try {
    unsigned short port = start_streaming(filename.data(), port_.port());
    port_.set_port(port);
  } catch (const std::exception& e) {
    // TODO: Rethrow
    log_->error("Error while trying to open a stream: {}", e.what());
    return -101;
  }

  ClientContext context;
  MpvResponse response;
  grpc::Status status = stub_->PlayFrom(&context, port_, &response);

  if (status.ok()) {
    // stream_socket_ = stream_acceptor_.accept(io_context_);
    // stream_socket_.async_send(asio::buffer(bytes), handle_write);

    return response.response();
  } else {
    throw status;
  }
}

int PlayerClient::Stop() {
  log_->debug("PlayerClient::Stop()");

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
  log_->debug("PlayerClient::TogglePause()");

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
  log_->debug("PlayerClient::Volume(\"{}\")", volume);

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
  log_->debug("PlayerClient::Ping()");

  ClientContext context;
  Empty empty;

  grpc::Status status = stub_->Ping(&context, empty, &empty);
  if (status.ok()) {
    return status.ok();
  } else {
    throw status;
  }
}
