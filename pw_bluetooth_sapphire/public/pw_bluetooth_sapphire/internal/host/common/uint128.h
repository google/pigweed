// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT128_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT128_H_

#include <array>
#include <cstdint>

namespace bt {

// Represents a 128-bit (16-octet) unsigned integer. This is commonly used for
// encryption keys and UUID values.
constexpr size_t kUInt128Size = 16;
using UInt128 = std::array<uint8_t, kUInt128Size>;

static_assert(sizeof(UInt128) == kUInt128Size,
              "UInt128 must take up exactly 16 bytes");

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_UINT128_H_
