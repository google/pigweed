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

#include "pw_assert/assert.h"
#include "pw_containers/intrusive_list.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/packet.h"
#include "pw_status/status.h"

namespace pw::rpc::internal {

// Base class representing an active client-side RPC call. Implementations
// derive from this class and provide a packet handler function which is
// called with a reference to the ClientCall object and the received packet.
class BaseClientCall : public IntrusiveList<BaseClientCall>::Item {
 public:
  using ResponseHandler = void (*)(BaseClientCall&, const Packet&);

  // TODO(frolv): Migrate everything to the new constructor and deprecate this.
  constexpr BaseClientCall(rpc::Channel* channel,
                           uint32_t service_id,
                           uint32_t method_id,
                           ResponseHandler handler)
      : client_(static_cast<Channel*>(channel)->client()),
        channel_id_(channel->id()),
        service_id_(service_id),
        method_id_(method_id),
        handler_(handler),
        active_(true) {
    Register();
  }

  constexpr BaseClientCall(Client* client,
                           uint32_t channel_id,
                           uint32_t service_id,
                           uint32_t method_id,
                           ResponseHandler handler)
      : client_(client),
        channel_id_(channel_id),
        service_id_(service_id),
        method_id_(method_id),
        handler_(handler),
        active_(true) {
    PW_ASSERT(client_ != nullptr);
    Register();
  }

  constexpr BaseClientCall()
      : client_(nullptr),
        channel_id_(0),
        service_id_(0),
        method_id_(0),
        handler_(nullptr),
        active_(false) {}

  ~BaseClientCall() { Unregister(); }

  BaseClientCall(const BaseClientCall&) = delete;
  BaseClientCall& operator=(const BaseClientCall&) = delete;

  BaseClientCall(BaseClientCall&& other) : active_(false) {
    *this = std::move(other);
  }
  BaseClientCall& operator=(BaseClientCall&& other);

  constexpr bool active() const { return active_; }

  void Cancel();

 protected:
  constexpr uint32_t channel_id() const { return channel_id_; }
  constexpr uint32_t service_id() const { return service_id_; }
  constexpr uint32_t method_id() const { return method_id_; }

  std::span<std::byte> AcquirePayloadBuffer();
  Status ReleasePayloadBuffer(std::span<const std::byte> payload);

  void Unregister();

 private:
  friend class rpc::Client;

  void Register();

  void HandleResponse(const Packet& packet) { handler_(*this, packet); }

  Packet NewPacket(PacketType type,
                   std::span<const std::byte> payload = {}) const;

  Client* client_;
  uint32_t channel_id_;
  uint32_t service_id_;
  uint32_t method_id_;
  Channel::OutputBuffer request_;
  ResponseHandler handler_;
  bool active_;
};

}  // namespace pw::rpc::internal
