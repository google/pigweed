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
#include <utility>

#include "pw_containers/intrusive_list.h"
#include "pw_rpc/internal/call.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/service.h"
#include "pw_status/status.h"

namespace pw::rpc {

class Server;

namespace internal {

class Packet;

// Internal RPC Responder class. The Responder is used to respond to any type of
// RPC. Public classes like ServerWriters inherit from it and provide a public
// API for their use case.
class Responder : public IntrusiveList<Responder>::Item {
 public:
  Responder(ServerCall& call);

  Responder(const Responder&) = delete;

  Responder(Responder&& other) : state_(kClosed) { *this = std::move(other); }

  ~Responder() { Finish(); }

  Responder& operator=(const Responder&) = delete;

  Responder& operator=(Responder&& other);

  // True if the Responder is active and ready to send responses.
  bool open() const { return state_ == kOpen; }

  uint32_t channel_id() const { return call_.channel().id(); }
  uint32_t service_id() const { return call_.service().id(); }
  uint32_t method_id() const;

  // Closes the Responder, if it is open.
  Status Finish(Status status = OkStatus());

 protected:
  constexpr Responder() : state_{kClosed} {}

  const Method& method() const { return call_.method(); }

  const Channel& channel() const { return call_.channel(); }

  constexpr const Channel::OutputBuffer& buffer() const { return response_; }

  // Acquires a buffer into which to write a payload. The Responder MUST be open
  // when this is called!
  std::span<std::byte> AcquirePayloadBuffer();

  // Releases the buffer, sending a packet with the specified payload. The
  // Responder MUST be open when this is called!
  Status ReleasePayloadBuffer(std::span<const std::byte> payload);

  // Releases the buffer without sending a packet.
  Status ReleasePayloadBuffer();

 private:
  friend class rpc::Server;

  void Close();

  ServerCall call_;
  Channel::OutputBuffer response_;
  enum { kClosed, kOpen } state_;
};

}  // namespace internal
}  // namespace pw::rpc
