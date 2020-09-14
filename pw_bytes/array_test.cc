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

#include "pw_bytes/array.h"

#include <array>
#include <cstddef>

#include "gtest/gtest.h"

namespace pw::bytes {
namespace {

template <typename T, typename U>
constexpr bool Equal(const T& lhs, const U& rhs) {
  if (sizeof(lhs) != sizeof(rhs) || std::size(lhs) != std::size(rhs)) {
    return false;
  }

  for (size_t i = 0; i < std::size(lhs); ++i) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }

  return true;
}

using std::byte;

constexpr std::array<byte, 5> kHello{
    byte{'H'}, byte{'e'}, byte{'l'}, byte{'l'}, byte{'o'}};

constexpr uint32_t kEllo =
    static_cast<uint32_t>('e') << 0 | static_cast<uint32_t>('l') << 8 |
    static_cast<uint32_t>('l') << 16 | static_cast<uint32_t>('o') << 24;

static_assert(Equal(String("Hello"), kHello));
static_assert(Equal(String(""), std::array<std::byte, 0>{}));
static_assert(Equal(MakeArray('H', 'e', 'l', 'l', 'o'), kHello));
static_assert(Equal(Concat('H', kEllo), kHello));

constexpr std::array<byte, 3> kInit{byte{'?'}, byte{'?'}, byte{'?'}};
static_assert(Equal(Initialized<3>('?'), kInit));

constexpr std::array<byte, 3> kCounting = MakeArray(0, 1, 2);
static_assert(Equal(Initialized<3>([](size_t i) { return i; }), kCounting));

constexpr std::array<byte, 3> kCounting2 = MakeArray(256, 1, 2);
static_assert(Equal(Initialized<3>([](size_t i) { return i; }), kCounting2));

constexpr auto kArray = Array<1, 2, 3, 255>();
static_assert(Equal(MakeArray(1, 2, 3, 255), kArray));

constexpr std::array<uint8_t, 4> kUintArray = Array<uint8_t, 1, 2, 3, 255>();
static_assert(Equal(MakeArray<uint8_t>(1, 2, 3, 255), kUintArray));

}  // namespace
}  // namespace pw::bytes
