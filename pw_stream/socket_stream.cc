// Copyright 2020 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_stream/socket_stream.h"
namespace pw::stream {

static constexpr uint32_t kMaxConcurrentUser = 1;
static constexpr char kLocalhostAddress[] = "127.0.0.1";

SocketStream::~SocketStream() { Close(); }

// Listen to the port and return after a client is connected
Status SocketStream::Serve(uint16_t port) {
  listen_port_ = port;
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ == kInvalidFd) {
    return Status::Internal();
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(listen_port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  int result =
      bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  if (result < 0) {
    return Status::Internal();
  }

  result = listen(socket_fd_, kMaxConcurrentUser);
  if (result < 0) {
    return Status::Internal();
  }

  socklen_t len = sizeof(sockaddr_client_);

  conn_fd_ =
      accept(socket_fd_, reinterpret_cast<sockaddr*>(&sockaddr_client_), &len);
  if (conn_fd_ < 0) {
    return Status::Internal();
  }
  return OkStatus();
}

Status SocketStream::SocketStream::Connect(const char* host, uint16_t port) {
  conn_fd_ = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (host == nullptr) {
    host = kLocalhostAddress;
  }

  if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
    return Status::Unknown();
  }

  int result = connect(
      conn_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  if (result < 0) {
    return Status::Unknown();
  }

  return OkStatus();
}

void SocketStream::Close() {
  if (socket_fd_ != kInvalidFd) {
    close(socket_fd_);
    socket_fd_ = kInvalidFd;
  }

  if (conn_fd_ != kInvalidFd) {
    close(conn_fd_);
    conn_fd_ = kInvalidFd;
  }
}

Status SocketStream::DoWrite(std::span<const std::byte> data) {
  ssize_t bytes_sent = send(conn_fd_, data.data(), data.size_bytes(), 0);

  if (bytes_sent < 0 || static_cast<uint64_t>(bytes_sent) != data.size()) {
    return Status::Internal();
  }
  return OkStatus();
}

StatusWithSize SocketStream::DoRead(ByteSpan dest) {
  ssize_t bytes_rcvd = recv(conn_fd_, dest.data(), dest.size_bytes(), 0);
  if (bytes_rcvd < 0) {
    return StatusWithSize::Internal();
  }
  return StatusWithSize(bytes_rcvd);
}

};  // namespace pw::stream
