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
#include <iterator>

#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/method_info.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/method_type.h"
#include "pw_rpc/payloads_view.h"

namespace pw::rpc {

class FakeServer;

namespace internal::test {

// A ChannelOutput implementation that stores outgoing packets.
class FakeChannelOutput : public ChannelOutput {
 public:
  FakeChannelOutput(const FakeChannelOutput&) = delete;
  FakeChannelOutput(FakeChannelOutput&&) = delete;

  FakeChannelOutput& operator=(const FakeChannelOutput&) = delete;
  FakeChannelOutput& operator=(FakeChannelOutput&&) = delete;

  Status last_status() const {
    PW_ASSERT(done());
    return packets_.back().status();
  }

  // Returns a view of the payloads seen for this RPC.
  template <auto kMethod>
  PayloadsView payloads(
      uint32_t channel_id = Channel::kUnassignedChannelId) const {
    return PayloadsView(packets_,
                        MethodInfo<kMethod>::kType,
                        channel_id,
                        MethodInfo<kMethod>::kServiceId,
                        MethodInfo<kMethod>::kMethodId);
  }

  PayloadsView payloads(MethodType type,
                        uint32_t channel_id,
                        uint32_t service_id,
                        uint32_t method_id) const {
    return PayloadsView(packets_, type, channel_id, service_id, method_id);
  }

  // Returns a view of the final statuses seen for this RPC. Only relevant for
  // checking packets sent by a server.
  template <auto kMethod>
  StatusView completions(
      uint32_t channel_id = Channel::kUnassignedChannelId) const {
    return StatusView(packets_,
                      internal::PacketType::RESPONSE,
                      internal::PacketType::RESPONSE,
                      channel_id,
                      MethodInfo<kMethod>::kServiceId,
                      MethodInfo<kMethod>::kMethodId);
  }

  template <auto kMethod>
  StatusView errors(uint32_t channel_id = Channel::kUnassignedChannelId) const {
    return StatusView(packets_,
                      internal::PacketType::CLIENT_ERROR,
                      internal::PacketType::SERVER_ERROR,
                      channel_id,
                      MethodInfo<kMethod>::kServiceId,
                      MethodInfo<kMethod>::kMethodId);
  }

  template <auto kMethod>
  size_t client_stream_end_packets(
      uint32_t channel_id = Channel::kUnassignedChannelId) const {
    return internal::test::PacketsView(
               packets_,
               internal::test::PacketFilter(
                   internal::PacketType::CLIENT_STREAM_END,
                   internal::PacketType::CLIENT_STREAM_END,
                   channel_id,
                   MethodInfo<kMethod>::kServiceId,
                   MethodInfo<kMethod>::kMethodId))
        .size();
  }

  // The maximum number of packets this FakeChannelOutput can store. Attempting
  // to store more packets than this is an error.
  size_t max_packets() const { return packets_.max_size(); }

  // The total number of packets that have been sent.
  size_t total_packets() const { return packets_.size(); }

  // Set to true if a RESPONSE packet is seen.
  bool done() const { return total_response_packets_ > 0; }

  // Clears and resets the FakeChannelOutput.
  void clear();

  // Returns `status` for all future SendAndReleaseBuffer calls. Enables packet
  // processing if `status` is OK.
  void set_send_status(Status status) {
    send_status_ = status;
    return_after_packet_count_ = status.ok() ? -1 : 0;
  }

  // Returns `status` once after the specified positive number of packets.
  void set_send_status(Status status, int return_after_packet_count) {
    PW_ASSERT(!status.ok());
    PW_ASSERT(return_after_packet_count > 0);
    send_status_ = status;
    return_after_packet_count_ = return_after_packet_count;
  }

  // Logs which packets have been sent for debugging purposes.
  void LogPackets() const;

 protected:
  constexpr FakeChannelOutput(Vector<Packet>& packets,
                              Vector<std::byte>& payloads,
                              ByteSpan encoding_buffer)
      : ChannelOutput("pw::rpc::internal::test::FakeChannelOutput"),
        packets_(packets),
        payloads_(payloads),
        encoding_buffer_(encoding_buffer) {}

  const Vector<Packet>& packets() const { return packets_; }

 private:
  friend class rpc::FakeServer;

  ByteSpan AcquireBuffer() final { return encoding_buffer_; }

  // Processes buffer according to packet type and `return_after_packet_count_`
  // value as follows:
  // When positive, returns `send_status_` once,
  // When equals 0, returns `send_status_` in all future calls,
  // When negative, ignores `send_status_` processes buffer.
  Status SendAndReleaseBuffer(ConstByteSpan buffer) final;

  void CopyPayloadToBuffer(const ConstByteSpan& payload);

  int return_after_packet_count_ = -1;
  unsigned total_response_packets_ = 0;

  Vector<Packet>& packets_;
  Vector<std::byte>& payloads_;
  Status send_status_ = OkStatus();
  const ByteSpan encoding_buffer_;
};

// Adds the packet output buffer to a FakeChannelOutput.
template <size_t kOutputSizeBytes,
          size_t kMaxPackets,
          size_t kPayloadsBufferSizeBytes>
class FakeChannelOutputBuffer : public FakeChannelOutput {
 protected:
  constexpr FakeChannelOutputBuffer()
      : FakeChannelOutput(packets_, payloads_, encoding_buffer),
        encoding_buffer{},
        payloads_ {}
  {}

  std::byte encoding_buffer[kOutputSizeBytes];
  Vector<std::byte, kPayloadsBufferSizeBytes> payloads_;
  Vector<Packet, kMaxPackets> packets_;
};

}  // namespace internal::test
}  // namespace pw::rpc
