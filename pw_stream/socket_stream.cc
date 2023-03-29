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

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_string/to_string.h"

namespace pw::stream {
namespace {

constexpr uint32_t kServerBacklogLength = 1;
constexpr const char* kLocalhostAddress = "127.0.0.1";

// Set necessary options on a socket file descriptor.
void ConfigureSocket([[maybe_unused]] int socket) {
#if defined(__APPLE__)
  // Use SO_NOSIGPIPE to avoid getting a SIGPIPE signal when the remote peer
  // drops the connection. This is supported on macOS only.
  constexpr int value = 1;
  if (setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(int)) < 0) {
    PW_LOG_WARN("Failed to set SO_NOSIGPIPE: %s", std::strerror(errno));
  }
#endif  // defined(__APPLE__)
}

}  // namespace

// TODO(b/240982565): Implement SocketStream for Windows.

// Listen to the port and return after a client is connected
Status SocketStream::Serve(uint16_t port) {
  listen_port_ = port;
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ == kInvalidFd) {
    PW_LOG_ERROR("Failed to create socket: %s", std::strerror(errno));
    return Status::Unknown();
  }

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(listen_port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  // Configure the socket to allow reusing the address. Closing a socket does
  // not immediately close it. Instead, the socket waits for some period of time
  // before it is actually closed. Setting SO_REUSEADDR allows this socket to
  // bind to an address that may still be in use by a recently closed socket.
  // Without this option, running a program multiple times in series may fail
  // unexpectedly.
  constexpr int value = 1;

  if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) <
      0) {
    PW_LOG_WARN("Failed to set SO_REUSEADDR: %s", std::strerror(errno));
  }

  if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    PW_LOG_ERROR("Failed to bind socket to localhost:%hu: %s",
                 listen_port_,
                 std::strerror(errno));
    return Status::Unknown();
  }

  if (listen(socket_fd_, kServerBacklogLength) < 0) {
    PW_LOG_ERROR("Failed to listen to socket: %s", std::strerror(errno));
    return Status::Unknown();
  }

  socklen_t len = sizeof(sockaddr_client_);

  connection_fd_ =
      accept(socket_fd_, reinterpret_cast<sockaddr*>(&sockaddr_client_), &len);
  if (connection_fd_ < 0) {
    return Status::Unknown();
  }
  ConfigureSocket(connection_fd_);
  return OkStatus();
}

Status SocketStream::SocketStream::Connect(const char* host, uint16_t port) {
  if (host == nullptr || std::strcmp(host, "localhost") == 0) {
    host = kLocalhostAddress;
  }

  struct addrinfo hints = {};
  struct addrinfo* res;
  char port_buffer[6];
  PW_CHECK(ToString(port, port_buffer).ok());
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | AI_PASSIVE;
  if (getaddrinfo(host, port_buffer, &hints, &res) != 0) {
    PW_LOG_ERROR("Failed to configure connection address for socket");
    return Status::InvalidArgument();
  }

  connection_fd_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  ConfigureSocket(connection_fd_);
  if (connect(connection_fd_, res->ai_addr, res->ai_addrlen) < 0) {
    close(connection_fd_);
    connection_fd_ = kInvalidFd;
  }
  freeaddrinfo(res);

  if (connection_fd_ == kInvalidFd) {
    PW_LOG_ERROR(
        "Failed to connect to %s:%d: %s", host, port, std::strerror(errno));
    return Status::Unknown();
  }

  return OkStatus();
}

void SocketStream::Close() {
  if (socket_fd_ != kInvalidFd) {
    close(socket_fd_);
    socket_fd_ = kInvalidFd;
  }

  if (connection_fd_ != kInvalidFd) {
    close(connection_fd_);
    connection_fd_ = kInvalidFd;
  }
}

Status SocketStream::DoWrite(span<const std::byte> data) {
  int send_flags = 0;
#if defined(__linux__)
  // Use MSG_NOSIGNAL to avoid getting a SIGPIPE signal when the remote
  // peer drops the connection. This is supported on Linux only.
  send_flags |= MSG_NOSIGNAL;
#endif  // defined(__linux__)

  ssize_t bytes_sent =
      send(connection_fd_, data.data(), data.size_bytes(), send_flags);

  if (bytes_sent < 0 || static_cast<size_t>(bytes_sent) != data.size()) {
    if (errno == EPIPE) {
      // An EPIPE indicates that the connection is closed.  Return an OutOfRange
      // error.
      return Status::OutOfRange();
    }

    return Status::Unknown();
  }
  return OkStatus();
}

StatusWithSize SocketStream::DoRead(ByteSpan dest) {
  ssize_t bytes_rcvd = recv(connection_fd_, dest.data(), dest.size_bytes(), 0);
  if (bytes_rcvd == 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // Socket timed out when trying to read.
      // This should only occur if SO_RCVTIMEO was configured to be nonzero, or
      // if the socket was opened with the O_NONBLOCK flag to prevent any
      // blocking when performing reads or writes.
      return StatusWithSize::ResourceExhausted();
    }
    // Remote peer has closed the connection.
    Close();
    return StatusWithSize::OutOfRange();
  } else if (bytes_rcvd < 0) {
    return StatusWithSize::Unknown();
  }
  return StatusWithSize(bytes_rcvd);
}

// Listen for connections on the given port.
// If port is 0, a random unused port is chosen and can be retrieved with
// port().
Status ServerSocket::Listen(uint16_t port) {
  socket_fd_ = socket(AF_INET6, SOCK_STREAM, 0);
  if (socket_fd_ == kInvalidFd) {
    return Status::Unknown();
  }

  // Allow binding to an address that may still be in use by a closed socket.
  constexpr int value = 1;
  setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int));

  if (port != 0) {
    struct sockaddr_in6 addr = {};
    socklen_t addr_len = sizeof(addr);
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
      return Status::Unknown();
    }
  }

  if (listen(socket_fd_, kServerBacklogLength) < 0) {
    return Status::Unknown();
  }

  // Find out which port the socket is listening on, and fill in port_.
  struct sockaddr_in6 addr = {};
  socklen_t addr_len = sizeof(addr);
  if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) <
          0 ||
      static_cast<size_t>(addr_len) > sizeof(addr)) {
    close(socket_fd_);
    return Status::Unknown();
  }

  port_ = ntohs(addr.sin6_port);

  return OkStatus();
}

// Accept a connection. Blocks until after a client is connected.
// On success, returns a SocketStream connected to the new client.
Result<SocketStream> ServerSocket::Accept() {
  struct sockaddr_in6 sockaddr_client_ = {};
  socklen_t len = sizeof(sockaddr_client_);

  int connection_fd =
      accept(socket_fd_, reinterpret_cast<sockaddr*>(&sockaddr_client_), &len);
  if (connection_fd == kInvalidFd) {
    return Status::Unknown();
  }
  ConfigureSocket(connection_fd);

  SocketStream client_stream;
  client_stream.connection_fd_ = connection_fd;
  return client_stream;
}

// Close the server socket, preventing further connections.
void ServerSocket::Close() {
  if (socket_fd_ != kInvalidFd) {
    close(socket_fd_);
    socket_fd_ = kInvalidFd;
  }
}

}  // namespace pw::stream
