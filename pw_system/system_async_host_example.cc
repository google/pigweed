// Copyright 2024 The Pigweed Authors
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

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>

#include "pw_assert/check.h"
#include "pw_channel/epoll_channel.h"
#include "pw_log/log.h"
#include "pw_multibuf/simple_allocator_for_test.h"
#include "pw_system/system.h"

namespace pw {
namespace {

constexpr int kInvalidFd = -1;

int WaitForTcpConnection(uint16_t port) {
  int listener_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (listener_fd < 0) {
    PW_LOG_ERROR("socket failed: %s", std::strerror(errno));
    return kInvalidFd;
  }

  // Allow binding to an address that may still be in use by a closed socket.
  constexpr int value = 1;
  setsockopt(listener_fd,
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
    if (bind(listener_fd, reinterpret_cast<sockaddr*>(&addr), addr_len) < 0) {
      close(listener_fd);
      PW_LOG_ERROR("bind failed: %s", std::strerror(errno));
      return kInvalidFd;
    }
  }

  if (listen(listener_fd, 1) < 0) {
    close(listener_fd);
    PW_LOG_ERROR("listen failed: %s", std::strerror(errno));
    return kInvalidFd;
  }

  struct sockaddr_in6 sockaddr_client_ = {};
  socklen_t len = sizeof(sockaddr_client_);

  int connection =
      accept(listener_fd, reinterpret_cast<sockaddr*>(&sockaddr_client_), &len);
  if (connection < 0) {
    PW_LOG_ERROR("accept failed: %s", std::strerror(errno));
    return kInvalidFd;
  }
  return connection;
}

}  // namespace
}  // namespace pw

constexpr uint16_t kPort = 33000;  // This should be configurable.

int main() {
  pw::multibuf::test::SimpleAllocatorForTest<4096, 4096> mb_alloc;
  PW_LOG_INFO("Waiting for TCP connection on port %hu", kPort);
  int socket_fd = pw::WaitForTcpConnection(kPort);
  PW_CHECK_INT_NE(socket_fd, pw::kInvalidFd);
  PW_LOG_INFO("Connected; socket descriptor %d", socket_fd);

  pw::channel::EpollChannel channel(
      socket_fd, pw::System().dispatcher(), mb_alloc);
  PW_CHECK(channel.is_read_or_write_open());

  pw::SystemStart(channel);
  return 0;
}
