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

#include <cstddef>
#include <span>
#include <utility>

#include "pw_containers/intrusive_list.h"
#include "pw_function/function.h"
#include "pw_rpc/internal/call.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/config.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_type.h"
#include "pw_rpc/service.h"
#include "pw_status/status.h"

namespace pw::rpc {

class Server;

namespace internal {

class Packet;

// Internal RPC Responder class. The Responder is used to respond to any type of
// RPC. Public classes like ServerWriters inherit from it with private
// inheritance and provide a public API for their use case. The Responder's
// public API is used by the Server class.
//
// Private inheritance is used in place of composition or more complex
// inheritance hierarchy so that these objects all inherit from a common
// IntrusiveList::Item object. Private inheritance also gives the derived classs
// full control over their interfaces.
class Responder : public IntrusiveList<Responder>::Item {
 public:
  Responder(const Responder&) = delete;

  ~Responder() { CloseAndSendResponse(OkStatus()).IgnoreError(); }

  Responder& operator=(const Responder&) = delete;

  // True if the Responder is active and ready to send responses.
  bool open() const { return rpc_state_ == kOpen; }

  uint32_t channel_id() const { return call_.channel().id(); }
  uint32_t service_id() const { return call_.service().id(); }
  uint32_t method_id() const;

  // Closes the Responder and sends a RESPONSE packet, if it is open. Returns
  // the status from sending the packet, or FAILED_PRECONDITION if the Responder
  // is not open.
  Status CloseAndSendResponse(std::span<const std::byte> response,
                              Status status);

  Status CloseAndSendResponse(Status status) {
    return CloseAndSendResponse({}, status);
  }

  void HandleError(Status status) {
    Close();
    if (on_error_) {
      on_error_(status);
    }
  }

  void HandleClientStream(std::span<const std::byte> message) const {
    if (on_next_) {
      on_next_(message);
    }
  }

  void EndClientStream() {
    client_stream_state_ = kClientStreamClosed;

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
    if (on_client_stream_end_) {
      on_client_stream_end_();
    }
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK
  }

  bool has_client_stream() const {
    return has_client_stream_ == kHasClientStream;
  }

  bool client_stream_open() const {
    return client_stream_state_ == kClientStreamOpen;
  }

 protected:
  enum HasClientStream : bool { kNoClientStream, kHasClientStream };

  // Creates a Responder for a closed RPC.
  constexpr Responder(HasClientStream has_client_stream)
      : rpc_state_(kClosed),
        has_client_stream_(has_client_stream),
        client_stream_state_(kClientStreamClosed) {}

  // Creates a Responder for an open RPC.
  Responder(ServerCall& call, HasClientStream has_client_stream);

  // Initialize rpc_state_ to closed since move-assignment will check if the
  // Responder is open before moving into it.
  Responder(Responder&& other) : rpc_state_(kClosed) {
    *this = std::move(other);
  }

  Responder& operator=(Responder&& other);

  const Method& method() const { return call_.method(); }

  const Channel& channel() const { return call_.channel(); }

  void set_on_error(Function<void(Status)> on_error) {
    on_error_ = std::move(on_error);
  }

  void set_on_next(Function<void(std::span<const std::byte>)> on_next) {
    on_next_ = std::move(on_next);
  }

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
  // Removes the RPC from the server & marks as closed. The responder must be
  // open when this is called.
  void Close();

  ServerCall call_;
  Channel::OutputBuffer response_;

  // Called when the RPC is terminated due to an error.
  Function<void(Status error)> on_error_;

  // Called when a request is received. Only used for RPCs with client streams.
  // The raw payload buffer is passed to the callback.
  Function<void(std::span<const std::byte> payload)> on_next_;

#if PW_RPC_CLIENT_STREAM_END_CALLBACK
  // Called when a client stream completes.
  Function<void()> on_client_stream_end_;
#endif  // PW_RPC_CLIENT_STREAM_END_CALLBACK

  enum : bool { kClosed, kOpen } rpc_state_;
  HasClientStream has_client_stream_;
  enum : bool { kClientStreamClosed, kClientStreamOpen } client_stream_state_;
};

}  // namespace internal
}  // namespace pw::rpc
