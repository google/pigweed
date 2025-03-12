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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_packet_filter.h"

namespace bt::hci {

AdvertisingPacketFilter::AdvertisingPacketFilter(
    const Config& packet_filter_config, Transport::WeakPtr hci)
    : config_(packet_filter_config), hci_(std::move(hci)) {
  PW_DCHECK(hci_.is_alive());
}

void AdvertisingPacketFilter::SetPacketFilters(
    ScanId scan_id, const std::vector<DiscoveryFilter>& filters) {
  UnsetPacketFilters(scan_id);

  scan_ids_.insert(scan_id);
  scan_id_to_filters_[scan_id] = filters;
}

void AdvertisingPacketFilter::UnsetPacketFilters(ScanId scan_id) {
  if (scan_id_to_filters_.count(scan_id) == 0) {
    return;
  }

  scan_ids_.erase(scan_id);
  scan_id_to_filters_.erase(scan_id);
}

std::unordered_set<AdvertisingPacketFilter::ScanId>
AdvertisingPacketFilter::Matches(const AdvertisingData::ParseResult& ad,
                                 bool connectable,
                                 int8_t rssi) const {
  std::unordered_set<ScanId> result;

  for (uint16_t scan_id : scan_ids_) {
    if (Matches(scan_id, ad, connectable, rssi)) {
      result.insert(scan_id);
    }
  }

  return result;
}

bool AdvertisingPacketFilter::Matches(ScanId scan_id,
                                      const AdvertisingData::ParseResult& ad,
                                      bool connectable,
                                      int8_t rssi) const {
  if (scan_id_to_filters_.count(scan_id) == 0) {
    return true;
  }

  auto iter = scan_id_to_filters_.find(scan_id);
  const std::vector<DiscoveryFilter>& filters = iter->second;
  if (filters.empty()) {
    return true;
  }

  std::optional<std::reference_wrapper<const AdvertisingData>> data;
  if (ad.is_ok()) {
    data.emplace(ad.value());
  }

  for (const DiscoveryFilter& filter : filters) {
    if (filter.Matches(data, connectable, rssi)) {
      return true;
    }
  }

  return false;
}

}  // namespace bt::hci
