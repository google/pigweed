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
#include <cpp-string/string_printf.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <type_traits>

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

namespace bt {

template <class InputIt>
std::string ByteContainerToString(InputIt begin, InputIt end) {
  std::string bytes_string;
  for (InputIt iter = begin; iter != end; ++iter) {
    bytes_string += bt_lib_cpp_string::StringPrintf("0x%.2x ", *iter);
  }
  return bytes_string;
}

template <class Container>
std::string ByteContainerToString(const Container& c) {
  return ByteContainerToString(c.begin(), c.end());
}

template <class InputIt>
void PrintByteContainer(InputIt begin, InputIt end) {
  std::cout << ByteContainerToString(begin, end);
}

// Prints the contents of a container as a string.
template <class Container>
void PrintByteContainer(const Container& c) {
  PrintByteContainer(c.begin(), c.end());
}

// Function-template for comparing contents of two iterable byte containers for
// equality. If the contents are not equal, this logs a GTEST-style error
// message to stdout. Meant to be used from unit tests.
template <class InputIt1, class InputIt2>
bool ContainersEqual(InputIt1 expected_begin,
                     InputIt1 expected_end,
                     InputIt2 actual_begin,
                     InputIt2 actual_end) {
  if (std::equal(expected_begin, expected_end, actual_begin, actual_end))
    return true;
  std::cout << "Expected: (" << (expected_end - expected_begin) << " bytes) { ";
  PrintByteContainer(expected_begin, expected_end);
  std::cout << "}\n   Found: (" << (actual_end - actual_begin) << " bytes) { ";
  PrintByteContainer(actual_begin, actual_end);
  std::cout << "}" << std::endl;
  return false;
}

template <class Container1, class Container2>
bool ContainersEqual(const Container1& expected, const Container2& actual) {
  return ContainersEqual(
      expected.begin(), expected.end(), actual.begin(), actual.end());
}

template <class Container1>
bool ContainersEqual(const Container1& expected,
                     const uint8_t* actual_bytes,
                     size_t actual_num_bytes) {
  return ContainersEqual(expected.begin(),
                         expected.end(),
                         actual_bytes,
                         actual_bytes + actual_num_bytes);
}

// Returns a managed pointer to a heap allocated MutableByteBuffer.
template <typename... T>
MutableByteBufferPtr NewBuffer(T... bytes) {
  return std::make_unique<StaticByteBuffer<sizeof...(T)>>(
      std::forward<T>(bytes)...);
}

// Returns the value of |x| as a little-endian array, i.e. the first byte of the
// array has the value of the least significant byte of |x|.
template <typename T>
constexpr std::array<uint8_t, sizeof(T)> ToBytes(T x) {
  static_assert(std::is_integral_v<T>,
                "Must use integral types for safe bytewise access");
  std::array<uint8_t, sizeof(T)> bytes;
  for (auto& byte : bytes) {
    byte = static_cast<uint8_t>(x);
    x >>= 8;
  }
  return bytes;
}

// Returns the Upper/Lower bits of a uint16_t
constexpr uint8_t UpperBits(const uint16_t x) { return ToBytes(x).back(); }
constexpr uint8_t LowerBits(const uint16_t x) { return ToBytes(x).front(); }

}  // namespace bt
