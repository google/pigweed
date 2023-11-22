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
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

namespace pw::stream {

class SocketStream : public NonSeekableReaderWriter {
 public:
  SocketStream() = default;
  // Construct a SocketStream directly from a file descriptor.
  explicit SocketStream(int connection_fd) : connection_fd_(connection_fd) {
    // Mark as ready and take ownership of the connection by this object.
    ready_ = true;
    TakeConnection();
  }

  // SocketStream objects are moveable but not copyable.
  SocketStream& operator=(SocketStream&& other) {
    MoveFrom(std::move(other));
    return *this;
  }
  SocketStream(SocketStream&& other) noexcept { MoveFrom(std::move(other)); }
  SocketStream(const SocketStream&) = delete;
  SocketStream& operator=(const SocketStream&) = delete;

  ~SocketStream() override { Close(); }

  // Connect to a local or remote endpoint. Host may be either an IPv4 or IPv6
  // address. If host is nullptr then the IPv4 localhost address is used
  // instead.
  Status Connect(const char* host, uint16_t port);

  // Configures socket options.
  int SetSockOpt(int level,
                 int optname,
                 const void* optval,
                 unsigned int optlen);

  // Close the socket stream and release all resources
  void Close();

 private:
  static constexpr int kInvalidFd = -1;

  class ConnectionOwnership {
   public:
    explicit ConnectionOwnership(SocketStream* socket_stream)
        : socket_stream_(socket_stream) {
      fd_ = socket_stream_->TakeConnection();
      std::lock_guard lock(socket_stream_->connection_mutex_);
      pipe_r_fd_ = socket_stream->connection_pipe_r_fd_;
    }

    ~ConnectionOwnership() { socket_stream_->ReleaseConnection(); }

    int fd() { return fd_; }

    int pipe_r_fd() { return pipe_r_fd_; }

   private:
    SocketStream* socket_stream_;
    int fd_;
    int pipe_r_fd_;
  };

  Status DoWrite(span<const std::byte> data) override;

  StatusWithSize DoRead(ByteSpan dest) override;

  // Take ownership of the connection. There may be multiple owners. Each time
  // TakeConnection is called, ReleaseConnection must be called to release
  // ownership, even if the connection is not valid.
  //
  // Returns the connection fd or kInvalidFd if the connection is not valid.
  int TakeConnection();
  int TakeConnectionWithLockHeld()
      PW_EXCLUSIVE_LOCKS_REQUIRED(connection_mutex_);

  // Release ownership of the connection. If no owners remain, close and clear
  // the connection fds.
  void ReleaseConnection();
  void ReleaseConnectionWithLockHeld()
      PW_EXCLUSIVE_LOCKS_REQUIRED(connection_mutex_);

  // Moves other to this.
  void MoveFrom(SocketStream&& other) {
    std::lock_guard lock(connection_mutex_);
    std::lock_guard other_lock(other.connection_mutex_);

    connection_own_count_ = other.connection_own_count_;
    other.connection_own_count_ = 0;
    ready_ = other.ready_;
    other.ready_ = false;
    connection_fd_ = other.connection_fd_;
    other.connection_fd_ = kInvalidFd;
    connection_pipe_r_fd_ = other.connection_pipe_r_fd_;
    other.connection_pipe_r_fd_ = kInvalidFd;
    connection_pipe_w_fd_ = other.connection_pipe_w_fd_;
    other.connection_pipe_w_fd_ = kInvalidFd;
  }

  sync::Mutex connection_mutex_;
  int connection_own_count_ PW_GUARDED_BY(connection_mutex_) = 0;
  bool ready_ PW_GUARDED_BY(connection_mutex_) = false;
  int connection_fd_ PW_GUARDED_BY(connection_mutex_) = kInvalidFd;
  int connection_pipe_r_fd_ PW_GUARDED_BY(connection_mutex_) = kInvalidFd;
  int connection_pipe_w_fd_ PW_GUARDED_BY(connection_mutex_) = kInvalidFd;
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

  class SocketOwnership {
   public:
    explicit SocketOwnership(ServerSocket* server_socket)
        : server_socket_(server_socket) {
      fd_ = server_socket_->TakeSocket();
      std::lock_guard lock(server_socket->socket_mutex_);
      pipe_r_fd_ = server_socket->socket_pipe_r_fd_;
    }

    ~SocketOwnership() { server_socket_->ReleaseSocket(); }

    int fd() { return fd_; }

    int pipe_r_fd() { return pipe_r_fd_; }

   private:
    ServerSocket* server_socket_;
    int fd_;
    int pipe_r_fd_;
  };

  // Take ownership of the socket. There may be multiple owners. Each time
  // TakeSocket is called, ReleaseSocket must be called to release ownership,
  // even if the socket is not invalid.
  //
  // Returns the socket fd or kInvalidFd if the socket is not valid.
  int TakeSocket();
  int TakeSocketWithLockHeld() PW_EXCLUSIVE_LOCKS_REQUIRED(socket_mutex_);

  // Release ownership of the socket. If no owners remain, close and clear the
  // socket fds.
  void ReleaseSocket();
  void ReleaseSocketWithLockHeld() PW_EXCLUSIVE_LOCKS_REQUIRED(socket_mutex_);

  uint16_t port_ = -1;
  sync::Mutex socket_mutex_;
  int socket_own_count_ PW_GUARDED_BY(socket_mutex_) = 0;
  bool ready_ PW_GUARDED_BY(socket_mutex_) = false;
  int socket_fd_ PW_GUARDED_BY(socket_mutex_) = kInvalidFd;
  int socket_pipe_r_fd_ PW_GUARDED_BY(socket_mutex_) = kInvalidFd;
  int socket_pipe_w_fd_ PW_GUARDED_BY(socket_mutex_) = kInvalidFd;
};

}  // namespace pw::stream
