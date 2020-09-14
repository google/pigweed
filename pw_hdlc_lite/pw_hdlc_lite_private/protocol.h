// Copyright 2020 The Pigweed Authors
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

#include <cstddef>

namespace pw::hdlc_lite {

inline constexpr std::byte kFlag = std::byte{0x7E};
inline constexpr std::byte kEscape = std::byte{0x7D};

inline constexpr std::array<std::byte, 2> kEscapedFlag = {kEscape,
                                                          std::byte{0x5E}};
inline constexpr std::array<std::byte, 2> kEscapedEscape = {kEscape,
                                                            std::byte{0x5D}};

constexpr bool NeedsEscaping(std::byte b) {
  return (b == kFlag || b == kEscape);
}

}  // namespace pw::hdlc_lite
