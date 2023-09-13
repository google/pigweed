// Copyright 2023 The Pigweed Authors
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

#include <utility>

#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/synchronous_call_result.h"
#include "pw_sync/timed_thread_notification.h"

namespace pw::rpc::internal {

template <typename Response>
struct SynchronousCallState {
  auto OnCompletedCallback() {
    return [this](const Response& response, Status status) {
      result = SynchronousCallResult<Response>(status, response);
      notify.release();
    };
  }

  auto OnRpcErrorCallback() {
    return [this](Status status) {
      result = SynchronousCallResult<Response>::RpcError(status);
      notify.release();
    };
  }

  SynchronousCallResult<Response> result;
  sync::TimedThreadNotification notify;
};

class RawSynchronousCallState {
 public:
  RawSynchronousCallState(Function<void(ConstByteSpan, Status)> on_completed)
      : on_completed_(std::move(on_completed)) {}

  auto OnCompletedCallback() {
    return [this](ConstByteSpan response, Status status) {
      if (on_completed_) {
        on_completed_(response, status);
      }
      notify.release();
    };
  }

  auto OnRpcErrorCallback() {
    return [this](Status status) {
      error = status;
      notify.release();
    };
  }

  Status error;
  sync::TimedThreadNotification notify;

 private:
  Function<void(ConstByteSpan, Status)> on_completed_;
};

// Overloaded function to choose detween timeout and deadline APIs.
inline bool AcquireNotification(sync::TimedThreadNotification& notification,
                                chrono::SystemClock::duration timeout) {
  return notification.try_acquire_for(timeout);
}

inline bool AcquireNotification(sync::TimedThreadNotification& notification,
                                chrono::SystemClock::time_point timeout) {
  return notification.try_acquire_until(timeout);
}

template <auto kRpcMethod,
          typename Response = typename MethodInfo<kRpcMethod>::Response,
          typename DoCall,
          typename... TimeoutArg>
SynchronousCallResult<Response> StructSynchronousCall(
    DoCall&& do_call, TimeoutArg... timeout_arg) {
  static_assert(MethodInfo<kRpcMethod>::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  SynchronousCallState<Response> call_state;

  auto call = std::forward<DoCall>(do_call)(call_state);

  // Wait for the notification based on the type of the timeout argument.
  if constexpr (sizeof...(TimeoutArg) == 0) {
    call_state.notify.acquire();  // Wait forever, since no timeout was given.
  } else if (!AcquireNotification(call_state.notify, timeout_arg...)) {
    return SynchronousCallResult<Response>::Timeout();
  }

  return std::move(call_state.result);
}

// Template for a raw synchronous call. Used for SynchronousCall,
// SynchronousCallFor, and SynchronousCallUntil. The type of the timeout
// argument is used to determine the behavior.
template <auto kRpcMethod, typename DoCall, typename... TimeoutArg>
Status RawSynchronousCall(Function<void(ConstByteSpan, Status)>&& on_completed,
                          DoCall&& do_call,
                          TimeoutArg... timeout_arg) {
  static_assert(MethodInfo<kRpcMethod>::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  RawSynchronousCallState call_state{std::move(on_completed)};

  auto call = std::forward<DoCall>(do_call)(call_state);

  // Wait for the notification based on the type of the timeout argument.
  if constexpr (sizeof...(TimeoutArg) == 0) {
    call_state.notify.acquire();  // Wait forever, since no timeout was given.
  } else if (!AcquireNotification(call_state.notify, timeout_arg...)) {
    return Status::DeadlineExceeded();
  }

  return call_state.error;
}

// Choose which call state object to use (raw or struct).
template <auto kRpcMethod,
          typename Response =
              typename internal::MethodInfo<kRpcMethod>::Response>
using CallState = std::conditional_t<
    std::is_same_v<typename MethodInfo<kRpcMethod>::Request, void>,
    RawSynchronousCallState,
    SynchronousCallState<Response>>;

// Invokes the RPC method free function using a call_state.
template <auto kRpcMethod, typename Request>
constexpr auto CallFreeFunction(Client& client,
                                uint32_t channel_id,
                                const Request& request) {
  return [&client, channel_id, &request](CallState<kRpcMethod>& call_state) {
    return kRpcMethod(client,
                      channel_id,
                      request,
                      call_state.OnCompletedCallback(),
                      call_state.OnRpcErrorCallback());
  };
}

// Invokes the RPC method free function using a call_state and a custom
// response.
template <
    auto kRpcMethod,
    typename Response = typename internal::MethodInfo<kRpcMethod>::Response,
    typename Request>
constexpr auto CallFreeFunctionWithCustomResponse(Client& client,
                                                  uint32_t channel_id,
                                                  const Request& request) {
  return [&client, channel_id, &request](
             CallState<kRpcMethod, Response>& call_state) {
    constexpr auto kMemberFunction =
        MethodInfo<kRpcMethod>::template FunctionTemplate<
            typename MethodInfo<kRpcMethod>::ServiceClass,
            Response>();
    return (*kMemberFunction)(client,
                              channel_id,
                              request,
                              call_state.OnCompletedCallback(),
                              call_state.OnRpcErrorCallback());
  };
}

// Invokes the RPC function on the generated service client using a call_state.
template <auto kRpcMethod, typename Request>
constexpr auto CallGeneratedClient(
    const typename MethodInfo<kRpcMethod>::GeneratedClient& client,
    const Request& request) {
  return [&client, &request](CallState<kRpcMethod>& call_state) {
    constexpr auto kMemberFunction = MethodInfo<kRpcMethod>::template Function<
        typename MethodInfo<kRpcMethod>::GeneratedClient>();
    return (client.*kMemberFunction)(request,
                                     call_state.OnCompletedCallback(),
                                     call_state.OnRpcErrorCallback());
  };
}

}  // namespace pw::rpc::internal
