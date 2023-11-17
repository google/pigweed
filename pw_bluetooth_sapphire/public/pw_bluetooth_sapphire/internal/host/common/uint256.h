// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT256_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT256_H_

#include <array>
#include <cstdint>

namespace bt {

// Represents a 256-bit (32-octet) unsigned integer. This is commonly used for
// authentication-related parameters.
constexpr size_t kUInt256Size = 32;
using UInt256 = std::array<uint8_t, kUInt256Size>;

static_assert(UInt256().size() == kUInt256Size,
              "UInt256 must take up exactly 32 bytes");

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT256_H_
