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

#include <array>

#include "gtest/gtest.h"
#include "pw_polyfill/language_feature_macros.h"
#include "pw_polyfill/standard.h"
#include "pw_polyfill/standard_library/bit.h"
#include "pw_polyfill/standard_library/cstddef.h"
#include "pw_polyfill/standard_library/iterator.h"
#include "pw_polyfill/standard_library/type_traits.h"

namespace pw {
namespace polyfill {
namespace {

PW_INLINE_VARIABLE constexpr int foo = 42;

static_assert(foo == 42, "Error!");

static_assert(PW_CXX_STANDARD_IS_SUPPORTED(98), "C++98 must be supported");
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(11), "C++11 must be supported");
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(14), "C++14 must be supported");

#if __cplusplus >= 201703L
static_assert(PW_CXX_STANDARD_IS_SUPPORTED(17), "C++17 must be not supported");
#else
static_assert(!PW_CXX_STANDARD_IS_SUPPORTED(17), "C++17 must be supported");
#endif  // __cplusplus >= 201703L

TEST(Bit, Endian) {
  if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
    EXPECT_EQ(std::endian::native, std::endian::big);
  } else if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
    EXPECT_EQ(std::endian::native, std::endian::little);
  } else {
    FAIL();
  }
}

TEST(Cstddef, Byte_Operators) {
  std::byte value = std::byte(0);
  EXPECT_EQ((value | std::byte(0x12)), std::byte(0x12));
  EXPECT_EQ((value & std::byte(0x12)), std::byte(0));
  EXPECT_EQ((value ^ std::byte(0x12)), std::byte(0x12));
  EXPECT_EQ(~std::byte(0), std::byte(-1));
  EXPECT_EQ((std::byte(1) << 3), std::byte(0x8));
  EXPECT_EQ((std::byte(0x8) >> 3), std::byte(1));
}

TEST(Cstddef, Byte_AssignmentOperators) {
  std::byte value = std::byte(0);
  EXPECT_EQ((value |= std::byte(0x12)), std::byte(0x12));
  EXPECT_EQ((value &= std::byte(0x0F)), std::byte(0x02));
  EXPECT_EQ((value ^= std::byte(0xFF)), std::byte(0xFD));
  EXPECT_EQ((value <<= 4), std::byte(0xD0));
  EXPECT_EQ((value >>= 5), std::byte(0x6));
}

// Check that consteval is at least equivalent to constexpr.
PW_CONSTEVAL int ConstevalFunction() { return 123; }
static_assert(ConstevalFunction() == 123,
              "Function should work in static_assert");

int c_array[5423] = {};
std::array<int, 32> array;

TEST(Iterator, Size) {
  EXPECT_EQ(std::size(c_array), sizeof(c_array) / sizeof(*c_array));
  EXPECT_EQ(std::size(array), array.size());
}

TEST(Iterator, Data) {
  EXPECT_EQ(std::data(c_array), c_array);
  EXPECT_EQ(std::data(array), array.data());
}

PW_CONSTINIT bool mutable_value = true;

TEST(Constinit, ValueIsMutable) {
  ASSERT_TRUE(mutable_value);
  mutable_value = false;
  ASSERT_FALSE(mutable_value);
  mutable_value = true;
}

TEST(TypeTraits, Aliases) {
  static_assert(
      std::is_same<std::aligned_storage_t<40, 40>,
                   typename std::aligned_storage<40, 40>::type>::value,
      "Alias must be defined");

  static_assert(std::is_same<std::common_type_t<int, bool>,
                             typename std::common_type<int, bool>::type>::value,
                "Alias must be defined");

  static_assert(
      std::is_same<std::conditional_t<false, int, char>,
                   typename std::conditional<false, int, char>::type>::value,
      "Alias must be defined");

  static_assert(
      std::is_same<std::decay_t<int>, typename std::decay<int>::type>::value,
      "Alias must be defined");

  static_assert(std::is_same<std::enable_if_t<true, int>,
                             typename std::enable_if<true, int>::type>::value,
                "Alias must be defined");

  static_assert(std::is_same<std::make_signed_t<int>,
                             typename std::make_signed<int>::type>::value,
                "Alias must be defined");

  static_assert(std::is_same<std::make_unsigned_t<int>,
                             typename std::make_unsigned<int>::type>::value,
                "Alias must be defined");

  static_assert(std::is_same<std::remove_cv_t<int>,
                             typename std::remove_cv<int>::type>::value,
                "Alias must be defined");

  static_assert(std::is_same<std::remove_pointer_t<int>,
                             typename std::remove_pointer<int>::type>::value,
                "Alias must be defined");

  static_assert(std::is_same<std::remove_reference_t<int>,
                             typename std::remove_reference<int>::type>::value,
                "Alias must be defined");
}

TEST(TypeTraits, LogicalTraits) {
  static_assert(std::conjunction<std::true_type, std::true_type>::value,
                "conjunction should be true");
  static_assert(!std::conjunction<std::true_type, std::false_type>::value,
                "conjunction should be false");
  static_assert(!std::conjunction<std::false_type, std::false_type>::value,
                "conjunction should be false");

  static_assert(std::disjunction<std::true_type, std::true_type>::value,
                "disjunction should be true");
  static_assert(std::disjunction<std::true_type, std::false_type>::value,
                "disjunction should be true");
  static_assert(!std::disjunction<std::false_type, std::false_type>::value,
                "disjunction should be false");

  static_assert(!std::negation<std::true_type>::value,
                "negation should be false");
  static_assert(std::negation<std::false_type>::value,
                "negation should be true");
}

}  // namespace
}  // namespace polyfill
}  // namespace pw
