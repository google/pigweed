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

#include "pw_numeric/checked_arithmetic.h"

#include "pw_unit_test/framework.h"

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

namespace {

// pw::CheckedAdd()

template <typename T>
void CheckedAddWorks() {
  static constexpr T kMax = std::numeric_limits<T>::max();

  EXPECT_EQ(pw::CheckedAdd<T>(0, 0), 0);
  EXPECT_EQ(pw::CheckedAdd<T>(0, 1), 1);
  EXPECT_EQ(pw::CheckedAdd<T>(1, 0), 1);
  EXPECT_EQ(pw::CheckedAdd<T>(1, 2), 3);
  EXPECT_EQ(pw::CheckedAdd<T>(kMax - 1, 1), kMax);
}

template <typename T>
void CheckedAddDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();

  EXPECT_EQ(pw::CheckedAdd<T>(kMax, 1), std::nullopt);
  EXPECT_EQ(pw::CheckedAdd<T>(1, kMax), std::nullopt);
  EXPECT_EQ(pw::CheckedAdd<T>(kMax, kMax), std::nullopt);
}

TEST_FOR_STDINT_TYPES(CheckedAdd, Works)
TEST_FOR_STDINT_TYPES(CheckedAdd, DetectsOverflow)

template <typename T>
void CheckedAddBoolWorks() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  T result;

  EXPECT_TRUE(pw::CheckedAdd(T{0}, T{0}, result));
  EXPECT_EQ(result, T{0});
  EXPECT_TRUE(pw::CheckedAdd(T{0}, T{1}, result));
  EXPECT_EQ(result, T{1});
  EXPECT_TRUE(pw::CheckedAdd(T{1}, T{0}, result));
  EXPECT_EQ(result, T{1});
  EXPECT_TRUE(pw::CheckedAdd(T{1}, T{2}, result));
  EXPECT_EQ(result, T{3});
  EXPECT_TRUE(pw::CheckedAdd(kMax - 1, T{1}, result));
  EXPECT_EQ(result, kMax);
}

template <typename T>
void CheckedAddBoolDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  T result = T{123};
  EXPECT_FALSE(pw::CheckedAdd(kMax, 1, result));
  EXPECT_EQ(result, T{123});
  EXPECT_FALSE(pw::CheckedAdd(1, kMax, result));
  EXPECT_EQ(result, T{123});
  EXPECT_FALSE(pw::CheckedAdd(kMax, kMax, result));
  EXPECT_EQ(result, T{123});
}

TEST_FOR_STDINT_TYPES(CheckedAddBool, Works)
TEST_FOR_STDINT_TYPES(CheckedAddBool, DetectsOverflow)

// pw::CheckedIncrement()

template <typename T>
void CheckedIncrementWorks() {
  constexpr T kBase = 100;
  T val = kBase;

  ASSERT_TRUE(pw::CheckedIncrement(val, 0u));
  EXPECT_EQ(val, kBase);

  ASSERT_TRUE(pw::CheckedIncrement(val, 1u));
  EXPECT_EQ(val, kBase + 1);

  ASSERT_TRUE(pw::CheckedIncrement(val, 2u));
  EXPECT_EQ(val, kBase + 1 + 2);
}

template <typename T>
void CheckedIncrementDetectsOverflow() {
  static constexpr T kMin = std::numeric_limits<T>::min();
  static constexpr T kMax = std::numeric_limits<T>::max();

  {
    // kMax + 1 => overflow
    static constexpr T kInitialVal = kMax;
    T val = kInitialVal;
    EXPECT_FALSE(pw::CheckedIncrement(val, 1));
    EXPECT_EQ(val, kInitialVal);  // val is unchanged
  }

  {
    // 1 + kMax => overflow
    static constexpr T kInitialVal = 1;
    T val = kInitialVal;
    EXPECT_FALSE(pw::CheckedIncrement(val, kMax));
    EXPECT_EQ(val, kInitialVal);  // val is unchanged
  }

  {
    // kMin + (-1) => overflow
    static constexpr T kInitialVal = kMin;
    T val = kInitialVal;
    EXPECT_FALSE(pw::CheckedIncrement(val, -1));
    EXPECT_EQ(val, kInitialVal);  // val is unchanged
  }

  {
    // kHalfMax + kHalfMax => overflow
    static constexpr T kHalfMax = (kMax / 2) + 1;
    static constexpr T kInitialVal = kHalfMax;
    T val = kInitialVal;
    EXPECT_FALSE(pw::CheckedIncrement(val, kHalfMax));
    EXPECT_EQ(val, kInitialVal);  // val is unchanged
  }
}

TEST_FOR_STDINT_TYPES(CheckedIncrement, Works)
TEST_FOR_STDINT_TYPES(CheckedIncrement, DetectsOverflow)

// pw::CheckedSub()

template <typename T>
void CheckedSubWorks() {
  static constexpr T kMax = std::numeric_limits<T>::max();

  EXPECT_EQ(pw::CheckedSub<T>(0, 0), 0);
  EXPECT_EQ(pw::CheckedSub<T>(1, 0), 1);
  EXPECT_EQ(pw::CheckedSub<T>(1, 1), 0);
  EXPECT_EQ(pw::CheckedSub<T>(3, 2), 1);
  EXPECT_EQ(pw::CheckedSub<T>(kMax, 1), kMax - 1);
}

template <typename T>
void CheckedSubDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();

  EXPECT_FALSE(pw::CheckedSub<T>(kMin, 1));
  EXPECT_FALSE(pw::CheckedSub<T>(kMin, kMax));
}

TEST_FOR_STDINT_TYPES(CheckedSub, Works)
TEST_FOR_STDINT_TYPES(CheckedSub, DetectsOverflow)

template <typename T>
void CheckedSubBoolWorks() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  T result;

  EXPECT_TRUE(pw::CheckedSub(T{0}, T{0}, result));
  EXPECT_EQ(result, T{0});
  EXPECT_TRUE(pw::CheckedSub(T{1}, T{0}, result));
  EXPECT_EQ(result, T{1});
  EXPECT_TRUE(pw::CheckedSub(T{1}, T{1}, result));
  EXPECT_EQ(result, T{0});
  EXPECT_TRUE(pw::CheckedSub(T{3}, T{2}, result));
  EXPECT_EQ(result, T{1});
  EXPECT_TRUE(pw::CheckedSub(kMax, T{1}, result));
  EXPECT_EQ(result, kMax - 1);
}

template <typename T>
void CheckedSubBoolDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();
  T result = T{123};

  EXPECT_FALSE(pw::CheckedSub(kMin, 1, result));
  EXPECT_EQ(result, T{123});
  EXPECT_FALSE(pw::CheckedSub(kMin, kMax, result));
  EXPECT_EQ(result, T{123});
}

TEST_FOR_STDINT_TYPES(CheckedSubBool, Works)
TEST_FOR_STDINT_TYPES(CheckedSubBool, DetectsOverflow)

// pw::CheckedDecrement()

template <typename T>
void CheckedDecrementWorks() {
  constexpr T kBase = 100;
  T val = kBase;

  ASSERT_TRUE(pw::CheckedDecrement(val, 0u));
  EXPECT_EQ(val, kBase);

  ASSERT_TRUE(pw::CheckedDecrement(val, 1u));
  EXPECT_EQ(val, kBase - 1);

  ASSERT_TRUE(pw::CheckedDecrement(val, 2u));
  EXPECT_EQ(val, kBase - 1 - 2);
}

template <typename T>
void CheckedDecrementDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();

  {
    // kMin - 1 => overflow
    static constexpr T kInitialVal = kMin;
    T val = kInitialVal;
    EXPECT_FALSE(pw::CheckedDecrement(val, 1));
    EXPECT_EQ(val, kInitialVal);  // val is unchanged
  }

  {
    // kMax - (-1) => overflow
    static constexpr T kInitialVal = kMax;
    T val = kInitialVal;
    EXPECT_FALSE(pw::CheckedDecrement(val, -1));
    EXPECT_EQ(val, kInitialVal);  // val is unchanged
  }
}

TEST_FOR_STDINT_TYPES(CheckedDecrement, Works)
TEST_FOR_STDINT_TYPES(CheckedDecrement, DetectsOverflow)

// pw::CheckedMul()

template <typename T>
void CheckedMulWorks() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();

  EXPECT_EQ(pw::CheckedMul<T>(0, 0), 0);
  EXPECT_EQ(pw::CheckedMul<T>(1, 0), 0);
  EXPECT_EQ(pw::CheckedMul<T>(1, 1), 1);
  EXPECT_EQ(pw::CheckedMul<T>(3, 2), 6);
  EXPECT_EQ(pw::CheckedMul<T>(kMax, 1), kMax);
  EXPECT_EQ(pw::CheckedMul<T>(kMin, 1), kMin);
}

template <typename T>
void CheckedMulDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();

  EXPECT_FALSE(pw::CheckedMul<T>(kMax, 2));
  if (std::is_signed_v<T>) {
    EXPECT_FALSE(pw::CheckedMul<T>(kMin, 2));
  }
}

TEST_FOR_STDINT_TYPES(CheckedMul, Works)
TEST_FOR_STDINT_TYPES(CheckedMul, DetectsOverflow)

template <typename T>
void CheckedMulBoolWorks() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();
  T result;

  EXPECT_TRUE(pw::CheckedMul(T{0}, T{0}, result));
  EXPECT_EQ(result, T{0});
  EXPECT_TRUE(pw::CheckedMul(T{1}, T{0}, result));
  EXPECT_EQ(result, T{0});
  EXPECT_TRUE(pw::CheckedMul(T{1}, T{1}, result));
  EXPECT_EQ(result, T{1});
  EXPECT_TRUE(pw::CheckedMul(T{3}, T{2}, result));
  EXPECT_EQ(result, T{6});
  EXPECT_TRUE(pw::CheckedMul(kMax, T{1}, result));
  EXPECT_EQ(result, kMax);
  EXPECT_TRUE(pw::CheckedMul(kMin, T{1}, result));
  EXPECT_EQ(result, kMin);
}

template <typename T>
void CheckedMulBoolDetectsOverflow() {
  static constexpr T kMax = std::numeric_limits<T>::max();
  static constexpr T kMin = std::numeric_limits<T>::min();
  T result = T{123};

  EXPECT_FALSE(pw::CheckedMul(kMax, 2, result));
  EXPECT_EQ(result, T{123});
  if (std::is_signed_v<T>) {
    EXPECT_FALSE(pw::CheckedMul(kMin, 2, result));
    EXPECT_EQ(result, T{123});
  }
}

TEST_FOR_STDINT_TYPES(CheckedMulBool, Works)
TEST_FOR_STDINT_TYPES(CheckedMulBool, DetectsOverflow)

}  // namespace
