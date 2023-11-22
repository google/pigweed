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

#if defined(_WIN32) && _WIN32
#include <fcntl.h>
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define SHUT_RDWR SD_BOTH
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(_WIN32) && _WIN32

#include <cerrno>
#include <cstring>

#include "pw_assert/check.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_string/to_string.h"

namespace pw::stream {
namespace {

constexpr uint32_t kServerBacklogLength = 1;
constexpr const char* kLocalhostAddress = "localhost";

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

#if defined(_WIN32) && _WIN32
int close(SOCKET s) { return closesocket(s); }

ssize_t write(int fd, const void* buf, size_t count) {
  return _write(fd, buf, count);
}

int poll(struct pollfd* fds, unsigned int nfds, int timeout) {
  return WSAPoll(fds, nfds, timeout);
}

int pipe(int pipefd[2]) { return _pipe(pipefd, 256, O_BINARY); }

int setsockopt(
    int fd, int level, int optname, const void* optval, unsigned int optlen) {
  return setsockopt(static_cast<SOCKET>(fd),
                    level,
                    optname,
                    static_cast<const char*>(optval),
                    static_cast<int>(optlen));
}

class WinsockInitializer {
 public:
  WinsockInitializer() {
    WSADATA data = {};
    PW_CHECK_INT_EQ(
        WSAStartup(MAKEWORD(2, 2), &data), 0, "Failed to initialize winsock");
  }
  ~WinsockInitializer() {
    // TODO: b/301545011 - This currently fails, probably a cleanup race.
    WSACleanup();
  }
};

[[maybe_unused]] WinsockInitializer initializer;

#endif  // defined(_WIN32) && _WIN32

}  // namespace

Status SocketStream::SocketStream::Connect(const char* host, uint16_t port) {
  if (host == nullptr) {
    host = kLocalhostAddress;
  }

  struct addrinfo hints = {};
  struct addrinfo* res;
  char port_buffer[6];
  PW_CHECK(ToString(port, port_buffer).ok());
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_NUMERICSERV;
  if (getaddrinfo(host, port_buffer, &hints, &res) != 0) {
    PW_LOG_ERROR("Failed to configure connection address for socket");
    return Status::InvalidArgument();
  }

  struct addrinfo* rp;
  int connection_fd;
  for (rp = res; rp != nullptr; rp = rp->ai_next) {
    connection_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (connection_fd != kInvalidFd) {
      break;
    }
  }

  if (connection_fd == kInvalidFd) {
    PW_LOG_ERROR("Failed to create a socket: %s", std::strerror(errno));
    freeaddrinfo(res);
    return Status::Unknown();
  }

  ConfigureSocket(connection_fd);
  if (connect(connection_fd, rp->ai_addr, rp->ai_addrlen) == -1) {
    close(connection_fd);
    PW_LOG_ERROR(
        "Failed to connect to %s:%d: %s", host, port, std::strerror(errno));
    freeaddrinfo(res);
    return Status::Unknown();
  }

  // Mark as ready and take ownership of the connection by this object.
  {
    std::lock_guard lock(connection_mutex_);
    connection_fd_ = connection_fd;
    TakeConnectionWithLockHeld();
    ready_ = true;
  }

  freeaddrinfo(res);
  return OkStatus();
}

// Configures socket options.
int SocketStream::SetSockOpt(int level,
                             int optname,
                             const void* optval,
                             unsigned int optlen) {
  ConnectionOwnership ownership(this);
  if (ownership.fd() == kInvalidFd) {
    return EBADF;
  }
  return setsockopt(ownership.fd(), level, optname, optval, optlen);
}

void SocketStream::Close() {
  ConnectionOwnership ownership(this);
  {
    std::lock_guard lock(connection_mutex_);
    if (ready_) {
      // Shutdown the connection and send tear down notification to unblock any
      // waiters.
      if (connection_fd_ != kInvalidFd) {
        shutdown(connection_fd_, SHUT_RDWR);
      }
      if (connection_pipe_w_fd_ != kInvalidFd) {
        write(connection_pipe_w_fd_, "T", 1);
      }

      // Release ownership of the connection by this object and mark as no
      // longer ready.
      ReleaseConnectionWithLockHeld();
      ready_ = false;
    }
  }
}

Status SocketStream::DoWrite(span<const std::byte> data) {
  int send_flags = 0;
#if defined(__linux__)
  // Use MSG_NOSIGNAL to avoid getting a SIGPIPE signal when the remote
  // peer drops the connection. This is supported on Linux only.
  send_flags |= MSG_NOSIGNAL;
#endif  // defined(__linux__)

  ssize_t bytes_sent;
  {
    ConnectionOwnership ownership(this);
    if (ownership.fd() == kInvalidFd) {
      return Status::Unknown();
    }
    bytes_sent = send(ownership.fd(),
                      reinterpret_cast<const char*>(data.data()),
                      data.size_bytes(),
                      send_flags);
  }

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
  ConnectionOwnership ownership(this);
  if (ownership.fd() == kInvalidFd) {
    return StatusWithSize::Unknown();
  }

  // Wait for data to read or a tear down notification.
  pollfd fds_to_poll[2];
  fds_to_poll[0].fd = ownership.fd();
  fds_to_poll[0].events = POLLIN | POLLERR | POLLHUP;
  fds_to_poll[1].fd = ownership.pipe_r_fd();
  fds_to_poll[1].events = POLLIN;
  poll(fds_to_poll, 2, -1);
  if (!(fds_to_poll[0].revents & POLLIN)) {
    return StatusWithSize::Unknown();
  }

  ssize_t bytes_rcvd = recv(ownership.fd(),
                            reinterpret_cast<char*>(dest.data()),
                            dest.size_bytes(),
                            0);
  if (bytes_rcvd == 0) {
    // Remote peer has closed the connection.
    Close();
    return StatusWithSize::OutOfRange();
  } else if (bytes_rcvd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // Socket timed out when trying to read.
      // This should only occur if SO_RCVTIMEO was configured to be nonzero, or
      // if the socket was opened with the O_NONBLOCK flag to prevent any
      // blocking when performing reads or writes.
      return StatusWithSize::ResourceExhausted();
    }
    return StatusWithSize::Unknown();
  }
  return StatusWithSize(bytes_rcvd);
}

int SocketStream::TakeConnection() {
  std::lock_guard lock(connection_mutex_);
  return TakeConnectionWithLockHeld();
}

int SocketStream::TakeConnectionWithLockHeld() {
  ++connection_own_count_;

  if (ready_ && (connection_fd_ != kInvalidFd) &&
      (connection_pipe_r_fd_ == kInvalidFd)) {
    int fd_list[2];
    if (pipe(fd_list) >= 0) {
      connection_pipe_r_fd_ = fd_list[0];
      connection_pipe_w_fd_ = fd_list[1];
    }
  }

  if (!ready_ || (connection_pipe_r_fd_ == kInvalidFd) ||
      (connection_pipe_w_fd_ == kInvalidFd)) {
    return kInvalidFd;
  }
  return connection_fd_;
}

void SocketStream::ReleaseConnection() {
  std::lock_guard lock(connection_mutex_);
  ReleaseConnectionWithLockHeld();
}

void SocketStream::ReleaseConnectionWithLockHeld() {
  --connection_own_count_;

  if (connection_own_count_ <= 0) {
    ready_ = false;
    if (connection_fd_ != kInvalidFd) {
      close(connection_fd_);
      connection_fd_ = kInvalidFd;
    }
    if (connection_pipe_r_fd_ != kInvalidFd) {
      close(connection_pipe_r_fd_);
      connection_pipe_r_fd_ = kInvalidFd;
    }
    if (connection_pipe_w_fd_ != kInvalidFd) {
      close(connection_pipe_w_fd_);
      connection_pipe_w_fd_ = kInvalidFd;
    }
  }
}

// Listen for connections on the given port.
// If port is 0, a random unused port is chosen and can be retrieved with
// port().
Status ServerSocket::Listen(uint16_t port) {
  int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (socket_fd == kInvalidFd) {
    return Status::Unknown();
  }

  // Allow binding to an address that may still be in use by a closed socket.
  constexpr int value = 1;
  setsockopt(socket_fd,
             SOL_SOCKET,
             SO_REUSEADDR,
             reinterpret_cast<const char*>(&value),
             sizeof(int));

  if (port != 0) {
    struct sockaddr_in6 addr = {};
    socklen_t addr_len = sizeof(addr);
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
      close(socket_fd);
      return Status::Unknown();
    }
  }

  if (listen(socket_fd, kServerBacklogLength) < 0) {
    close(socket_fd);
    return Status::Unknown();
  }

  // Find out which port the socket is listening on, and fill in port_.
  struct sockaddr_in6 addr = {};
  socklen_t addr_len = sizeof(addr);
  if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) <
          0 ||
      static_cast<size_t>(addr_len) > sizeof(addr)) {
    close(socket_fd);
    return Status::Unknown();
  }

  port_ = ntohs(addr.sin6_port);

  // Mark as ready and take ownership of the socket by this object.
  {
    std::lock_guard lock(socket_mutex_);
    socket_fd_ = socket_fd;
    TakeSocketWithLockHeld();
    ready_ = true;
  }

  return OkStatus();
}

// Accept a connection. Blocks until after a client is connected.
// On success, returns a SocketStream connected to the new client.
Result<SocketStream> ServerSocket::Accept() {
  struct sockaddr_in6 sockaddr_client_ = {};
  socklen_t len = sizeof(sockaddr_client_);

  SocketOwnership ownership(this);
  if (ownership.fd() == kInvalidFd) {
    return Status::Unknown();
  }

  // Wait for a connection or a tear down notification.
  pollfd fds_to_poll[2];
  fds_to_poll[0].fd = ownership.fd();
  fds_to_poll[0].events = POLLIN | POLLERR | POLLHUP;
  fds_to_poll[1].fd = ownership.pipe_r_fd();
  fds_to_poll[1].events = POLLIN;
  int rv = poll(fds_to_poll, 2, -1);
  if ((rv <= 0) || !(fds_to_poll[0].revents & POLLIN)) {
    return Status::Unknown();
  }

  int connection_fd = accept(
      ownership.fd(), reinterpret_cast<sockaddr*>(&sockaddr_client_), &len);
  if (connection_fd == kInvalidFd) {
    return Status::Unknown();
  }
  ConfigureSocket(connection_fd);

  return SocketStream(connection_fd);
}

// Close the server socket, preventing further connections.
void ServerSocket::Close() {
  SocketOwnership ownership(this);
  {
    std::lock_guard lock(socket_mutex_);
    if (ready_) {
      // Shutdown the socket and send tear down notification to unblock any
      // waiters.
      if (socket_fd_ != kInvalidFd) {
        shutdown(socket_fd_, SHUT_RDWR);
      }
      if (socket_pipe_w_fd_ != kInvalidFd) {
        write(socket_pipe_w_fd_, "T", 1);
      }

      // Release ownership of the socket by this object and mark as no longer
      // ready.
      ReleaseSocketWithLockHeld();
      ready_ = false;
    }
  }
}

int ServerSocket::TakeSocket() {
  std::lock_guard lock(socket_mutex_);
  return TakeSocketWithLockHeld();
}

int ServerSocket::TakeSocketWithLockHeld() {
  ++socket_own_count_;

  if (ready_ && (socket_fd_ != kInvalidFd) &&
      (socket_pipe_r_fd_ == kInvalidFd)) {
    int fd_list[2];
    if (pipe(fd_list) >= 0) {
      socket_pipe_r_fd_ = fd_list[0];
      socket_pipe_w_fd_ = fd_list[1];
    }
  }

  if (!ready_ || (socket_pipe_r_fd_ == kInvalidFd) ||
      (socket_pipe_w_fd_ == kInvalidFd)) {
    return kInvalidFd;
  }
  return socket_fd_;
}

void ServerSocket::ReleaseSocket() {
  std::lock_guard lock(socket_mutex_);
  ReleaseSocketWithLockHeld();
}

void ServerSocket::ReleaseSocketWithLockHeld() {
  --socket_own_count_;

  if (socket_own_count_ <= 0) {
    ready_ = false;
    if (socket_fd_ != kInvalidFd) {
      close(socket_fd_);
      socket_fd_ = kInvalidFd;
    }
    if (socket_pipe_r_fd_ != kInvalidFd) {
      close(socket_pipe_r_fd_);
      socket_pipe_r_fd_ = kInvalidFd;
    }
    if (socket_pipe_w_fd_ != kInvalidFd) {
      close(socket_pipe_w_fd_);
      socket_pipe_w_fd_ = kInvalidFd;
    }
  }
}

}  // namespace pw::stream
