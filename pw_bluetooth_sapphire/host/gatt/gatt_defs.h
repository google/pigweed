// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GATT_DEFS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GATT_DEFS_H_

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"

namespace bt {
namespace gatt {

// 16-bit Attribute Types defined by the GATT profile (Vol 3, Part G, 3.4).
namespace types {

constexpr uint16_t kPrimaryService16 = 0x2800;
constexpr uint16_t kSecondaryService16 = 0x2801;
constexpr uint16_t kIncludeDeclaration16 = 0x2802;
constexpr uint16_t kCharacteristicDeclaration16 = 0x2803;
constexpr uint16_t kCharacteristicExtProperties16 = 0x2900;
constexpr uint16_t kCharacteristicUserDescription16 = 0x2901;
constexpr uint16_t kClientCharacteristicConfig16 = 0x2902;
constexpr uint16_t kServerCharacteristicConfig16 = 0x2903;
constexpr uint16_t kCharacteristicFormat16 = 0x2904;
constexpr uint16_t kCharacteristicAggregateFormat16 = 0x2905;
constexpr uint16_t kGenericAttributeService16 = 0x1801;
constexpr uint16_t kServiceChangedCharacteristic16 = 0x2a05;

constexpr UUID kPrimaryService(kPrimaryService16);
constexpr UUID kSecondaryService(kSecondaryService16);
constexpr UUID kIncludeDeclaration(kIncludeDeclaration16);
constexpr UUID kCharacteristicDeclaration(kCharacteristicDeclaration16);
constexpr UUID kCharacteristicExtProperties(kCharacteristicExtProperties16);
constexpr UUID kCharacteristicUserDescription(kCharacteristicUserDescription16);
constexpr UUID kClientCharacteristicConfig(kClientCharacteristicConfig16);
constexpr UUID kServerCharacteristicConfig(kServerCharacteristicConfig16);
constexpr UUID kCharacteristicFormat(kCharacteristicFormat16);
constexpr UUID kCharacteristicAggregateFormat(kCharacteristicAggregateFormat16);

// Defined Generic Attribute Profile Service (Vol 3, Part G, 7)
constexpr bt::UUID kGenericAttributeService(kGenericAttributeService16);
constexpr bt::UUID kServiceChangedCharacteristic(kServiceChangedCharacteristic16);

}  // namespace types

// Possible values that can be used in a "Characteristic Properties" bitfield.
// (see Vol 3, Part G, 3.3.1.1)
enum Property : uint8_t {
  kBroadcast = 0x01,
  kRead = 0x02,
  kWriteWithoutResponse = 0x04,
  kWrite = 0x08,
  kNotify = 0x10,
  kIndicate = 0x20,
  kAuthenticatedSignedWrites = 0x40,
  kExtendedProperties = 0x80,
};
using Properties = uint8_t;

// Values for "Characteristic Extended Properties" bitfield.
// (see Vol 3, Part G, 3.3.3.1)
enum ExtendedProperty : uint16_t {
  kReliableWrite = 0x0001,
  kWritableAuxiliaries = 0x0002,
};
using ExtendedProperties = uint16_t;

// Values for the "Client Characteristic Configuration" descriptor.
constexpr uint16_t kCCCNotificationBit = 0x0001;
constexpr uint16_t kCCCIndicationBit = 0x0002;

using PeerId = PeerId;

// An identifier uniquely identifies a service, characteristic, or descriptor.
using IdType = uint64_t;

// 0 is reserved as an invalid ID.
constexpr IdType kInvalidId = 0u;

// Types representing GATT discovery results.

struct ServiceData {
  ServiceData() = default;
  ServiceData(att::Handle start, att::Handle end, const UUID& type);

  att::Handle range_start;
  att::Handle range_end;
  UUID type;
};

struct CharacteristicData {
  CharacteristicData() = default;
  CharacteristicData(Properties props, att::Handle handle, att::Handle value_handle,
                     const UUID& type);

  Properties properties;
  att::Handle handle;
  att::Handle value_handle;
  UUID type;
};

struct DescriptorData {
  DescriptorData() = default;
  DescriptorData(att::Handle handle, const UUID& type);

  att::Handle handle;
  UUID type;
};

}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GATT_DEFS_H_
