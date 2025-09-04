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

// Docs on using the client synchronous call wrappers defined in this header:
// https://pigweed.dev/pw_rpc/cpp.html#module-pw_rpc-client-sync-call-wrappers

namespace pw::rpc {

/// @submodule{pw_rpc,sync}

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks indefinitely
/// until a response is received.
///
/// @param client The `pw::rpc::Client` to use for the call
/// @param channel_id The ID of the RPC channel to make the call on
/// @param request The proto struct to send as the request
template <
    auto kRpcMethod,
    typename Response = typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCallResult<Response> SynchronousCall(
    Client& client,
    uint32_t channel_id,
    const typename internal::MethodInfo<kRpcMethod>::Request& request) {
  return internal::StructSynchronousCall<kRpcMethod, Response>(
      internal::CallFreeFunctionWithCustomResponse<kRpcMethod, Response>(
          client, channel_id, request));
}

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks indefinitely
/// until a response is received.
///
/// @param client The generated service client to use for the call
/// @param request The proto struct to send as the request
template <auto kRpcMethod, typename GeneratedClient>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCall(
    const GeneratedClient& client,
    const typename internal::MethodInfo<kRpcMethod>::Request& request) {
  return internal::StructSynchronousCall<kRpcMethod>(
      internal::CallGeneratedClient<kRpcMethod>(client, request));
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
  return internal::StructSynchronousCall<kRpcMethod>(
      internal::CallFreeFunction<kRpcMethod>(client, channel_id, request),
      timeout);
}

/// Invokes a unary RPC synchronously using Nanopb or pwpb. Blocks until a
/// response is received or the provided timeout passes.
///
/// @param client The generated service client to use for the call
/// @param request The proto struct to send as the request
/// @param timeout Duration to block for before returning with Timeout
template <auto kRpcMethod, typename GeneratedClient>
SynchronousCallResult<typename internal::MethodInfo<kRpcMethod>::Response>
SynchronousCallFor(
    const GeneratedClient& client,
    const typename internal::MethodInfo<kRpcMethod>::Request& request,
    chrono::SystemClock::duration timeout) {
  return internal::StructSynchronousCall<kRpcMethod>(
      internal::CallGeneratedClient<kRpcMethod>(client, request), timeout);
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
  return internal::StructSynchronousCall<kRpcMethod>(
      internal::CallFreeFunction<kRpcMethod>(client, channel_id, request),
      deadline);
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
  return internal::StructSynchronousCall<kRpcMethod>(
      internal::CallGeneratedClient<kRpcMethod>(client, request), deadline);
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

/// @}

}  // namespace pw::rpc
