// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TYPES_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_L2CAP_TYPES_H_

#include <optional>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt::l2cap {

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
  ChannelInfo(ChannelMode mode, uint16_t max_rx_sdu_size, uint16_t max_tx_sdu_size)
      : mode(mode), max_rx_sdu_size(max_rx_sdu_size), max_tx_sdu_size(max_tx_sdu_size) {}

  ChannelMode mode;
  uint16_t max_rx_sdu_size;
  uint16_t max_tx_sdu_size;
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
