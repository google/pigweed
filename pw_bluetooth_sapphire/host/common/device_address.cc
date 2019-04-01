// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_address.h"

#include <zircon/assert.h>

#include "src/lib/fxl/strings/split_string.h"
#include "src/lib/fxl/strings/string_number_conversions.h"
#include "src/lib/fxl/strings/string_printf.h"

namespace bt {
namespace common {
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

DeviceAddressBytes::DeviceAddressBytes() {
  SetToZero();
}

DeviceAddressBytes::DeviceAddressBytes(
    std::array<uint8_t, kDeviceAddressSize> bytes) {
  bytes_ = std::move(bytes);
}

DeviceAddressBytes::DeviceAddressBytes(const common::ByteBuffer& bytes) {
  ZX_DEBUG_ASSERT(bytes.size() == bytes_.size());
  std::copy(bytes.cbegin(), bytes.cend(), bytes_.begin());
}

DeviceAddressBytes::DeviceAddressBytes(const fbl::StringPiece bdaddr_string) {
  // Use ZX_ASSERT to prevent this from being compiled out on non-debug builds.
  ZX_ASSERT(SetFromString(bdaddr_string));
}

bool DeviceAddressBytes::SetFromString(const fbl::StringPiece bdaddr_string) {
  // There are 17 characters in XX:XX:XX:XX:XX:XX
  if (bdaddr_string.size() != 17)
    return false;

  auto split = fxl::SplitString(
      fxl::StringView(bdaddr_string.data(), bdaddr_string.size()), ":",
      fxl::kKeepWhitespace, fxl::kSplitWantAll);
  if (split.size() != 6)
    return false;

  size_t index = 5;
  for (const auto& octet_str : split) {
    uint8_t octet;
    if (!fxl::StringToNumberWithError<uint8_t>(octet_str, &octet,
                                               fxl::Base::k16))
      return false;
    bytes_[index--] = octet;
  }

  return true;
}

std::string DeviceAddressBytes::ToString() const {
  return fxl::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X", bytes_[5],
                           bytes_[4], bytes_[3], bytes_[2], bytes_[1],
                           bytes_[0]);
}

void DeviceAddressBytes::SetToZero() {
  bytes_.fill(0);
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

DeviceAddress::DeviceAddress(Type type, const fbl::StringPiece bdaddr_string)
    : DeviceAddress(type, DeviceAddressBytes(bdaddr_string)) {}

DeviceAddress::DeviceAddress(Type type, const DeviceAddressBytes& value)
    : type_(type), value_(value) {}

DeviceAddress::DeviceAddress(Type type,
                             std::array<uint8_t, kDeviceAddressSize> bytes)
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
  std::size_t const h1(std::hash<int>{}(static_cast<int>(type_)));
  std::size_t h2 = value_.Hash();

  return h1 ^ (h2 << 1);
}

std::string DeviceAddress::ToString() const {
  return TypeToString(type_) + value_.ToString();
}

}  // namespace common
}  // namespace bt

namespace std {

hash<bt::common::DeviceAddress>::result_type hash<
    bt::common::DeviceAddress>::operator()(argument_type const& value) const {
  return value.Hash();
}

}  // namespace std
