// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT128_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT128_H_

#include <array>
#include <cstdint>

namespace btlib {
namespace common {

// Represents a 128-bit (16-octet) unsigned integer. This is commonly used for
// encryption keys and UUID values.
using UInt128 = std::array<uint8_t, 16>;

static_assert(sizeof(UInt128) == 16, "UInt128 must take up exactly 16 bytes");

// Returns a random 128-bit value.
// TODO(armansito): Remove this in favor of using Random<UInt128>() directly.
UInt128 RandomUInt128();

}  // namespace common
}  // namespace btlib

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT128_H_
