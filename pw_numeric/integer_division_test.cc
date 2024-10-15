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

#include "pw_numeric/integer_division.h"

#include <cmath>
#include <limits>

#include "pw_unit_test/framework.h"

namespace {

TEST(IntegerDivisionRoundNearest, Sweep) {
  for (int dividend = -100; dividend <= 100; ++dividend) {
    for (int divisor = -100; divisor <= 100; ++divisor) {
      if (divisor == 0) {
        continue;
      }
      EXPECT_EQ(pw::IntegerDivisionRoundNearest(dividend, divisor),
                std::round(static_cast<double>(dividend) /
                           static_cast<double>(divisor)));
    }
  }
}

static_assert(pw::IntegerDivisionRoundNearest<unsigned char>(255, 255) == 1);
static_assert(pw::IntegerDivisionRoundNearest<unsigned char>(254, 255) == 1);
static_assert(pw::IntegerDivisionRoundNearest<unsigned char>(128, 255) == 1);
static_assert(pw::IntegerDivisionRoundNearest<unsigned char>(127, 255) == 0);
static_assert(pw::IntegerDivisionRoundNearest<unsigned char>(1, 255) == 0);

static_assert(pw::IntegerDivisionRoundNearest<signed char>(127, 127) == 1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(126, 127) == 1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(64, 127) == 1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(63, 127) == 0);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(1, 127) == 0);

static_assert(pw::IntegerDivisionRoundNearest<signed char>(-128, -128) == 1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-127, -128) == 1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-64, -128) == 1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-63, -128) == 0);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-1, -128) == 0);

static_assert(pw::IntegerDivisionRoundNearest<signed char>(-128, 127) == -1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-127, 127) == -1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-64, 127) == -1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-63, 127) == 0);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(-1, 127) == 0);

static_assert(pw::IntegerDivisionRoundNearest<signed char>(127, -128) == -1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(64, -128) == -1);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(63, -128) == 0);
static_assert(pw::IntegerDivisionRoundNearest<signed char>(1, -128) == 0);

}  // namespace
