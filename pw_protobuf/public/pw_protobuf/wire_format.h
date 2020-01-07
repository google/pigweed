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

#include <cstdint>

namespace pw::protobuf {

enum class WireType {
  kVarint = 0,
  kFixed64 = 1,
  kDelimited = 2,
  // Wire types 3 and 4 are deprecated per the protobuf specification.
  kFixed32 = 5,
};

inline constexpr unsigned int kFieldNumberShift = 3u;
inline constexpr unsigned int kWireTypeMask = (1u << kFieldNumberShift) - 1u;

constexpr uint32_t MakeKey(uint32_t field_number, WireType wire_type) {
  return (field_number << kFieldNumberShift | static_cast<uint32_t>(wire_type));
}

}  // namespace pw::protobuf
