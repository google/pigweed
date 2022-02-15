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

#include <cstddef>
#include <span>
#include <tuple>

#include "pw_containers/intrusive_list.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/endpoint.h"
#include "pw_rpc/internal/lock.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/internal/server_call.h"
#include "pw_rpc/service.h"
#include "pw_status/status.h"

namespace pw::rpc {

class Server : public internal::Endpoint {
 public:
  _PW_RPC_CONSTEXPR Server(std::span<Channel> channels) : Endpoint(channels) {}

  // Registers one or more services with the server. This should not be called
  // directly with a Service; instead, use a generated class which inherits
  // from it.
  //
  // This function may be called with any number of services. Combining
  // registration into fewer calls is preferred so the RPC mutex is only
  // locked/unlocked once.
  template <typename... OtherServices>
  void RegisterService(Service& service, OtherServices&... services)
      PW_LOCKS_EXCLUDED(internal::rpc_lock()) {
    internal::LockGuard lock(internal::rpc_lock());
    services_.push_front(service);  // Register the first service

    // Register any additional services by expanding the parameter pack. This
    // is a fold expression of the comma operator.
    (services_.push_front(services), ...);
  }

  // Processes an RPC packet. The packet may contain an RPC request or a control
  // packet, the result of which is processed in this function. Returns whether
  // the packet was able to be processed:
  //
  //   OK - The packet was processed by the server.
  //   DATA_LOSS - Failed to decode the packet.
  //   INVALID_ARGUMENT - The packet is intended for a client, not a server.
  //   UNAVAILABLE - No RPC channel with the requested ID was found.
  //
  // ProcessPacket optionally accepts a ChannelOutput as a second argument. If
  // provided, the server respond on that interface if an unknown channel is
  // requested.
  Status ProcessPacket(ConstByteSpan packet_data)
      PW_LOCKS_EXCLUDED(internal::rpc_lock()) {
    return ProcessPacket(packet_data, nullptr);
  }
  Status ProcessPacket(ConstByteSpan packet_data, ChannelOutput& interface)
      PW_LOCKS_EXCLUDED(internal::rpc_lock()) {
    return ProcessPacket(packet_data, &interface);
  }

 private:
  friend class internal::Call;
  friend class ClientServer;

  // Give call classes access to OpenContext.
  friend class RawServerReaderWriter;
  friend class RawServerWriter;
  friend class RawServerReader;
  friend class RawUnaryResponder;

  template <typename, typename>
  friend class NanopbServerReaderWriter;
  template <typename>
  friend class NanopbServerWriter;
  template <typename, typename>
  friend class NanopbServerReader;
  template <typename>
  friend class NanopbUnaryResponder;

  // Creates a call context for a particular RPC. Unlike the CallContext
  // constructor, this function checks the type of RPC at compile time.
  template <auto kMethod,
            MethodType kExpected,
            typename ServiceImpl,
            typename MethodImpl>
  internal::CallContext OpenContext(uint32_t channel_id,
                                    ServiceImpl& service,
                                    const MethodImpl& method)
      PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock()) {
    using Info = internal::MethodInfo<kMethod>;
    if constexpr (kExpected == MethodType::kUnary) {
      static_assert(
          Info::kType == kExpected,
          "UnaryResponder objects may only be opened for unary RPCs.");
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

    // Unrequested RPCs always use 0 as the call ID. When an actual request is
    // sent, the call will be replaced with its real ID.
    constexpr uint32_t kOpenCallId = 0;

    return internal::CallContext(
        *this, channel_id, service, method, kOpenCallId);
  }

  Status ProcessPacket(ConstByteSpan packet_data, ChannelOutput* interface)
      PW_LOCKS_EXCLUDED(internal::rpc_lock());

  std::tuple<Service*, const internal::Method*> FindMethod(
      const internal::Packet& packet)
      PW_EXCLUSIVE_LOCKS_REQUIRED(internal::rpc_lock());

  void HandleClientStreamPacket(const internal::Packet& packet,
                                internal::Channel& channel,
                                internal::ServerCall* call) const
      PW_UNLOCK_FUNCTION(internal::rpc_lock());

  // Remove these internal::Endpoint functions from the public interface.
  using Endpoint::active_call_count;
  using Endpoint::GetInternalChannel;

  IntrusiveList<Service> services_ PW_GUARDED_BY(internal::rpc_lock());
};

}  // namespace pw::rpc
