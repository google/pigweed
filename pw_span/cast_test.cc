// Copyright 2025 The Pigweed Authors
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

#include "pw_span/cast.h"

#include <array>
#include <cstdint>

#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

// TODO: https://pwbug.dev/395945006 - DRY these macros with
// pw_numeric/checked_arithmetic_test.cc

// Emits a TEST() for a given templated function with the given type.
// The test name is prefixed with the type name.
#define TEST_FUNC_FOR_TYPE(suite, name, func, type) \
  TEST(suite, name##_##type) { func<type>(); }

// Emits a TEST() for a templated function named by the concatention of the
// test suite and test name, invoked with the given type.
#define TEST_FOR_TYPE(suite, name, type) \
  TEST_FUNC_FOR_TYPE(suite, name, suite##name, type)

// Emits a TEST_FOR_TYPE() for all common <cstdint> types.
#define TEST_FOR_STDINT_TYPES(suite, func) \
  TEST_FOR_TYPE(suite, func, uint8_t)      \
  TEST_FOR_TYPE(suite, func, int8_t)       \
  TEST_FOR_TYPE(suite, func, uint16_t)     \
  TEST_FOR_TYPE(suite, func, int16_t)      \
  TEST_FOR_TYPE(suite, func, uint32_t)     \
  TEST_FOR_TYPE(suite, func, int32_t)      \
  TEST_FOR_TYPE(suite, func, uint64_t)     \
  TEST_FOR_TYPE(suite, func, int64_t)

#define STATIC_ASSERT_EXTENT(spanvar, n) \
  static_assert(decltype(spanvar)::extent == n)

namespace {

template <class T>
void SpanCastRoundTrip() {
  constexpr size_t kNum = 4;
  std::array<T, kNum> t_array;

  pw::span<T> t_span = pw::span(t_array);

  pw::ByteSpan bytes = pw::as_writable_bytes(t_span);
  EXPECT_EQ(bytes.size(), t_span.size_bytes());
  // The expectation above is actually outside the scope of this test,
  // but we do it to ensure proper setup for the test below.
  // Now verify the UUT span_cast<T>.

  pw::span<T> t_span2 = pw::span_cast<T>(bytes);

  EXPECT_EQ(t_span.data(), t_span2.data());
  EXPECT_EQ(t_span.size(), t_span2.size());
}

template <class T>
void SpanCastRoundTripConst() {
  constexpr size_t kNum = 4;
  std::array<T, kNum> t_array;

  pw::span<T> t_span = pw::span(t_array);

  pw::ConstByteSpan bytes = pw::as_bytes(t_span);
  EXPECT_EQ(bytes.size(), t_span.size_bytes());
  // The expectation above is actually outside the scope of this test,
  // but we do it to ensure proper setup for the test below.
  // Now verify the UUT span_cast<T>.

  // N.B. We use span_cast<T> but the result is span<const T> due to the const
  // type of the span argument.
  pw::span<const T> t_span2 = pw::span_cast<T>(bytes);

  // ...but it's okay if you want to include const in the template arg.
  [[maybe_unused]] pw::span<const T> t_span3 = pw::span_cast<const T>(bytes);

  EXPECT_EQ(t_span.data(), t_span2.data());
  EXPECT_EQ(t_span.size(), t_span2.size());
}

template <class T>
void SpanCastRoundTripStaticExtent() {
  constexpr size_t kNumElem = 4;
  constexpr size_t kNumBytes = kNumElem * sizeof(T);

  std::array<T, kNumElem> t_array;

  // N.B. We use `auto` variables throughout this test, rather than pw::span<T>
  // or pw::ByteSpan, which are always of a dynamic extent!

  auto t_span = pw::span(t_array);
  STATIC_ASSERT_EXTENT(t_span, kNumElem);  // Ensure static extent

  auto byte_span = pw::as_writable_bytes(t_span);
  STATIC_ASSERT_EXTENT(byte_span, kNumBytes);  // Ensure static extent

  // Everything above is actually out of scope for this test...
  // Now verify the UUT span_cast<T>...

  auto t_span2 = pw::span_cast<T>(byte_span);
  STATIC_ASSERT_EXTENT(t_span2, kNumElem);  // Ensure proper static extent
}

template <class T>
void SpanCastRoundTripStaticExtentConst() {
  constexpr size_t kNumElem = 4;
  constexpr size_t kNumBytes = kNumElem * sizeof(T);

  std::array<T, kNumElem> t_array;

  // N.B. We use `auto` variables throughout this test, rather than pw::span<T>
  // or pw::ConstByteSpan, which are always of a dynamic extent!

  auto t_span = pw::span(t_array);
  STATIC_ASSERT_EXTENT(t_span, kNumElem);  // Ensure static extent

  auto byte_span = pw::as_bytes(t_span);
  STATIC_ASSERT_EXTENT(byte_span, kNumBytes);  // Ensure static extent

  // The static_asert_extent checks above are actually out of scope for this
  // test, but we check them to ensure the proper setup for the test below.
  // Now verify the UUT span_cast<T>.

  auto t_span2 = pw::span_cast<const T>(byte_span);
  STATIC_ASSERT_EXTENT(t_span2, kNumElem);  // Ensure proper static extent
}

struct MixedBag {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  float f;
  double d;
};

#define TEST_FOR_ALL_TYPES(suite, func) \
  TEST_FOR_STDINT_TYPES(suite, func)    \
  TEST_FOR_TYPE(suite, func, MixedBag)

TEST_FOR_ALL_TYPES(SpanCast, RoundTrip)
TEST_FOR_ALL_TYPES(SpanCast, RoundTripConst)
TEST_FOR_ALL_TYPES(SpanCast, RoundTripStaticExtent)
TEST_FOR_ALL_TYPES(SpanCast, RoundTripStaticExtentConst)

}  // namespace

// An example test for the docs, which includes the #includes.
// DOCSTAG[start-pw_span-cast-example]
#include "pw_bytes/span.h"
#include "pw_span/cast.h"

void SDK_ReadData(uint8_t* data, size_t size);
void SDK_WriteData(const uint8_t* data, size_t size);

void Write(pw::ConstByteSpan buffer) {
  auto data = pw::span_cast<const uint8_t>(buffer);
  SDK_WriteData(data.data(), data.size());
}

void Read(pw::ByteSpan buffer) {
  auto data = pw::span_cast<uint8_t>(buffer);
  SDK_ReadData(data.data(), data.size());
}
// DOCSTAG[end-pw_span-cast-example]

TEST(SpanCast, Examples) {
  std::array<std::byte, 4> data{};
  Read(data);
  Write(data);
}

void SDK_ReadData(uint8_t*, size_t) {}
void SDK_WriteData(const uint8_t*, size_t) {}
