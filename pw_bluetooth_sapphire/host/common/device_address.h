// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_DEVICE_ADDRESS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_DEVICE_ADDRESS_H_

#include <array>
#include <initializer_list>
#include <string>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

#include <pw_bluetooth/hci.emb.h>

namespace bt {

const size_t kDeviceAddressSize = 6;

// Represents a 48-bit BD_ADDR. This data structure can be directly serialized
// into HCI command payloads.
class DeviceAddressBytes {
 public:
  // The default constructor initializes the address to 00:00:00:00:00:00.
  DeviceAddressBytes();

  // Initializes the contents from |bytes|.
  explicit DeviceAddressBytes(std::array<uint8_t, kDeviceAddressSize> bytes);
  explicit DeviceAddressBytes(const ByteBuffer& bytes);
  explicit DeviceAddressBytes(pw::bluetooth::emboss::BdAddrView view);

  // Returns a string representation of the device address. The bytes in
  // human-readable form will appear in big-endian byte order even though the
  // underlying array stores the bytes in little-endian. The returned string
  // will be of the form:
  //
  //   XX:XX:XX:XX:XX:XX
  std::string ToString() const;

  // Sets all bits of the BD_ADDR to 0.
  void SetToZero();

  // Returns a view over the raw bytes of this address.
  BufferView bytes() const { return BufferView(bytes_.data(), bytes_.size()); }

  // Comparison operators.
  bool operator==(const DeviceAddressBytes& other) const { return bytes_ == other.bytes_; }
  bool operator!=(const DeviceAddressBytes& other) const { return !(*this == other); }
  bool operator<(const DeviceAddressBytes& other) const { return bytes_ < other.bytes_; }

  pw::bluetooth::emboss::BdAddrView view() const {
    return pw::bluetooth::emboss::MakeBdAddrView(&bytes_);
  }

  // Returns a hash of the contents of this address.
  std::size_t Hash() const;

 private:
  // The raw bytes of the BD_ADDR stored in little-endian byte order.
  std::array<uint8_t, kDeviceAddressSize> bytes_;
};

static_assert(sizeof(DeviceAddressBytes) == 6, "DeviceAddressBytes must take up exactly 6 bytes");

// DeviceAddress represents a Bluetooth device address, encapsulating the 48-bit
// device address and the address type. A DeviceAddress is comparable and can be
// used as a key in ordered and unordered associative STL containers.
//
// TODO(fxbug.dev/2761): Using the underlying DeviceAddressBytes for equality, comparison, and
// hashing effectively obsoletes DeviceAddressBytes as a separate class. Removing the |type_| field
// (see bug for rationale) will make this class compatible with serialization.
class DeviceAddress {
 public:
  // Bluetooth device address types.
  enum class Type : uint16_t {
    // BD_ADDR as used in Bluetooth Classic.
    kBREDR,

    // Low Energy Address types.
    kLEPublic,
    kLERandom,
    kLEAnonymous,
  };

  // The default constructor initializes the address to 00:00:00:00:00:00 and
  // the type to Type::kBREDR.
  DeviceAddress();

  // Initializes the contents from raw data.
  DeviceAddress(Type type, const DeviceAddressBytes& value);
  DeviceAddress(Type type, std::array<uint8_t, kDeviceAddressSize> bytes);

  Type type() const { return type_; }
  const DeviceAddressBytes& value() const { return value_; }

  bool IsBrEdr() const { return type_ == Type::kBREDR; }
  bool IsLowEnergy() const {
    return type_ == Type::kLEPublic || type_ == Type::kLERandom || type_ == Type::kLEAnonymous;
  }

  // Comparison operators. The equality and less-than operators are needed to
  // support unordered and ordered containers, respectively.
  bool operator==(const DeviceAddress& other) const {
    return IsTypeCompatible(other) && value_ == other.value_;
  }
  bool operator!=(const DeviceAddress& other) const { return !(*this == other); }
  bool operator<(const DeviceAddress& other) const {
    // Treat |type_| as the higher-order bits
    if (type_ < other.type_ && !IsTypeCompatible(other)) {
      return true;
    }
    return IsTypeCompatible(other) && value_ < other.value_;
  }

  // Returns true if this address is a BR/EDR BD_ADDR or LE public address.
  bool IsPublic() const { return type_ == Type::kBREDR || type_ == Type::kLEPublic; }

  // Returns true if this address is a Resolvable Private Address.
  bool IsResolvablePrivate() const;

  // Returns true if this address is a Non-resolvable Private Address.
  bool IsNonResolvablePrivate() const;

  // Returns true if this is a static random device address.
  bool IsStaticRandom() const;

  // Returns a hash of the contents of this address.
  std::size_t Hash() const;

  // Returns a string representation of this address.
  std::string ToString() const;

 private:
  // True if both addresses have types indicating they're used for the same purpose
  bool IsTypeCompatible(const DeviceAddress& other) const {
    return (type_ == other.type_) || (IsPublic() && other.IsPublic());
  }

  Type type_;
  DeviceAddressBytes value_;
};

static_assert(sizeof(DeviceAddress) == 8, "DeviceAddress must take up exactly 8 bytes");

}  // namespace bt

// Custom specialization of std::hash to support unordered associative
// containers.
namespace std {

template <>
struct hash<bt::DeviceAddress> {
  using argument_type = bt::DeviceAddress;
  using result_type = std::size_t;

  result_type operator()(argument_type const& value) const;
};

}  // namespace std

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_DEVICE_ADDRESS_H_
