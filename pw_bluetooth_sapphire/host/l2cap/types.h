// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TYPES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TYPES_H_

#include <lib/fit/function.h>

#include <optional>

#include <fbl/macros.h>
#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection_parameters.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt::l2cap {

class Channel;
// Callback invoked when a channel has been created or when an error occurs during channel creation
// (in which case the channel will be nullptr).
using ChannelCallback = fit::function<void(fbl::RefPtr<Channel>)>;

// Callback invoked when a logical link should be closed due to an error.
using LinkErrorCallback = fit::closure;

// Callback called to notify LE preferred connection parameters during the "LE
// Connection Parameter Update" procedure.
using LEConnectionParameterUpdateCallback =
    fit::function<void(const hci::LEPreferredConnectionParameters&)>;

// Callback called when response received to LE signaling channel Connection Parameters Update
// Request. |accepted| indicates whether the parameters were accepted by the peer.
using ConnectionParameterUpdateRequestCallback = fit::function<void(bool accepted)>;

// Callback used to deliver LE fixed channels that are created when a LE link is
// registered with L2CAP.
using LEFixedChannelsCallback =
    fit::function<void(fbl::RefPtr<Channel> att, fbl::RefPtr<Channel> smp)>;

// Callback used to request a security upgrade for an active logical link.
// Invokes its |callback| argument with the result of the operation.
using SecurityUpgradeCallback = fit::function<void(
    hci::ConnectionHandle ll_handle, sm::SecurityLevel level, sm::StatusCallback callback)>;

// Channel configuration parameters specified by higher layers.
struct ChannelParameters {
  std::optional<ChannelMode> mode;
  // MTU
  std::optional<uint16_t> max_rx_sdu_size;

  bool operator==(const ChannelParameters& rhs) const {
    return mode == rhs.mode && max_rx_sdu_size == rhs.max_rx_sdu_size;
  }

  std::string ToString() const {
    auto mode_string = mode.has_value() ? fxl::StringPrintf("%#.2x", static_cast<uint8_t>(*mode))
                                        : std::string("nullopt");
    auto sdu_string = max_rx_sdu_size.has_value() ? fxl::StringPrintf("%hu", *max_rx_sdu_size)
                                                  : std::string("nullopt");
    return fxl::StringPrintf("ChannelParameters{mode: %s, max_rx_sdu_size: %s}",
                             mode_string.c_str(), sdu_string.c_str());
  };
};

// Convenience struct for passsing around information about an opened channel.
// For example, this is useful when describing the L2CAP channel underlying a zx::socket.
struct ChannelInfo {
  ChannelInfo() = default;

  static ChannelInfo MakeBasicMode(uint16_t max_rx_sdu_size, uint16_t max_tx_sdu_size) {
    return ChannelInfo(ChannelMode::kBasic, max_rx_sdu_size, max_tx_sdu_size, 0, 0, 0);
  }

  static ChannelInfo MakeEnhancedRetransmissionMode(uint16_t max_rx_sdu_size,
                                                    uint16_t max_tx_sdu_size,
                                                    uint8_t n_frames_in_tx_window,
                                                    uint8_t max_transmissions,
                                                    uint16_t max_tx_pdu_payload_size) {
    return ChannelInfo(ChannelMode::kEnhancedRetransmission, max_rx_sdu_size, max_tx_sdu_size,
                       n_frames_in_tx_window, max_transmissions, max_tx_pdu_payload_size);
  }

  ChannelInfo(ChannelMode mode, uint16_t max_rx_sdu_size, uint16_t max_tx_sdu_size,
              uint8_t n_frames_in_tx_window, uint8_t max_transmissions,
              uint16_t max_tx_pdu_payload_size)
      : mode(mode),
        max_rx_sdu_size(max_rx_sdu_size),
        max_tx_sdu_size(max_tx_sdu_size),
        n_frames_in_tx_window(n_frames_in_tx_window),
        max_transmissions(max_transmissions),
        max_tx_pdu_payload_size(max_tx_pdu_payload_size) {}

  ChannelMode mode;
  uint16_t max_rx_sdu_size;
  uint16_t max_tx_sdu_size;

  // TODO(57163): Remove after profile server hack is removed.
  std::optional<hci::ConnectionHandle> handle;

  // For Enhanced Retransmission Mode only. See Core Spec v5.0 Vol 3, Part A, Sec 5.4 for details on
  // each field. Values are not meaningful if mode = ChannelMode::kBasic.
  uint8_t n_frames_in_tx_window;
  uint8_t max_transmissions;
  uint16_t max_tx_pdu_payload_size;
};

// Data stored for services registered by higher layers.
template <typename ChannelCallbackT>
struct ServiceInfo {
  ServiceInfo(ChannelParameters params, ChannelCallbackT cb)
      : channel_params(params), channel_cb(std::move(cb)) {}
  ServiceInfo(ServiceInfo&&) = default;
  ServiceInfo& operator=(ServiceInfo&&) = default;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ServiceInfo);

  // Preferred channel configuration parameters for new channels for this service.
  ChannelParameters channel_params;

  // Callback for forwarding new channels to locally-hosted service.
  ChannelCallbackT channel_cb;
};

}  // namespace bt::l2cap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TYPES_H_
