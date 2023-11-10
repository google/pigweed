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

#include <cstdint>

#include "pw_result/result.h"
#include "pw_span/span.h"
#include "pw_stream/stream.h"
#include "pw_sync/mutex.h"

namespace pw::stream {

class SocketStream : public NonSeekableReaderWriter {
 public:
  SocketStream() = default;
  // Construct a SocketStream directly from a file descriptor.
  explicit SocketStream(int connection_fd) : connection_fd_(connection_fd) {
    // Take ownership of the connection fd by this object.
    TakeConnectionFd();
  }

  // SocketStream objects are moveable but not copyable.
  SocketStream& operator=(SocketStream&& other) {
    connection_fd_ = other.connection_fd_;
    other.connection_fd_ = kInvalidFd;
    connection_fd_own_count_ = other.connection_fd_own_count_;
    other.connection_fd_own_count_ = 0;
    return *this;
  }
  SocketStream(SocketStream&& other) noexcept
      : connection_fd_(other.connection_fd_) {
    other.connection_fd_ = kInvalidFd;
    connection_fd_own_count_ = other.connection_fd_own_count_;
    other.connection_fd_own_count_ = 0;
  }
  SocketStream(const SocketStream&) = delete;
  SocketStream& operator=(const SocketStream&) = delete;

  ~SocketStream() override { Close(); }

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

  // Take ownership of the connection fd. There may be multiple owners. Each
  // time TakeConnectionFd is called, ReleaseConnectionFd must be called to
  // release ownership, even if the connection fd is invalid.
  //
  // Returns the connection fd.
  int TakeConnectionFd();

  // Release ownership of the connection fd. If no owners remain, close and
  // clear the connection fd.
  void ReleaseConnectionFd();

  sync::Mutex connection_fd_mutex_;
  int connection_fd_ = kInvalidFd;
  int connection_fd_own_count_ = 0;
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

  // Take ownership of the socket fd. There may be multiple owners. Each time
  // TakeSocketFd is called, ReleaseReleaseFd must be called to release
  // ownership, even if the socket fd is invalid.
  //
  // Returns the socket fd.
  int TakeSocketFd();

  // Release ownership of the socket fd. If no owners remain, close and clear
  // the socket fd.
  void ReleaseSocketFd();

  uint16_t port_ = -1;
  sync::Mutex socket_fd_mutex_;
  int socket_fd_ = kInvalidFd;
  int socket_fd_own_count_ = 0;
};

}  // namespace pw::stream
