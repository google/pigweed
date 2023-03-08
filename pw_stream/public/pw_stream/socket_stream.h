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
#pragma once

#include <netinet/in.h>

#include <cstdint>

#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_stream/stream.h"

namespace pw::stream {

class SocketStream : public NonSeekableReaderWriter {
 public:
  constexpr SocketStream() = default;

  // SocketStream objects are moveable but not copyable.
  SocketStream& operator=(SocketStream&& other) {
    listen_port_ = other.listen_port_;
    socket_fd_ = other.socket_fd_;
    other.socket_fd_ = kInvalidFd;
    connection_fd_ = other.connection_fd_;
    other.connection_fd_ = kInvalidFd;
    sockaddr_client_ = other.sockaddr_client_;
    return *this;
  }
  SocketStream(SocketStream&& other) noexcept
      : listen_port_(other.listen_port_),
        socket_fd_(other.socket_fd_),
        connection_fd_(other.connection_fd_),
        sockaddr_client_(other.sockaddr_client_) {
    other.socket_fd_ = kInvalidFd;
    other.connection_fd_ = kInvalidFd;
  }
  SocketStream(const SocketStream&) = delete;
  SocketStream& operator=(const SocketStream&) = delete;

  ~SocketStream() override { Close(); }

  // Listen to the port and return after a client is connected
  //
  // DEPRECATED: Use the ServerSocket class instead.
  // TODO(b/271323032): Remove when this method is no longer used.
  Status Serve(uint16_t port);

  // Connect to a local or remote endpoint. Host may be either an IPv4 or IPv6
  // address. If host is nullptr then the IPv4 localhost address is used
  // instead.
  Status Connect(const char* host, uint16_t port);

  // Close the socket stream and release all resources
  void Close();

  // Exposes the file descriptor for the active connection. This is exposed to
  // allow configuration and introspection of this socket's current
  // configuration using setsockopt() and getsockopt().
  //
  // Returns -1 if there is no active connection.
  int connection_fd() { return connection_fd_; }

 private:
  friend class ServerSocket;

  static constexpr int kInvalidFd = -1;

  Status DoWrite(span<const std::byte> data) override;

  StatusWithSize DoRead(ByteSpan dest) override;

  uint16_t listen_port_ = 0;
  int socket_fd_ = kInvalidFd;
  int connection_fd_ = kInvalidFd;
  struct sockaddr_in sockaddr_client_ = {};
};

/// `ServerSocket` wraps a POSIX-style server socket, producing a `SocketStream`
/// for each accepted client connection.
///
/// Call `Listen` to create the socket and start listening for connections.
/// Then call `Accept` any number of times to accept client connections.
class ServerSocket {
 public:
  ServerSocket() = default;
  ~ServerSocket() { Close(); }

  ServerSocket(const ServerSocket& other) = delete;
  ServerSocket& operator=(const ServerSocket& other) = delete;

  // Listen for connections on the given port.
  // If port is 0, a random unused port is chosen and can be retrieved with
  // port().
  Status Listen(uint16_t port = 0);

  // Accept a connection. Blocks until after a client is connected.
  // On success, returns a SocketStream connected to the new client.
  Result<SocketStream> Accept();

  // Close the server socket, preventing further connections.
  void Close();

  // Returns the port this socket is listening on.
  uint16_t port() const { return port_; }

 private:
  static constexpr int kInvalidFd = -1;

  uint16_t port_ = -1;
  int socket_fd_ = kInvalidFd;
};

}  // namespace pw::stream
