// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_address.h"

#include <climits>

#include "pw_string/format.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"

namespace bt {
namespace {

std::string TypeToString(DeviceAddress::Type type) {
  switch (type) {
    case DeviceAddress::Type::kBREDR:
      return "(BD_ADDR) ";
    case DeviceAddress::Type::kLEPublic:
      return "(LE publ) ";
    case DeviceAddress::Type::kLERandom:
      return "(LE rand) ";
    case DeviceAddress::Type::kLEAnonymous:
      return "(LE anon) ";
  }

  return "(invalid) ";
}

}  // namespace

DeviceAddressBytes::DeviceAddressBytes() { SetToZero(); }

DeviceAddressBytes::DeviceAddressBytes(std::array<uint8_t, kDeviceAddressSize> bytes) {
  bytes_ = bytes;
}

DeviceAddressBytes::DeviceAddressBytes(const ByteBuffer& bytes) {
  BT_DEBUG_ASSERT(bytes.size() == bytes_.size());
  std::copy(bytes.cbegin(), bytes.cend(), bytes_.begin());
}

DeviceAddressBytes::DeviceAddressBytes(uint64_t addr) {
  for (uint8_t i = 0; i < kDeviceAddressSize; ++i) {
    bytes_[i] = static_cast<uint8_t>(addr & 0xff);
    addr >>= CHAR_BIT;
  }
}

std::string DeviceAddressBytes::ToString() const {
  constexpr size_t out_size = sizeof("00:00:00:00:00:00");
  char out[out_size] = "";
  // Ignore errors. If an error occurs, an empty string will be returned.
  pw::StatusWithSize result =
      pw::string::Format({out, sizeof(out)}, "%02X:%02X:%02X:%02X:%02X:%02X", bytes_[5], bytes_[4],
                         bytes_[3], bytes_[2], bytes_[1], bytes_[0]);
  BT_DEBUG_ASSERT(result.ok());
  return out;
}

void DeviceAddressBytes::SetToZero() { bytes_.fill(0); }

uint64_t DeviceAddressBytes::as_int() const {
  uint64_t addr = 0;

  for (int i = kDeviceAddressSize - 1; i >= 0; --i) {
    addr |= bytes_[i];
    if (i != 0) {
      addr <<= CHAR_BIT;
    }
  }

  return addr;
}

std::size_t DeviceAddressBytes::Hash() const {
  uint64_t bytes_as_int = 0;
  int shift_amount = 0;
  for (const uint8_t& byte : bytes_) {
    bytes_as_int |= (static_cast<uint64_t>(byte) << shift_amount);
    shift_amount += 8;
  }

  std::hash<uint64_t> hash_func;
  return hash_func(bytes_as_int);
}

DeviceAddress::DeviceAddress() : type_(Type::kBREDR) {}

DeviceAddress::DeviceAddress(Type type, const DeviceAddressBytes& value)
    : type_(type), value_(value) {}

DeviceAddress::DeviceAddress(Type type, std::array<uint8_t, kDeviceAddressSize> bytes)
    : DeviceAddress(type, DeviceAddressBytes(bytes)) {}

bool DeviceAddress::IsResolvablePrivate() const {
  // "The two most significant bits of [a RPA] shall be equal to 0 and 1".
  // (Vol 6, Part B, 1.3.2.2).
  uint8_t msb = value_.bytes()[5];
  return type_ == Type::kLERandom && (msb & 0b01000000) && (~msb & 0b10000000);
}

bool DeviceAddress::IsNonResolvablePrivate() const {
  // "The two most significant bits of [a NRPA] shall be equal to 0".
  // (Vol 6, Part B, 1.3.2.2).
  uint8_t msb = value_.bytes()[5];
  return type_ == Type::kLERandom && !(msb & 0b11000000);
}

bool DeviceAddress::IsStaticRandom() const {
  // "The two most significant bits of [a static random address] shall be equal
  // to 1". (Vol 6, Part B, 1.3.2.1).
  uint8_t msb = value_.bytes()[5];
  return type_ == Type::kLERandom && ((msb & 0b11000000) == 0b11000000);
}

std::size_t DeviceAddress::Hash() const {
  const Type type_for_hashing = IsPublic() ? Type::kBREDR : type_;
  std::size_t const h1(std::hash<Type>{}(type_for_hashing));
  std::size_t h2 = value_.Hash();

  return h1 ^ (h2 << 1);
}

std::string DeviceAddress::ToString() const { return TypeToString(type_) + value_.ToString(); }

}  // namespace bt

namespace std {

hash<bt::DeviceAddress>::result_type hash<bt::DeviceAddress>::operator()(
    argument_type const& value) const {
  return value.Hash();
}

}  // namespace std
