// Copyright 2022 The Pigweed Authors
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

#include <cstdio>
#include <mutex>

#include "pw_assert/check.h"
#include "pw_stream/socket_stream.h"
#include "pw_stream/stream.h"
#include "pw_system/config.h"
#include "pw_system/io.h"

namespace pw::system {
namespace {

constexpr uint16_t kPort = PW_SYSTEM_SOCKET_IO_PORT;

stream::SocketStream& GetStream() {
  static bool listening = false;
  static bool client_connected = false;
  static std::mutex socket_open_lock;
  static stream::ServerSocket server_socket;
  static stream::SocketStream socket_stream;
  std::lock_guard guard(socket_open_lock);
  if (!listening) {
    std::printf("Awaiting connection on port %d\n", static_cast<int>(kPort));
    PW_CHECK_OK(server_socket.Listen(kPort));
    listening = true;
  }
  if (client_connected && !socket_stream.IsReady()) {
    client_connected = false;
    std::printf("Client disconnected\n");
  }
  if (!client_connected) {
    auto accept_result = server_socket.Accept();
    PW_CHECK_OK(accept_result.status());
    socket_stream = *std::move(accept_result);
    client_connected = true;
    std::printf("Client connected\n");
  }
  return socket_stream;
}

}  // namespace

stream::Reader& GetReader() { return GetStream(); }
stream::Writer& GetWriter() { return GetStream(); }

}  // namespace pw::system
