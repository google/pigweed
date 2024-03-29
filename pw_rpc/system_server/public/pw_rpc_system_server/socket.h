// Copyright 2021 The Pigweed Authors
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

namespace pw::rpc::system_server {

// Sets the port to use for pw::rpc::system_server backends that use sockets.
void set_socket_port(uint16_t port);

// Configure options for the socket associated with the server.
int SetServerSockOpt(int level,
                     int optname,
                     const void* optval,
                     unsigned int optlen);

}  // namespace pw::rpc::system_server
