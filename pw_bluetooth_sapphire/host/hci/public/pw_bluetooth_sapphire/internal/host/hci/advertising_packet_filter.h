// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/hci/discovery_filter.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"
#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {

class AdvertisingPacketFilter {
 public:
  // Some Controllers support offloaded packet filtering (e.g. in Android vendor
  // extensions). PacketFilterConfig allows us to switch on whether this feature
  // is enabled for the current Controller or not.
  class Config {
   public:
    Config(bool offloading_enabled, uint8_t max_filters)
        : offloading_enabled_(offloading_enabled), max_filters_(max_filters) {}

    bool offloading_enabled() const { return offloading_enabled_; }
    uint8_t max_filters() const { return max_filters_; }

   private:
    bool offloading_enabled_ = false;
    uint8_t max_filters_ = 0;
  };

  using ScanId = uint16_t;
  using FilterIndex = uint8_t;

  AdvertisingPacketFilter(const Config& config, Transport::WeakPtr hci);

  void SetPacketFilters(ScanId scan_id,
                        const std::vector<DiscoveryFilter>& filters);

  void UnsetPacketFilters(ScanId scan_id);

  std::unordered_set<ScanId> Matches(const AdvertisingData::ParseResult& ad,
                                     bool connectable,
                                     int8_t rssi) const;

  bool Matches(ScanId scan_id,
               const AdvertisingData::ParseResult& ad,
               bool connectable,
               int8_t rssi) const;

  const std::unordered_set<ScanId>& scan_ids() const { return scan_ids_; }

 private:
  // Packet filter configuration that controls how this class behaves
  // (e.g. Controller offloading enabled, etc).
  Config config_;

  // The total set of scan ids being tracked.
  std::unordered_set<ScanId> scan_ids_;

  // Filters associated with a particular upper layer scan session with a given
  // scan id. These filters may be offloaded to the Controller if Controller
  // offloading is supported. If not, or if the Controller memory is full, they
  // are used to perform Host level packet filtering.
  std::unordered_map<ScanId, std::vector<DiscoveryFilter>> scan_id_to_filters_;

  // The HCI transport.
  Transport::WeakPtr hci_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(AdvertisingPacketFilter);
};

}  // namespace bt::hci
