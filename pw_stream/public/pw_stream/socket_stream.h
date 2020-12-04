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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "pw_stream/stream.h"

namespace pw::stream {

static constexpr int kExitCode = -1;
static constexpr int kInvalidFd = -1;

class SocketStream : public Writer, public Reader {
 public:
  explicit SocketStream() {}

  // Listen to the port and return after a client is connected
  Status Init(uint16_t port);

 private:
  Status DoWrite(std::span<const std::byte> data) override;

  StatusWithSize DoRead(ByteSpan dest) override;

  uint16_t listen_port_ = 0;
  int socket_fd_ = kInvalidFd;
  int conn_fd_ = kInvalidFd;
  struct sockaddr_in sockaddr_client_;
};

}  // namespace pw::stream
