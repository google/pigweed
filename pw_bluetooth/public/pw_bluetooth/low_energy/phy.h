// Copyright 2024 The Pigweed Authors
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

#include <cstdint>
namespace pw::bluetooth::low_energy {

/// Physical layer configurations. Can be used as a bitmask.
enum class Phy : uint8_t {
  /// Original/default PHY configuration. The only configuration compatible with
  /// legacy devices.
  k1Megabit = 0x01,
  /// Increases throughput but reduces range.
  k2Megabit = 0x02,
  /// Increases maximum range but reduces throughput.
  kLeCoded = 0x04,
};

}  // namespace pw::bluetooth::low_energy
