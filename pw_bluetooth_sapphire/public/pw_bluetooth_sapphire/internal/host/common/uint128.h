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
