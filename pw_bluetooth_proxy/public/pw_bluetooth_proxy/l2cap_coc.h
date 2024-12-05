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

#include "pw_bluetooth_proxy/internal/l2cap_channel.h"
#include "pw_sync/mutex.h"

namespace pw::bluetooth::proxy {

/// L2CAP connection-oriented channel that supports writing to and reading
/// from a remote peer.
class L2capCoc : public L2capChannel {
 public:
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

  enum class Event {
    // TODO: https://pwbug.dev/360929142 - Listen for
    // L2CAP_DISCONNECTION_REQ/RSP packets and report this event accordingly.
    kChannelClosedByOther,
    /// An invalid packet was received. The channel is now `kStopped` and should
    /// be closed. See error logs for details.
    kRxInvalid,
    /// The channel has received a packet while in the `kStopped` state. The
    /// channel should have been closed.
    kRxWhileStopped,
    /// PDU recombination is not yet supported, but a fragmented L2CAP frame has
    /// been received. The channel is now `kStopped` and should be closed.
    // TODO: https://pwbug.dev/365179076 - Support recombination.
    kRxFragmented,
  };

  L2capCoc(const L2capCoc& other) = delete;
  L2capCoc& operator=(const L2capCoc& other) = delete;
  /// Channel is moved on return from factory function, so client is responsible
  /// for storing channel.
  L2capCoc(L2capCoc&& other);
  // TODO: https://pwbug.dev/360929142 - Define move assignment operator so
  // `L2capCoc` can be erased from pw containers.
  L2capCoc& operator=(L2capCoc&& other) = delete;

  /// Enter `kStopped` state. This means
  ///   - Pending sends will not complete.
  ///   - Calls to `Write()` will return PW_STATUS_FAILED_PRECONDITION.
  ///   - Incoming packets will be dropped & trigger `kRxWhileStopped` events.
  ///   - Container is responsible for closing L2CAP connection & destructing
  ///     the channel object to free its resources.
  ///
  /// .. pw-status-codes::
  ///  OK:               If channel entered `kStopped` state.
  ///  INVALID_ARGUMENT: If channel was previously `kStopped`.
  /// @endrst
  pw::Status Stop();

  /// Send an L2CAP payload to the remote peer.
  ///
  /// @param[in] payload The L2CAP payload to be sent. Payload will be copied
  ///                    before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK:                  If packet was successfully queued for send.
  ///  UNAVAILABLE:         If channel could not acquire the resources to queue
  ///                       the send at this time (transient error).
  ///  INVALID_ARGUMENT:    If payload is too large.
  ///  FAILED_PRECONDITION: If channel is `kStopped`.
  /// @endrst
  pw::Status Write(pw::span<const uint8_t> payload);

 protected:
  static pw::Result<L2capCoc> Create(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      CocConfig rx_config,
      CocConfig tx_config,
      pw::Function<void(pw::span<uint8_t> payload)>&&
          payload_from_controller_fn,
      pw::Function<void(Event event)>&& event_fn);

  // `SendPayloadFromControllerToClient` with the information payload contained
  // in `kframe`. As packet desegmentation is not supported, segmented SDUs are
  // discarded.
  bool HandlePduFromController(pw::span<uint8_t> kframe) override
      PW_LOCKS_EXCLUDED(mutex_);

  bool HandlePduFromHost(pw::span<uint8_t> kframe) override
      PW_LOCKS_EXCLUDED(mutex_);

  // Increment `send_credits_` by `credits`.
  void AddCredits(uint16_t credits) PW_LOCKS_EXCLUDED(mutex_);

 private:
  enum class CocState {
    kRunning,
    kStopped,
  };

  explicit L2capCoc(L2capChannelManager& l2cap_channel_manager,
                    uint16_t connection_handle,
                    CocConfig rx_config,
                    CocConfig tx_config,
                    pw::Function<void(pw::span<uint8_t> payload)>&&
                        payload_from_controller_fn,
                    pw::Function<void(Event event)>&& event_fn);

  // Stop channel & notify client.
  void OnFragmentedPduReceived() override;

  // `Stop()` channel if `kRunning` & call `event_fn_(error)` if it exists.
  void StopChannelAndReportError(Event error);

  // Override: Dequeue a packet only if a credit is able to be subtracted.
  std::optional<H4PacketWithH4> DequeuePacket() override
      PW_LOCKS_EXCLUDED(mutex_);

  CocState state_;
  sync::Mutex mutex_;
  uint16_t rx_mtu_;
  uint16_t rx_mps_;
  uint16_t tx_mtu_;
  uint16_t tx_mps_;
  uint16_t tx_credits_ PW_GUARDED_BY(mutex_);
  uint16_t remaining_sdu_bytes_to_ignore_ PW_GUARDED_BY(mutex_);
  pw::Function<void(Event event)> event_fn_;
};

}  // namespace pw::bluetooth::proxy
