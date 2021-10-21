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

#include "pw_function/function.h"
#include "pw_rpc/internal/call.h"
#include "pw_rpc/internal/config.h"

namespace pw::rpc::internal {

// A Call object, as used by an RPC server.
class ServerCall : public Call {
 public:
  void EndClientStream() {
    MarkClientStreamCompleted();

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
    if (on_client_stream_end_) {
      on_client_stream_end_();
    }
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
  }

 protected:
  ServerCall(ServerCall&& other) { *this = std::move(other); }

  ~ServerCall() {
    // Any errors are logged in Channel::Send.
    CloseAndSendResponse(OkStatus()).IgnoreError();
  }

  ServerCall& operator=(ServerCall&& other);

  constexpr ServerCall() = default;

  ServerCall(const CallContext& context, MethodType type)
      : Call(context, type) {}

  // set_on_client_stream_end is templated so that it can be conditionally
  // disabled with a helpful static_assert message.
  template <typename UnusedType = void>
  void set_on_client_stream_end(
      [[maybe_unused]] Function<void()> on_client_stream_end) {
    static_assert(
        cfg::kClientStreamEndCallbackEnabled<UnusedType>,
        "The client stream end callback is disabled, so "
        "set_on_client_stream_end cannot be called. To enable the client end "
        "callback, set PW_RPC_CLIENT_STREAM_END_CALLBACK to 1.");
#if PW_RPC_CLIENT_STREAM_END_CALLBACK
    on_client_stream_end_ = std::move(on_client_stream_end);
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
  }

 private:
#if PW_RPC_CLIENT_STREAM_END_CALLBACK
  // Called when a client stream completes.
  Function<void()> on_client_stream_end_;
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
};

}  // namespace pw::rpc::internal
