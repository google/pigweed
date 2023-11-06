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

// Gets the value of an object whose value is guarded by a ``pw::OkStatus()``.
// Used by Matchers.
constexpr size_t GetValue(StatusWithSize status_with_size) {
  return status_with_size.size();
}

template <typename V>
constexpr const V& GetValue(const Result<V>& result) {
  return result.value();
}

template <typename V>
constexpr V GetValue(Result<V>&& result) {
  return std::move(result).value();
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

// Implements IsOkAndHolds(m) as a monomorphic matcher.
template <typename StatusType>
class IsOkAndHoldsMatcherImpl {
 public:
  using is_gtest_matcher = void;
  using ValueType = decltype(GetValue(std::declval<StatusType>()));

  // NOLINTBEGIN(bugprone-forwarding-reference-overload)
  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const ValueType&>(
            std::forward<InnerMatcher>(inner_matcher))) {}
  // NOLINTEND(bugprone-forwarding-reference-overload)

  void DescribeTo(std::ostream* os) const {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(const StatusType& actual_value,
                       ::testing::MatchResultListener* listener) const {
    const auto& status = GetStatus(actual_value);
    if (!status.ok()) {
      *listener << "which has status " << pw_StatusString(status);
      return false;
    }

    const auto& value = GetValue(actual_value);
    *listener << "which contains value " << ::testing::PrintToString(value);

    ::testing::StringMatchResultListener inner_listener;
    const bool matches = inner_matcher_.MatchAndExplain(value, &inner_listener);
    const std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *listener << ", " << inner_explanation;
    }

    return matches;
  }

 private:
  const ::testing::Matcher<const ValueType&> inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
//
// We have to manually create it as a class instead of using the
// `::testing::MakePolymorphicMatcher()` helper because of the custom conversion
// to Matcher<T>.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // NOLINTBEGIN(google-explicit-constructor)
  template <typename StatusType>
  operator ::testing::Matcher<StatusType>() const {
    return ::testing::Matcher<StatusType>(
        internal::IsOkAndHoldsMatcherImpl<const StatusType&>(inner_matcher_));
  }
  // NOLINTEND(google-explicit-constructor)

 private:
  const InnerMatcher inner_matcher_;
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

/// Returns a gMock matcher that matches a `pw::StatusWithSize` or
/// `pw::Result<T>` (for any T) which is OK and holds a value matching the inner
/// matcher.
template <typename InnerMatcher>
inline internal::IsOkAndHoldsMatcher<InnerMatcher> IsOkAndHolds(
    InnerMatcher&& inner_matcher) {
  return internal::IsOkAndHoldsMatcher<InnerMatcher>(
      std::forward<InnerMatcher>(inner_matcher));
}

/// Executes an expression that returns a `pw::Result` or `pw::StatusWithSize`
/// and assigns or moves that value to lhs if the error code is OK. If the
// status is non-OK, generates a test failure and returns from the current
/// function, which must have a void return type.
///
/// The MOVE variant moves the content out of the `pw::Result` and into lhs.
/// This variant is required for move-only types.
//
/// Example: Declaring and initializing a new value. E.g.:
///   ASSERT_OK_AND_ASSIGN(auto value, MaybeGetValue(arg));
///   ASSERT_OK_AND_ASSIGN(const ValueType& value, MaybeGetValue(arg));
///   ASSERT_OK_AND_MOVE(auto ptr, MaybeGetUniquePtr(arg))
///
/// Example: Assigning to an existing value
///   ValueType value;
///   ASSERT_OK_AND_ASSIGN(value, MaybeGetValue(arg));
///
/// The value assignment example would expand into something like:
///   auto status_or_value = MaybeGetValue(arg);
///   ASSERT_OK(status_or_value.status());
///   value = status_or_value.ValueOrDie();
///
/// WARNING: ASSERT_OK_AND_ASSIGN (and the move variant) expand into multiple
///   statements; it cannot be used in a single statement (e.g. as the body of
///   an if statement without {})!
#define ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  ASSERT_OK_AND_ASSIGN_DETAIL(UNIQUE_IDENTIFIER_DETAIL(__LINE__), lhs, rexpr)
#define ASSERT_OK_AND_MOVE(lhs, rexpr) \
  ASSERT_OK_AND_MOVE_DETAIL(UNIQUE_IDENTIFIER_DETAIL(__LINE__), lhs, rexpr)

// NOLINTBEGIN(bugprone-macro-parentheses)
// The suggestion would produce bad code.
#define ASSERT_OK_AND_ASSIGN_DETAIL(result, lhs, rexpr) \
  const auto& result = (rexpr);                         \
  if (!result.ok()) {                                   \
    FAIL() << #rexpr << " is not OK.";                  \
  }                                                     \
  lhs = ::pw::unit_test::internal::GetValue(result);
#define ASSERT_OK_AND_MOVE_DETAIL(result, lhs, rexpr) \
  auto&& result = (rexpr);                            \
  if (!result.ok()) {                                 \
    FAIL() << #rexpr << " is not OK.";                \
  }                                                   \
  lhs = ::pw::unit_test::internal::GetValue(std::move(result));
// NOLINTEND(bugprone-macro-parentheses)

#define UNIQUE_IDENTIFIER_DETAIL(line) UNIQUE_IDENTIFIER_EXPANDED_DETAIL(line)
#define UNIQUE_IDENTIFIER_EXPANDED_DETAIL(line) \
  _assert_ok_and_assign_unique_name_##line

}  // namespace pw::unit_test