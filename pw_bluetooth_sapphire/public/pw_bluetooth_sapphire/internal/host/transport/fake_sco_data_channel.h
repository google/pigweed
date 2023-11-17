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
#include "pw_bluetooth_sapphire/internal/host/transport/sco_data_channel.h"

namespace bt::hci {

class FakeScoDataChannel final : public ScoDataChannel {
 public:
  struct RegisteredConnection {
    WeakPtr<ConnectionInterface> connection;
  };

  explicit FakeScoDataChannel(uint16_t max_data_length)
      : max_data_length_(max_data_length) {}

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
  std::unordered_map<hci_spec::ConnectionHandle, RegisteredConnection>
      connections_;
};

}  // namespace bt::hci
