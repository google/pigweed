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

#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"

namespace bt::hci {

// Extended advertising HCI commands refer to a particular advertising set via
// an AdvertisingHandle. An AdvertisingHandle is an eight bit unsigned integer
// that uniquely identifies an advertising set and vice versa. This means that
// we frequently need to convert between a DeviceAddress and an
// AdvertisingHandle. AdvertisingHandleMap provides a mapping from an
// AdvertisingHandle to a DeviceAddress, allocating the next available
// AdvertisingHandle.
//
// When using extended advertising, there are two types of advertising PDU
// formats available: legacy PDUs and extended PDUs. Legacy advertising PDUs are
// currently the most widely compatible type, discoverable by devices deployed
// prior to the adoption of Bluetooth 5.0. Devices deployed more recently are
// also able to discover this type of advertising packet. Conversely, extended
// advertising PDUs are a newer format that offers a number of improvements,
// including the ability to advertise larger amounts of data. However, devices
// not specifically scanning for them, or who are running on an older version of
// Bluetooth (pre-5.0), won't be able to see them.
//
// When advertising using extended advertising PDUs, users often choose to emit
// legacy advertising PDUs as well in order to maintain backwards compatibility
// with older Bluetooth devices. As such, advertisers such as
// ExtendedLowEnergyAdvertiser may need to track two real AdvertisingHandles
// for each logical advertisement, one for legacy advertising PDUs and one for
// extended advertising PDUs. Along with DeviceAddress, AdvertisingHandleMap
// tracks whether the mapping is for an extended PDU or a legacy PDU.
//
// NOTE: Users shouldn't rely on any particular ordering of the next available
// mapping. Any available AdvertisingHandle may be used.
class AdvertisingHandleMap {
 public:
  // Instantiate an AdvertisingHandleMap. The capacity parameter specifies the
  // maximum number of mappings that this instance will support. Setting the
  // capacity also restricts the range of advertising handles
  // AdvertisingHandleMap will return: [0, capacity).
  explicit AdvertisingHandleMap(
      uint8_t capacity = hci_spec::kMaxAdvertisingHandle + 1)
      : capacity_(capacity) {}

  // Convert a DeviceAddress to an AdvertisingHandle, creating the mapping if it
  // doesn't already exist. The conversion may fail if there are already
  // hci_spec::kMaxAdvertisingHandles in the container.
  std::optional<hci_spec::AdvertisingHandle> MapHandle(
      const DeviceAddress& address);

  // Convert an AdvertisingHandle to a DeviceAddress. The conversion may fail if
  // there is no DeviceAddress currently mapping to the provided handle.
  std::optional<DeviceAddress> GetAddress(
      hci_spec::AdvertisingHandle handle) const;

  // Remove the mapping between an AdvertisingHandle and the DeviceAddress it
  // maps to. The container may reuse the AdvertisingHandle for other
  // DeviceAddresses in the future. Immediate future calls to GetAddress(...)
  // with the same AdvertisingHandle will fail because the mapping no longer
  // exists.
  //
  // If the given handle doesn't map to any DeviceAddress, this function does
  // nothing.
  void RemoveHandle(hci_spec::AdvertisingHandle handle) { map_.erase(handle); }

  // Get the maximum number of mappings the AdvertisingHandleMap will support.
  uint8_t capacity() const { return capacity_; }

  // Retrieve the advertising handle that was most recently generated. This
  // function is primarily used by unit tests so as to avoid hardcoding values
  // or making assumptions about the starting point or ordering of advertising
  // handle generation.
  std::optional<hci_spec::AdvertisingHandle> LastUsedHandleForTesting() const;

  // Get the number of unique mappings in the container
  std::size_t Size() const { return map_.size(); }

  // Return true if the container has no mappings, false otherwise
  bool Empty() const { return map_.empty(); }

  // Remove all mappings in the container
  void Clear() { return map_.clear(); }

 private:
  // Although not in the range of valid advertising handles (0x00 to 0xEF),
  // kStartHandle is chosen to be 0xFF because adding one to it will overflow to
  // 0, the first valid advertising handle.
  constexpr static hci_spec::AdvertisingHandle kStartHandle = 0xFF;

  // Tracks the maximum number of elements that can be stored in this container.
  //
  // NOTE: AdvertisingHandles have a range of [0, capacity_). This value isn't
  // set using default member initialization because it is set within the
  // constructor itself.
  uint8_t capacity_;

  // Generate the next valid, available, and within range AdvertisingHandle.
  // This function may fail if there are already
  // hci_spec::kMaxAdvertisingHandles in the container: there are no more valid
  // AdvertisingHandles.
  std::optional<hci_spec::AdvertisingHandle> NextHandle();

  // The last generated advertising handle used as a hint to generate the next
  // handle.
  hci_spec::AdvertisingHandle last_handle_ = kStartHandle;

  std::unordered_map<hci_spec::AdvertisingHandle, DeviceAddress> map_;
};

}  // namespace bt::hci
