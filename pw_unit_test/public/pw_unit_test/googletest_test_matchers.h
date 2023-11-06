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

#include <type_traits>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::unit_test {
namespace internal {
// Gets the pw::Status of different types of objects with a pw::Status for
// Matchers that check the status.
inline constexpr Status GetStatus(Status status) { return status; }

inline constexpr Status GetStatus(StatusWithSize status_with_size) {
  return status_with_size.status();
}

template <typename T>
inline constexpr Status GetStatus(const Result<T>& result) {
  return result.status();
}

// Implements IsOk().
class IsOkMatcher {
 public:
  using is_gtest_matcher = void;

  void DescribeTo(std::ostream* os) const { *os << "is OK"; }

  void DescribeNegationTo(std::ostream* os) const { *os << "isn't OK"; }

  template <typename T>
  bool MatchAndExplain(T&& actual_value,
                       ::testing::MatchResultListener* listener) const {
    const auto status = GetStatus(actual_value);
    if (!status.ok()) {
      *listener << "which has status " << pw_StatusString(status);
      return false;
    }
    return true;
  }
};

// Implements StatusIs().
class StatusIsMatcher {
 public:
  explicit StatusIsMatcher(Status expected_status)
      : expected_status_(expected_status) {}

  void DescribeTo(std::ostream* os) const {
    *os << "has status " << pw_StatusString(expected_status_);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "does not have status " << pw_StatusString(expected_status_);
  }

  template <typename T>
  bool MatchAndExplain(T&& actual_value,
                       ::testing::MatchResultListener* listener) const {
    const auto status = GetStatus(actual_value);
    if (status != expected_status_) {
      *listener << "which has status " << pw_StatusString(status);
      return false;
    }
    return true;
  }

 private:
  const Status expected_status_;
};

}  // namespace internal

/// Macros for testing the results of functions that return ``pw::Status``,
/// ``pw::StatusWithSize``, or ``pw::Result<T>`` (for any T).
#define EXPECT_OK(expression) EXPECT_THAT(expression, ::pw::unit_test::IsOk())
#define ASSERT_OK(expression) ASSERT_THAT(expression, ::pw::unit_test::IsOk())

/// Returns a gMock matcher that matches a `pw::Status`, `pw::StatusWithSize`,
/// or `pw::Result<T>` (for any T) which is OK.
inline internal::IsOkMatcher IsOk() { return {}; }

/// Returns a gMock matcher that matches a `pw::Status`, `pw::StatusWithSize`,
/// or `pw::Result<T>` (for any T) which has the given status.
inline auto StatusIs(Status expected_status) {
  return ::testing::MakePolymorphicMatcher(
      internal::StatusIsMatcher(expected_status));
}

}  // namespace pw::unit_test