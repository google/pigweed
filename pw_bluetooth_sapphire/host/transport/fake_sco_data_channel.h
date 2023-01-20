// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_FAKE_SCO_DATA_CHANNEL_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_FAKE_SCO_DATA_CHANNEL_H_

#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"

namespace bt::hci {

class FakeScoDataChannel final : public ScoDataChannel {
 public:
  struct RegisteredConnection {
    WeakPtr<ConnectionInterface> connection;
  };

  explicit FakeScoDataChannel(uint16_t max_data_length) : max_data_length_(max_data_length) {}

  ~FakeScoDataChannel() override = default;

  const auto& connections() { return connections_; }

  uint16_t readable_count() const { return readable_count_; }

  // ScoDataChannel overrides:
  void RegisterConnection(WeakPtr<ConnectionInterface> connection) override;
  void UnregisterConnection(hci_spec::ConnectionHandle handle) override;
  void OnOutboundPacketReadable() override;
  void ClearControllerPacketCount(hci_spec::ConnectionHandle handle) override {}
  uint16_t max_data_length() const override { return max_data_length_; }

 private:
  uint16_t max_data_length_;
  uint16_t readable_count_ = 0;
  std::unordered_map<hci_spec::ConnectionHandle, RegisteredConnection> connections_;
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_FAKE_SCO_DATA_CHANNEL_H_
