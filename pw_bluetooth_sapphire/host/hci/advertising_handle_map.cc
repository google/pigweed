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

namespace bt::hci {

std::optional<hci_spec::AdvertisingHandle> AdvertisingHandleMap::MapHandle(
    const DeviceAddress& address) {
  if (auto it = addr_to_handle_.find(address); it != addr_to_handle_.end()) {
    return it->second;
  }

  if (Size() >= capacity_) {
    return std::nullopt;
  }

  std::optional<hci_spec::AdvertisingHandle> handle = NextHandle();
  BT_ASSERT(handle);

  addr_to_handle_[address] = handle.value();
  handle_to_addr_[handle.value()] = address;
  return handle;
}

// Convert a DeviceAddress to an AdvertisingHandle. The conversion may fail if
// there is no AdvertisingHandle currently mapping to the provided device
// address.
std::optional<hci_spec::AdvertisingHandle> AdvertisingHandleMap::GetHandle(
    const DeviceAddress& address) const {
  if (auto it = addr_to_handle_.find(address); it != addr_to_handle_.end()) {
    return it->second;
  }

  return std::nullopt;
}

std::optional<DeviceAddress> AdvertisingHandleMap::GetAddress(
    hci_spec::AdvertisingHandle handle) const {
  if (handle_to_addr_.count(handle) != 0) {
    return handle_to_addr_.at(handle);
  }

  return std::nullopt;
}

void AdvertisingHandleMap::RemoveHandle(hci_spec::AdvertisingHandle handle) {
  if (handle_to_addr_.count(handle) == 0) {
    return;
  }

  const DeviceAddress& address = handle_to_addr_[handle];
  addr_to_handle_.erase(address);
  handle_to_addr_.erase(handle);
}

void AdvertisingHandleMap::RemoveAddress(const DeviceAddress& address) {
  auto node = addr_to_handle_.extract(address);
  if (node.empty()) {
    return;
  }

  hci_spec::AdvertisingHandle handle = node.mapped();
  handle_to_addr_.erase(handle);
}

std::size_t AdvertisingHandleMap::Size() const {
  BT_ASSERT(addr_to_handle_.size() == handle_to_addr_.size());
  return addr_to_handle_.size();
}

bool AdvertisingHandleMap::Empty() const {
  BT_ASSERT(addr_to_handle_.empty() == handle_to_addr_.empty());
  return addr_to_handle_.empty();
}

void AdvertisingHandleMap::Clear() {
  last_handle_ = kStartHandle;
  addr_to_handle_.clear();
  handle_to_addr_.clear();
}

std::optional<hci_spec::AdvertisingHandle> AdvertisingHandleMap::NextHandle() {
  if (Size() >= capacity_) {
    return std::nullopt;
  }

  hci_spec::AdvertisingHandle handle = last_handle_;
  do {
    handle = static_cast<uint8_t>(handle + 1) % capacity_;
  } while (handle_to_addr_.count(handle) != 0);

  last_handle_ = handle;
  return handle;
}

std::optional<hci_spec::AdvertisingHandle>
AdvertisingHandleMap::LastUsedHandleForTesting() const {
  if (last_handle_ > hci_spec::kMaxAdvertisingHandle) {
    return std::nullopt;
  }

  return last_handle_;
}
}  // namespace bt::hci
