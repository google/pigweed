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

#include "pw_rpc/client.h"
#include "pw_status/status.h"

namespace pw::rpc::integration_test {

// The RPC channel for integration test RPCs.
inline constexpr uint32_t kChannelId = 1;

// Returns the global RPC client for integration test use.
Client& client();

// Initializes logging and the global RPC client for integration testing. Starts
// a background thread that processes incoming.
Status InitializeClient(int argc,
                        char* argv[],
                        const char* usage_args = "PORT");

}  // namespace pw::rpc::integration_test
