// Copyright 2024 The Pigweed Authors
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

#include <optional>

#include "pw_allocator/allocator.h"
#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_bluetooth_proxy/internal/l2cap_signaling_channel.h"
#include "pw_bluetooth_proxy/l2cap_channel_common.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

/// L2CAP connection-oriented channel that supports writing to and reading
/// from a remote peer.
class L2capCoc : public L2capChannel {
 public:
  // TODO: https://pwbug.dev/382783733 - Move downstream client to
  // `L2capChannelEvent` instead of `L2capCoc::Event` and delete this alias.
  using Event = L2capChannelEvent;

  /// Parameters for a direction of packet flow in an `L2capCoc`.
  struct CocConfig {
    /// Channel identifier of the endpoint.
    /// For Rx: Local CID.
    /// For Tx: Remote CID.
    uint16_t cid;
    /// Maximum Transmission Unit.
    /// For Rx: Specified by local device. Indicates the maximum SDU size we are
    ///         capable of accepting.
    /// For Tx: Specified by remote peer. Indicates the maximum SDU size we are
    ///         allowed to send.
    uint16_t mtu;
    /// Maximum PDU payload Size.
    /// For Rx: Specified by local device. Indicates the maximum payload size
    ///         for an L2CAP packet we are capable of accepting.
    /// For Tx: Specified by remote peer. Indicates the maximum payload size for
    ///         for an L2CAP packet we are allowed to send.
    uint16_t mps;
    /// For Rx: Tracks the number of credits we have currently apportioned to
    ///         the remote peer for sending us K-frames in LE Credit Based Flow
    ///         Control mode.
    /// For Tx: Currently available credits for sending K-frames in LE Credit
    ///         Based Flow Control mode. This may be different from the initial
    ///         value if the container has already sent K-frames and/or received
    ///         credits.
    uint16_t credits;
  };

  L2capCoc(const L2capCoc& other) = delete;
  L2capCoc& operator=(const L2capCoc& other) = delete;
  /// Channel is moved on return from factory function, so client is responsible
  /// for storing channel.
  L2capCoc(L2capCoc&& other);
  // TODO: https://pwbug.dev/360929142 - Define move assignment operator so
  // `L2capCoc` can be erased from pw containers.
  L2capCoc& operator=(L2capCoc&& other) = delete;
  ~L2capCoc() override;

  StatusWithMultiBuf Write(pw::multibuf::MultiBuf&& payload) override;

  /// Send an L2CAP_FLOW_CONTROL_CREDIT_IND signaling packet to dispense the
  /// remote peer additional L2CAP connection-oriented channel credits for this
  /// channel.
  ///
  /// @param[in] additional_rx_credits Number of credits to dispense.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  /// UNAVAILABLE:   Send could not be queued due to lack of memory in the
  /// client-provided rx_multibuf_allocator (transient error).
  ///  FAILED_PRECONDITION: If channel is not `State::kRunning`.
  /// @endrst
  pw::Status SendAdditionalRxCredits(uint16_t additional_rx_credits)
      PW_LOCKS_EXCLUDED(rx_mutex_);

 protected:
  static pw::Result<L2capCoc> Create(
      pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
      L2capChannelManager& l2cap_channel_manager,
      L2capSignalingChannel* signaling_channel,
      uint16_t connection_handle,
      CocConfig rx_config,
      CocConfig tx_config,
      Function<void(L2capChannelEvent event)>&& event_fn,
      Function<void(multibuf::MultiBuf&& payload)>&& receive_fn);

  // `SendPayloadFromControllerToClient` with the information payload contained
  // in `kframe`.
  bool DoHandlePduFromController(pw::span<uint8_t> kframe) override
      PW_LOCKS_EXCLUDED(rx_mutex_);

  bool HandlePduFromHost(pw::span<uint8_t> kframe) override;

  void DoClose() override;

  // Increment tx credits by `credits`.
  void AddTxCredits(uint16_t credits) PW_LOCKS_EXCLUDED(tx_mutex_);

 private:
  explicit L2capCoc(pw::multibuf::MultiBufAllocator& rx_multibuf_allocator,
                    L2capChannelManager& l2cap_channel_manager,
                    L2capSignalingChannel* signaling_channel,
                    uint16_t connection_handle,
                    CocConfig rx_config,
                    CocConfig tx_config,
                    Function<void(L2capChannelEvent event)>&& event_fn,
                    Function<void(multibuf::MultiBuf&& payload)>&& receive_fn);

  // Returns max size of L2CAP PDU payload supported by this channel.
  //
  // Returns std::nullopt if ACL data channel is not yet initialized.
  std::optional<uint16_t> MaxL2capPayloadSize() const;

  std::optional<H4PacketWithH4> GenerateNextTxPacket()
      PW_LOCKS_EXCLUDED(tx_mutex_)
          PW_EXCLUSIVE_LOCKS_REQUIRED(send_queue_mutex()) override;

  // TODO: https://pwbug.dev/379337272 - Delete this once all channels have
  // transitioned to payload_queue_.
  bool UsesPayloadQueue() override { return true; }

  // Replenish some of the remote's credits.
  pw::Status ReplenishRxCredits(uint16_t additional_rx_credits)
      PW_EXCLUSIVE_LOCKS_REQUIRED(rx_mutex_);

  L2capSignalingChannel* signaling_channel_ PW_GUARDED_BY(rx_mutex_);

  uint16_t rx_mtu_;
  uint16_t rx_mps_;
  uint16_t tx_mtu_;
  uint16_t tx_mps_;

  Function<void(multibuf::MultiBuf&& payload)> receive_fn_;

  sync::Mutex rx_mutex_;
  uint16_t remaining_sdu_bytes_to_ignore_ PW_GUARDED_BY(rx_mutex_) = 0;
  std::optional<multibuf::MultiBuf> rx_sdu_ PW_GUARDED_BY(rx_mutex_) =
      std::nullopt;
  uint16_t rx_sdu_offset_ PW_GUARDED_BY(rx_mutex_) = 0;
  uint16_t rx_sdu_bytes_remaining_ PW_GUARDED_BY(rx_mutex_) = 0;
  uint16_t rx_remaining_credits_ PW_GUARDED_BY(rx_mutex_);
  uint16_t rx_total_credits_ PW_GUARDED_BY(rx_mutex_);

  sync::Mutex tx_mutex_;
  uint16_t tx_credits_ PW_GUARDED_BY(tx_mutex_);
  uint16_t tx_sdu_offset_ PW_GUARDED_BY(tx_mutex_) = 0;
  bool is_continuing_segment_ PW_GUARDED_BY(tx_mutex_) = false;
};

}  // namespace pw::bluetooth::proxy
