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

#include "pw_rpc/internal/call_context.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/method_type.h"
#include "pw_rpc/server.h"
#include "pw_rpc/service.h"

namespace pw::rpc::internal {

// Creates a call context for a particular RPC. Unlike the CallContext
// constructor, this function checks the type of RPC at compile time.
template <auto kMethod,
          MethodType kExpected,
          typename ServiceImpl,
          typename MethodImpl>
CallContext OpenContext(Server& server,
                        uint32_t channel_id,
                        ServiceImpl& service,
                        const MethodImpl& method) {
  using Info = internal::MethodInfo<kMethod>;
  if constexpr (kExpected == MethodType::kUnary) {
    static_assert(Info::kType == kExpected,
                  "ServerResponse objects may only be opened for unary RPCs.");
  } else if constexpr (kExpected == MethodType::kServerStreaming) {
    static_assert(
        Info::kType == kExpected,
        "ServerWriters may only be opened for server streaming RPCs.");
  } else if constexpr (kExpected == MethodType::kClientStreaming) {
    static_assert(
        Info::kType == kExpected,
        "ServerReaders may only be opened for client streaming RPCs.");
  } else if constexpr (kExpected == MethodType::kBidirectionalStreaming) {
    static_assert(Info::kType == kExpected,
                  "ServerReaderWriters may only be opened for bidirectional "
                  "streaming RPCs.");
  }

  // TODO(hepler): Update the CallContext to store the ID instead, and lookup
  //     the channel by ID.
  rpc::Channel* channel = server.GetChannel(channel_id);
  PW_ASSERT(channel != nullptr);

  // Unrequested RPCs always use 0 as the call ID. When an actual request is
  // sent, the call will be replaced with its real ID.
  constexpr uint32_t kOpenCallId = 0;

  return CallContext(
      server, static_cast<Channel&>(*channel), service, method, kOpenCallId);
}

}  // namespace pw::rpc::internal
