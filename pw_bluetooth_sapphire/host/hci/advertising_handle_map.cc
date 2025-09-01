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

#include "pw_bluetooth_sapphire/internal/host/hci/advertising_handle_map.h"

#include <pw_assert/check.h>

namespace bt::hci {

AdvertisingHandleMap::AdvertisingHandleMap(uint8_t capacity) {
  constexpr uint8_t max_capacity =
      hci_spec::kMaxAdvertisingHandle - hci_spec::kMinAdvertisingHandle + 1;
  capacity_ = std::min(capacity, max_capacity);
}

std::optional<AdvertisementId> AdvertisingHandleMap::Insert(
    const DeviceAddress& address) {
  auto next_handle = NextHandle();
  if (!next_handle) {
    return std::nullopt;
  }

  AdvertisementId next_id = AdvertisementId(next_advertisement_id_++);

  Value value;
  value.handle = next_handle.value();
  value.address = address;
  value.node = node_.CreateChild(node_.UniqueName("advertising_set_"));
  value.node.RecordString("address", address.ToString());
  value.node.RecordUint("handle", next_handle.value());
  value.node.RecordString("id", next_id.ToString());

  PW_CHECK(map_.try_emplace(next_id, std::move(value)).second);
  PW_CHECK(handle_to_id_.try_emplace(next_handle.value(), next_id).second);

  return next_id;
}

hci_spec::AdvertisingHandle AdvertisingHandleMap::GetHandle(
    AdvertisementId id) const {
  auto iter = map_.find(id);
  PW_CHECK(iter != map_.end());
  return iter->second.handle;
}

DeviceAddress AdvertisingHandleMap::GetAddress(AdvertisementId id) const {
  auto iter = map_.find(id);
  PW_CHECK(iter != map_.end());
  return iter->second.address;
}

std::optional<AdvertisementId> AdvertisingHandleMap::GetId(
    hci_spec::AdvertisingHandle handle) const {
  auto iter = handle_to_id_.find(handle);
  if (iter == handle_to_id_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

std::optional<hci_spec::AdvertisingHandle>
AdvertisingHandleMap::LastUsedHandleForTesting() const {
  if (last_handle_ > hci_spec::kMaxAdvertisingHandle) {
    return std::nullopt;
  }

  return last_handle_;
}

std::optional<hci_spec::AdvertisingHandle> AdvertisingHandleMap::NextHandle() {
  if (Size() >= capacity_) {
    return std::nullopt;
  }

  // We avoid selecting an advertising handle with value 0 because some
  // Controllers reserve advertising handle 0 for the legacy HCI LE command use.
  hci_spec::AdvertisingHandle handle = last_handle_;
  do {
    handle = static_cast<uint8_t>(handle + 1) % (capacity_ + 1);
  } while (handle == 0 || handle_to_id_.count(handle) != 0);

  last_handle_ = handle;
  return handle;
}

void AdvertisingHandleMap::Erase(AdvertisementId id) {
  auto iter = map_.find(id);
  if (iter == map_.end()) {
    return;
  }
  handle_to_id_.erase(iter->second.handle);
  map_.erase(iter);
}

void AdvertisingHandleMap::AttachInspect(inspect::Node& parent) {
  node_ = parent.CreateChild("advertising_handle_map");
}

}  // namespace bt::hci
