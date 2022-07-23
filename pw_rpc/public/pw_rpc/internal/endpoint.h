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

#include <span>

#include "pw_containers/intrusive_list.h"
#include "pw_result/result.h"
#include "pw_rpc/internal/call.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/channel_list.h"
#include "pw_rpc/internal/lock.h"
#include "pw_rpc/internal/packet.h"
#include "pw_sync/lock_annotations.h"

namespace pw::rpc::internal {

// Manages a list of channels and a list of ongoing calls for either a server or
// client.
//
// For clients, calls start when they send a REQUEST packet to a server. For
// servers, calls start when the REQUEST packet is received. In either case,
// calls add themselves to the Endpoint's list when they're started and
// remove themselves when they complete. Calls do this through their associated
// Server or Client object, which derive from Endpoint.
class Endpoint {
 public:
  ~Endpoint();

  // Creates a channel with the provided ID and ChannelOutput, if a channel slot
  // is available or can be allocated (if PW_RPC_DYNAMIC_ALLOCATION is enabled).
  // Returns:
  //
  //   OK - the channel was opened successfully
  //   ALREADY_EXISTS - a channel with this ID is already present; remove it
  //       first
  //   RESOURCE_EXHAUSTED - no unassigned channels are available and
  //       PW_RPC_DYNAMIC_ALLOCATION is disabled
  //
  Status OpenChannel(uint32_t id, ChannelOutput& interface)
      PW_LOCKS_EXCLUDED(rpc_lock()) {
    LockGuard lock(rpc_lock());
    return channels_.Add(id, interface);
  }

  // Closes a channel and terminates any pending calls on that channel.
  // If the calls are client requests, their on_error callback will be
  // called with the ABORTED status.
  Status CloseChannel(uint32_t channel_id) PW_LOCKS_EXCLUDED(rpc_lock());

  // For internal use only: returns the number calls in the RPC calls list.
  size_t active_call_count() const PW_LOCKS_EXCLUDED(rpc_lock()) {
    LockGuard lock(rpc_lock());
    return calls_.size();
  }

  // For internal use only: finds an internal::Channel with this ID or nullptr
  // if none matches.
  Channel* GetInternalChannel(uint32_t channel_id)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return channels_.Get(channel_id);
  }

 protected:
  _PW_RPC_CONSTEXPR Endpoint(std::span<rpc::Channel> channels)
      : channels_(std::span(static_cast<internal::Channel*>(channels.data()),
                            channels.size())),
        next_call_id_(0) {}

  // Parses an RPC packet and sets ongoing_call to the matching call, if any.
  // Returns the parsed packet or an error.
  Result<Packet> ProcessPacket(std::span<const std::byte> data,
                               Packet::Destination destination)
      PW_LOCKS_EXCLUDED(rpc_lock());

  // Finds a call object for an ongoing call associated with this packet, if
  // any. Returns nullptr if no matching call exists.
  Call* FindCall(const Packet& packet) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    return FindCallById(
        packet.channel_id(), packet.service_id(), packet.method_id());
  }

 private:
  // Give Call access to the register/unregister functions.
  friend class Call;

  // Returns an ID that can be assigned to a new call.
  uint32_t NewCallId() PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    // Call IDs are varint encoded. Limit the varint size to 2 bytes (14 usable
    // bits).
    constexpr uint32_t kMaxCallId = 1 << 14;
    return (++next_call_id_) % kMaxCallId;
  }

  // Adds a call to the internal call registry. If a matching call already
  // exists, it is cancelled locally (on_error called, no packet sent).
  void RegisterCall(Call& call) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  // Registers a call that is known to be unique. The calls list is NOT checked
  // for existing calls.
  void RegisterUniqueCall(Call& call) PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    calls_.push_front(call);
  }

  // Removes the provided call from the call registry.
  void UnregisterCall(const Call& call)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock()) {
    calls_.remove(call);
  }

  Call* FindCallById(uint32_t channel_id,
                     uint32_t service_id,
                     uint32_t method_id)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rpc_lock());

  ChannelList channels_ PW_GUARDED_BY(rpc_lock());
  IntrusiveList<Call> calls_ PW_GUARDED_BY(rpc_lock());

  uint32_t next_call_id_ PW_GUARDED_BY(rpc_lock());
};

}  // namespace pw::rpc::internal
