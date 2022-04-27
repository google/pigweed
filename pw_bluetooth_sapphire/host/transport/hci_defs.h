// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_DEFS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_DEFS_H_

#include <cstdint>
#include <type_traits>

namespace bt::hci {

// Indicates the priority of ACL data on a link. Corresponds to priorities in the ACL priority
// vendor command.
enum class AclPriority : uint8_t {
  // Default. Do not prioritize data.
  kNormal,
  // Prioritize receiving data in the inbound direction.
  kSink,
  // Prioritize sending data in the outbound direction.
  kSource,
};

enum class ScoCodingFormat : uint8_t {
  kCvsd = 1,
  kMsbc = 2,
};

enum class ScoEncoding : uint8_t {
  k8Bits = 1,
  k16Bits = 2,
};

enum class ScoSampleRate : uint8_t {
  k8Khz = 1,
  k16Khz = 2,
};

enum class VendorFeaturesBits : uint32_t {
  kSetAclPriorityCommand = (1 << 0),
  kAndroidVendorExtensions = (1 << 1),
};

inline constexpr bool operator&(VendorFeaturesBits left, VendorFeaturesBits right) {
  return static_cast<bool>(static_cast<std::underlying_type_t<VendorFeaturesBits>>(left) &
                           static_cast<std::underlying_type_t<VendorFeaturesBits>>(right));
}

inline constexpr VendorFeaturesBits operator|(VendorFeaturesBits left, VendorFeaturesBits right) {
  return static_cast<VendorFeaturesBits>(
      static_cast<std::underlying_type_t<VendorFeaturesBits>>(left) |
      static_cast<std::underlying_type_t<VendorFeaturesBits>>(right));
}

inline constexpr VendorFeaturesBits& operator|=(VendorFeaturesBits& left,
                                                VendorFeaturesBits right) {
  return left = left | right;
}

enum class VendorCommand : uint32_t {
  kSetAclPriority = 0,
};

}  // namespace bt::hci

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TRANSPORT_HCI_DEFS_H_
