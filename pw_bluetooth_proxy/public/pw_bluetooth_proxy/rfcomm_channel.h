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

/// L2CAP RFCOMM channel that supports writing to and reading
/// from a remote peer.
/// TODO: https://pwbug.dev/378691959 - Switch to just containing a
/// BasicL2capChannel instead of inheritance.
///
/// This implementation requires use of RFCOMM credit based flow control.
class RfcommChannel final : public L2capChannel {
 public:
  /// Parameters for a direction of packet flow in an `RfcommChannel`.
  struct Config {
    /// Channel identifier of the endpoint.
    /// For Rx: Local CID.
    /// For Tx: Remote CID.
    uint16_t cid;
    /// Maximum Information Length.
    /// For Rx: Specified by local device. Indicates the maximum frame size
    ///         for an RFCOMM packet we are capable of accepting.
    /// For Tx: Specified by remote peer. Indicates the maximum frame size for
    ///         for an RFCOMM packet we are allowed to send.
    uint16_t max_information_length;
    /// For Rx: Tracks the number of RFCOMM credits we have currently
    ///         apportioned to the remote peer for sending us frames.
    /// For Tx: Currently available credits for sending frames in RFCOMM Credit
    ///         Based Flow Control mode. This may be different from the initial
    ///         value if the container has already sent frames and/or received
    ///         credits.
    uint16_t credits;
  };

  RfcommChannel(const RfcommChannel& other) = delete;
  RfcommChannel& operator=(const RfcommChannel& other) = delete;
  RfcommChannel(RfcommChannel&& other);
  RfcommChannel& operator=(RfcommChannel&& other) = delete;

  /// Returns an RFCOMM channel that supports writing to and reading from a
  /// remote peer.
  ///
  /// @param[in] l2cap_channel_manager The L2CAP channel manager to register
  ///                                  with.
  ///
  /// @param[in] connection_handle The connection handle of the remote peer.
  ///
  /// @param[in] rx_config         Parameters applying to reading packets.
  ///                              See `rfcomm_channel.h` for details.
  ///
  /// @param[in] tx_config         Parameters applying to writing packets.
  ///                              See `rfcomm_channel.h` for details.
  ///
  /// @param[in] channel_number    RFCOMM channel number to use.
  ///
  /// @param[in] receive_fn        Read callback to be invoked on Rx frames.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  INVALID_ARGUMENT: If arguments are invalid (check logs).
  ///  UNAVAILABLE: If channel could not be created.
  /// @endrst
  static pw::Result<RfcommChannel> Create(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      Config rx_config,
      Config tx_config,
      uint8_t channel_number,
      Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  /// Send an RFCOMM payload to the remote peer.
  ///
  /// @param[in] payload The payload to be sent. Payload will be copied
  ///                    before function completes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///  OK:                  If packet was successfully queued for send.
  ///  UNAVAILABLE:         If channel could not acquire the resources to queue
  ///                       the send at this time (transient error). If an
  ///                       `event_fn` has been provided it will be called with
  ///                       `L2capChannelEvent::kWriteAvailable` when there is
  ///                       queue space available again.
  ///  INVALID_ARGUMENT:    If payload is too large.
  ///  FAILED_PRECONDITION: If channel is not `State::kRunning`.
  /// @endrst
  Status Write(pw::span<const uint8_t> payload);

  Config rx_config() const { return rx_config_; }
  Config tx_config() const { return tx_config_; }

 private:
  static constexpr uint8_t kMinRxCredits = 2;

  RfcommChannel(
      L2capChannelManager& l2cap_channel_manager,
      uint16_t connection_handle,
      Config rx_config,
      Config tx_config,
      uint8_t channel_number,
      Function<void(pw::span<uint8_t> payload)>&& payload_from_controller_fn,
      Function<void(L2capChannelEvent event)>&& event_fn);

  // Parses out RFCOMM payload from `l2cap_pdu` and calls
  // `SendPayloadFromControllerToClient`.
  bool HandlePduFromController(pw::span<uint8_t> l2cap_pdu) override;
  bool HandlePduFromHost(pw::span<uint8_t> l2cap_pdu) override;

  void OnFragmentedPduReceived() override;

  // Override: Dequeue a packet only if a credit is able to be subtracted.
  std::optional<H4PacketWithH4> DequeuePacket() override
      PW_LOCKS_EXCLUDED(mutex_);

  // Override: All traffic on this channel goes to client.
  bool SendPayloadFromControllerToClient(pw::span<uint8_t> payload) override {
    if (payload_from_controller_fn_) {
      payload_from_controller_fn_(payload);
    }
    return true;
  }

  const Config rx_config_;
  const Config tx_config_;
  const uint8_t channel_number_;
  uint8_t rx_credits_ PW_GUARDED_BY(mutex_);
  uint8_t tx_credits_ PW_GUARDED_BY(mutex_);
  sync::Mutex mutex_;
  Function<void(pw::span<uint8_t> payload)> payload_from_controller_fn_;
};

}  // namespace pw::bluetooth::proxy
