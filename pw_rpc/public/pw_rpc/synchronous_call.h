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

#include "pw_chrono/system_clock.h"
#include "pw_rpc/client.h"
#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/internal/synchronous_call_impl.h"
#include "pw_rpc/synchronous_call_result.h"

/// @file pw_rpc/synchronous_call.h
///
/// `pw_rpc` provides wrappers that convert the asynchronous client API to a
/// synchronous API. The `SynchronousCall<RpcMethod>` functions wrap the
/// asynchronous client RPC call with a timed thread notification and returns
/// once a result is known or a timeout has occurred. Only unary methods are
/// supported.
///
/// The Nanopb and pwpb APIs return a `SynchronousCallResult<Response>` object,
/// which can be queried to determine whether any error scenarios occurred and,
/// if not, access the response. The raw API executes a function when the call
/// completes or returns a `pw::Status` if it does not.
///
/// `SynchronousCall<RpcMethod>` blocks indefinitely, whereas
/// `SynchronousCallFor<RpcMethod>` and `SynchronousCallUntil<RpcMethod>` block
/// for a given timeout or until a deadline, respectively. All wrappers work
/// with either the standalone static RPC functions or the generated service
/// client member methods.
///
/// @note Use of the SynchronousCall wrappers requires a
/// @cpp_class{pw::sync::TimedThreadNotification} backend.
///
/// The following examples use the Nanopb API to make a call that blocks
/// indefinitely. If you'd like to include a timeout for how long the call
/// should block for, use the `SynchronousCallFor()` or `SynchronousCallUntil()`
/// variants.
///
/// @code{.cpp}
///   pw_rpc_EchoMessage request{.msg = "hello" };
///   pw::rpc::SynchronousCallResult<pw_rpc_EchoMessage> result =
///     pw::rpc::SynchronousCall<EchoService::Echo>(rpc_client,
///                                                 channel_id,
///                                                 request);
///   if (result.ok()) {
///     PW_LOG_INFO("%s", result.response().msg);
///   }
/// @endcode
///
/// Additionally, the use of a generated `Client` object is supported:
///
/// @code{.cpp}
///   pw_rpc::nanopb::EchoService::Client client(rpc_client, channel_id);
///   pw_rpc_EchoMessage request{.msg = "hello" };
///   pw::rpc::SynchronousCallResult<pw_rpc_EchoMessage> result =
///     pw::rpc::SynchronousCall<EchoService::Echo>(client, request);
///
///   if (result.ok()) {
///     PW_LOG_INFO("%s", result.response().msg);
///   }
/// @endcode
///
/// The raw API works similarly to the Nanopb API, but takes a
/// @cpp_type{pw::Function} and returns a @cpp_class{pw::Status}. If the RPC
/// completes, the @cpp_type{pw::Function} is called with the response and
/// returned status, and the `SynchronousCall` invocation returns
/// @pw_status{OK}. If the RPC fails, `SynchronousCall` returns an error.
///
/// @code{.cpp}
///   pw::Status rpc_status = pw::rpc::SynchronousCall<EchoService::Echo>(
///       rpc_client, channel_id, encoded_request,
///       [](pw::ConstByteSpan reply, pw::Status status) {
///         PW_LOG_INFO("Received %zu bytes with status %s",
///                     reply.size(),
///                     status.str());
///       });
/// @endcode
///
/// @warning These wrappers should not be used from any context that cannot be
/// blocked! This method will block the calling thread until the RPC completes,
/// and translate the response into a `pw::rpc::SynchronousCallResult` that
/// contains the error type and status or the proto response.
namespace pw::rpc {

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks indefinitely
/// until a response is received.
///
/// @param client The `pw::rpc::Client` to use for the call
/// @param channel_id The ID of the RPC channel to make the call on
/// @param request The proto struct to send as the request
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

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks indefinitely
/// until a response is received.
///
/// @param client The generated service client to use for the call
/// @param request The proto struct to send as the request
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

/// Invokes a unary RPC synchronously using the raw API. Blocks until a
/// response is received.
template <auto kRpcMethod>
Status SynchronousCall(Client& client,
                       uint32_t channel_id,
                       ConstByteSpan request,
                       Function<void(ConstByteSpan, Status)>&& on_completed) {
  return internal::RawSynchronousCall<kRpcMethod>(
      std::move(on_completed),
      internal::CallFreeFunction<kRpcMethod>(client, channel_id, request));
}

/// Invokes a unary RPC synchronously using the raw API. Blocks until a
/// response is received.
template <auto kRpcMethod>
Status SynchronousCall(
    const typename internal::MethodInfo<kRpcMethod>::GeneratedClient& client,
    ConstByteSpan request,
    Function<void(ConstByteSpan, Status)>&& on_completed) {
  return internal::RawSynchronousCall<kRpcMethod>(
      std::move(on_completed),
      internal::CallGeneratedClient<kRpcMethod>(client, request));
}

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks until a
/// response is received or the provided timeout passes.
///
/// @param client The `pw::rpc::Client` to use for the call
/// @param channel_id The ID of the RPC channel to make the call on
/// @param request The proto struct to send as the request
/// @param timeout Duration to block for before returning with Timeout
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

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks until a
/// response is received or the provided timeout passes.
///
/// @param client The generated service client to use for the call
/// @param request The proto struct to send as the request
/// @param timeout Duration to block for before returning with Timeout
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

/// Invokes a unary RPC synchronously using the raw API. Blocks until a
/// response is received or the provided timeout passes.
template <auto kRpcMethod>
Status SynchronousCallFor(
    Client& client,
    uint32_t channel_id,
    ConstByteSpan request,
    chrono::SystemClock::duration timeout,
    Function<void(ConstByteSpan, Status)>&& on_completed) {
  return internal::RawSynchronousCall<kRpcMethod>(
      std::move(on_completed),
      internal::CallFreeFunction<kRpcMethod>(client, channel_id, request),
      timeout);
}

/// Invokes a unary RPC synchronously using the raw API. Blocks until a
/// response is received or the provided timeout passes.
template <auto kRpcMethod>
Status SynchronousCallFor(
    const typename internal::MethodInfo<kRpcMethod>::GeneratedClient& client,
    ConstByteSpan request,
    chrono::SystemClock::duration timeout,
    Function<void(ConstByteSpan, Status)>&& on_completed) {
  return internal::RawSynchronousCall<kRpcMethod>(
      std::move(on_completed),
      internal::CallGeneratedClient<kRpcMethod>(client, request),
      timeout);
}

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks until a
/// response is received or the provided deadline arrives.
///
/// @param client The `pw::rpc::Client` to use for the call
/// @param channel_id The ID of the RPC channel to make the call on
/// @param request The proto struct to send as the request
/// @param deadline Timepoint to block until before returning with Timeout
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

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks until a
/// response is received or the provided deadline arrives.
///
/// @param client The generated service client to use for the call
/// @param request The proto struct to send as the request
/// @param deadline Timepoint to block until before returning with Timeout
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

/// Invokes a unary RPC synchronously using the raw API. Blocks until a
/// response is received or the provided deadline arrives.
template <auto kRpcMethod>
Status SynchronousCallUntil(
    Client& client,
    uint32_t channel_id,
    ConstByteSpan request,
    chrono::SystemClock::time_point deadline,
    Function<void(ConstByteSpan, Status)>&& on_completed) {
  return internal::RawSynchronousCall<kRpcMethod>(
      std::move(on_completed),
      internal::CallFreeFunction<kRpcMethod>(client, channel_id, request),
      deadline);
}

/// Invokes a unary RPC synchronously using the raw API. Blocks until a
/// response is received or the provided deadline arrives.
template <auto kRpcMethod>
Status SynchronousCallUntil(
    const typename internal::MethodInfo<kRpcMethod>::GeneratedClient& client,
    ConstByteSpan request,
    chrono::SystemClock::time_point deadline,
    Function<void(ConstByteSpan, Status)>&& on_completed) {
  return internal::RawSynchronousCall<kRpcMethod>(
      std::move(on_completed),
      internal::CallGeneratedClient<kRpcMethod>(client, request),
      deadline);
}

}  // namespace pw::rpc
