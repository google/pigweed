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
#pragma once

#include <utility>

#include "pw_chrono/system_clock.h"
#include "pw_rpc/client.h"
#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/synchronous_call_result.h"
#include "pw_sync/timed_thread_notification.h"

// Synchronous Call wrappers
//
// Wraps an asynchronous RPC client call, converting it to a synchronous
// interface.
//
// WARNING! This should not be called from any context that cannot be blocked!
// This method will block the calling thread until the RPC completes, and
// translate the response into a pw::rpc::SynchronousCallResult that contains
// the error type and status or the proto response.
//
// Example:
//
//   pw_rpc_EchoMessage request{.msg = "hello" };
//   pw::rpc::SynchronousCallResult<pw_rpc_EchoMessage> result =
//     pw::rpc::SynchronousCall<EchoService::Echo>(rpc_client,
//                                                 channel_id,
//                                                 request);
//   if (result.ok()) {
//     printf("%s", result.response().msg);
//   }
//
// Note: The above example will block indefinitely. If you'd like to include a
// timeout for how long the call should block for, use the
// `SynchronousCallFor()` or `SynchronousCallUntil()` variants.
//
// Additionally, the use of a generated Client object is supported:
//
//   pw_rpc::nanopb::EchoService::client client;
//   pw_rpc_EchoMessage request{.msg = "hello" };
//   pw::rpc::SynchronousCallResult<pw_rpc_EchoMessage> result =
//     pw::rpc::SynchronousCall<EchoService::Echo>(client, request);
//
//   if (result.ok()) {
//     printf("%s", result.response().msg);
//   }

namespace pw::rpc {
namespace internal {

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

}  // namespace internal

// SynchronousCall
//
// Template arguments:
//   kRpcMethod: The RPC Method to invoke
//
// Arguments:
//   client: The pw::rpc::Client to use for the call
//   channel_id: The ID of the RPC channel to make the call on
//   request: The proto struct to send as the request
template <auto kRpcMethod>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCall(
    Client& client,
    uint32_t channel_id,
    const typename internal::MethodInfo<kRpcMethod>::Request& request) {
  using Info = internal::MethodInfo<kRpcMethod>;
  using Response = typename Info::Response;
  static_assert(Info::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  internal::SynchronousCallState<Response> call_state;

  auto call = kRpcMethod(client,
                         channel_id,
                         request,
                         call_state.OnCompletedCallback(),
                         call_state.OnRpcErrorCallback());

  call_state.notify.acquire();

  return std::move(call_state.result);
}

// SynchronousCall
//
// Template arguments:
//   kRpcMethod: The RPC Method to invoke
//
// Arguments:
//   client: The service Client to use for the call
//   request: The proto struct to send as the request
template <auto kRpcMethod>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCall(
    const typename internal::MethodInfo<kRpcMethod>::GeneratedClient& client,
    const typename internal::MethodInfo<kRpcMethod>::Request& request) {
  using Info = internal::MethodInfo<kRpcMethod>;
  using Response = typename Info::Response;
  static_assert(Info::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  constexpr auto Function =
      Info::template Function<typename Info::GeneratedClient>();

  internal::SynchronousCallState<Response> call_state;

  auto call = (client.*Function)(request,
                                 call_state.OnCompletedCallback(),
                                 call_state.OnRpcErrorCallback());

  call_state.notify.acquire();

  return std::move(call_state.result);
}

// SynchronousCallFor
//
// Template arguments:
//   kRpcMethod: The RPC Method to invoke
//
// Arguments:
//   client: The pw::rpc::Client to use for the call
//   channel_id: The ID of the RPC channel to make the call on
//   request: The proto struct to send as the request
//   timeout: Duration to block for before returning with Timeout
template <auto kRpcMethod>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCallFor(
    Client& client,
    uint32_t channel_id,
    const typename internal::MethodInfo<kRpcMethod>::Request& request,
    chrono::SystemClock::duration timeout) {
  using Info = internal::MethodInfo<kRpcMethod>;
  using Response = typename Info::Response;
  static_assert(Info::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  internal::SynchronousCallState<Response> call_state;

  auto call = kRpcMethod(client,
                         channel_id,
                         request,
                         call_state.OnCompletedCallback(),
                         call_state.OnRpcErrorCallback());

  if (!call_state.notify.try_acquire_for(timeout)) {
    return SynchronousCallResult<Response>::Timeout();
  }

  return std::move(call_state.result);
}

// SynchronousCallFor
//
// Template arguments:
//   kRpcMethod: The RPC Method to invoke
//
// Arguments:
//   client: The service Client to use for the call
//   request: The proto struct to send as the request
//   timeout: Duration to block for before returning with Timeout
template <auto kRpcMethod>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCallFor(
    const typename internal::MethodInfo<kRpcMethod>::GeneratedClient& client,
    const typename internal::MethodInfo<kRpcMethod>::Request& request,
    chrono::SystemClock::duration timeout) {
  using Info = internal::MethodInfo<kRpcMethod>;
  using Response = typename Info::Response;
  static_assert(Info::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  constexpr auto Function =
      Info::template Function<typename Info::GeneratedClient>();

  internal::SynchronousCallState<Response> call_state;

  auto call = (client.*Function)(request,
                                 call_state.OnCompletedCallback(),
                                 call_state.OnRpcErrorCallback());

  if (!call_state.notify.try_acquire_for(timeout)) {
    return SynchronousCallResult<Response>::Timeout();
  }

  return std::move(call_state.result);
}

// SynchronousCallUntil
//
// Template arguments:
//   kRpcMethod: The RPC Method to invoke
//
// Arguments:
//   client: The pw::rpc::Client to use for the call
//   channel_id: The ID of the RPC channel to make the call on
//   request: The proto struct to send as the request
//   deadline: Timepoint to block until before returning with Timeout
template <auto kRpcMethod>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCallUntil(
    Client& client,
    uint32_t channel_id,
    const typename internal::MethodInfo<kRpcMethod>::Request& request,
    chrono::SystemClock::time_point deadline) {
  using Info = internal::MethodInfo<kRpcMethod>;
  using Response = typename Info::Response;
  static_assert(Info::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  internal::SynchronousCallState<Response> call_state;

  auto call = kRpcMethod(client,
                         channel_id,
                         request,
                         call_state.OnCompletedCallback(),
                         call_state.OnRpcErrorCallback());

  if (!call_state.notify.try_acquire_until(deadline)) {
    return SynchronousCallResult<Response>::Timeout();
  }

  return std::move(call_state.result);
}

// SynchronousCallUntil
//
// Template arguments:
//   kRpcMethod: The RPC Method to invoke
//
// Arguments:
//   client: The service Client to use for the call
//   request: The proto struct to send as the request
//   deadline: Timepoint to block until before returning with Timeout
template <auto kRpcMethod>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCallUntil(
    const typename internal::MethodInfo<kRpcMethod>::GeneratedClient& client,
    const typename internal::MethodInfo<kRpcMethod>::Request& request,
    chrono::SystemClock::time_point deadline) {
  using Info = internal::MethodInfo<kRpcMethod>;
  using Response = typename Info::Response;
  static_assert(Info::kType == MethodType::kUnary,
                "Only unary methods can be used with synchronous calls");

  constexpr auto Function =
      Info::template Function<typename Info::GeneratedClient>();

  internal::SynchronousCallState<Response> call_state;

  auto call = (client.*Function)(request,
                                 call_state.OnCompletedCallback(),
                                 call_state.OnRpcErrorCallback());

  if (!call_state.notify.try_acquire_until(deadline)) {
    return SynchronousCallResult<Response>::Timeout();
  }

  return std::move(call_state.result);
}
}  // namespace pw::rpc
