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

#include <sys/types.h>

#include <mutex>

#include "pw_bytes/span.h"
#include "pw_metric/metric.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/packet_meta.h"
#include "pw_rpc_transport/hdlc_framing.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_rpc_transport/simple_framing.h"
#include "pw_status/status.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "rpc_transport.h"

namespace pw::rpc {
namespace internal {

void LogBadPacket();
void LogChannelIdOverflow(uint32_t channel_id, uint32_t max_channel_id);
void LogMissingEgressForChannel(uint32_t channel_id);
void LogIngressSendFailure(uint32_t channel_id, Status status);

}  // namespace internal

// Ties RPC transport and RPC frame encoder together.
template <typename Encoder>
class RpcEgress : public RpcEgressHandler, public ChannelOutput {
 public:
  RpcEgress(std::string_view channel_name, RpcFrameSender& transport)
      : ChannelOutput(channel_name.data()), transport_(transport) {}

  // Implements both rpc::ChannelOutput and RpcEgressHandler. Encodes the
  // provided packet using the target transport's MTU as max frame size and
  // sends it over that transport.
  //
  // Sending a packet may result in multiple RpcTransport::Write calls which
  // must not be interleaved in order for the packet to be successfully
  // reassembled from the transport-level frames by the receiver. RpcEgress
  // is using a mutex to ensure this. Technically we could just rely on pw_rpc
  // global lock but that would unnecessarily couple transport logic to pw_rpc
  // internals.
  Status SendRpcPacket(ConstByteSpan rpc_packet) override {
    std::lock_guard lock(mutex_);
    return encoder_.Encode(rpc_packet,
                           transport_.MaximumTransmissionUnit(),
                           [this](RpcFrame& frame) {
                             // Encoders must call this callback inline so that
                             // we're still holding `mutex_` here. Unfortunately
                             // the lock annotations cannot be used on
                             // `transport_` to enforce this.
                             return transport_.Send(frame);
                           });
  }

  // Implements ChannelOutput.
  Status Send(ConstByteSpan buffer) override { return SendRpcPacket(buffer); }

 private:
  sync::Mutex mutex_;
  RpcFrameSender& transport_;
  Encoder encoder_ PW_GUARDED_BY(mutex_);
};

// Ties a channel id and the egress that packets on that channel should be sent
// to.
struct ChannelEgress {
  ChannelEgress(uint32_t id, RpcEgressHandler& egress_handler)
      : channel_id(id), egress(&egress_handler) {}

  const uint32_t channel_id;
  RpcEgressHandler* const egress = nullptr;
};

// Handler for incoming RPC packets. RpcIngress is not thread-safe and must be
// accessed from a single thread (typically the RPC RX thread).
template <typename Decoder>
class RpcIngress : public RpcIngressHandler {
 public:
  static constexpr size_t kMaxChannelId = 64;
  RpcIngress() = default;

  explicit RpcIngress(span<ChannelEgress> channel_egresses) {
    for (auto& channel : channel_egresses) {
      PW_ASSERT(channel.channel_id <= kMaxChannelId);
      channel_egresses_[channel.channel_id] = channel.egress;
    }
  }

  const metric::Group& metrics() const { return metrics_; }

  uint32_t num_bad_packets() const { return bad_packets_.value(); }

  uint32_t num_overflow_channel_ids() const {
    return overflow_channel_ids_.value();
  }

  uint32_t num_missing_egresses() const { return missing_egresses_.value(); }

  uint32_t num_egress_errors() const { return egress_errors_.value(); }

  // Finds RPC packets in `buffer`, extracts pw_rpc channel ID from each
  // packet and sends the packet to the egress registered for that channel.
  Status ProcessIncomingData(ConstByteSpan buffer) override {
    return decoder_.Decode(buffer, [this](ConstByteSpan packet) {
      const auto packet_meta = rpc::PacketMeta::FromBuffer(packet);
      if (!packet_meta.ok()) {
        bad_packets_.Increment();
        internal::LogBadPacket();
        return;
      }
      if (packet_meta->channel_id() > kMaxChannelId) {
        overflow_channel_ids_.Increment();
        internal::LogChannelIdOverflow(packet_meta->channel_id(),
                                       kMaxChannelId);
        return;
      }
      auto* egress = channel_egresses_[packet_meta->channel_id()];
      if (egress == nullptr) {
        missing_egresses_.Increment();
        internal::LogMissingEgressForChannel(packet_meta->channel_id());
        return;
      }
      const auto status = egress->SendRpcPacket(packet);
      if (!status.ok()) {
        egress_errors_.Increment();
        internal::LogIngressSendFailure(packet_meta->channel_id(), status);
      }
    });
  }

 private:
  std::array<RpcEgressHandler*, kMaxChannelId + 1> channel_egresses_{};
  Decoder decoder_;
  PW_METRIC_GROUP(metrics_, "pw_rpc_transport");
  PW_METRIC(metrics_, bad_packets_, "bad_packets", 0u);
  PW_METRIC(metrics_, overflow_channel_ids_, "overflow_channel_ids", 0u);
  PW_METRIC(metrics_, missing_egresses_, "missing_egresses", 0u);
  PW_METRIC(metrics_, egress_errors_, "egress_errors", 0u);
};

template <size_t kMaxPacketSize>
using HdlcRpcEgress = RpcEgress<HdlcRpcPacketEncoder<kMaxPacketSize>>;

template <size_t kMaxPacketSize>
using HdlcRpcIngress = RpcIngress<HdlcRpcPacketDecoder<kMaxPacketSize>>;

template <size_t kMaxPacketSize>
using SimpleRpcEgress = RpcEgress<SimpleRpcPacketEncoder<kMaxPacketSize>>;

template <size_t kMaxPacketSize>
using SimpleRpcIngress = RpcIngress<SimpleRpcPacketDecoder<kMaxPacketSize>>;

}  // namespace pw::rpc
