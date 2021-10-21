// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "error.h"

#include <gtest/gtest.h>

#include "lib/fitx/internal/result.h"

namespace bt {
namespace {

enum class TestError : uint8_t {
  kSuccess = 0,
  kFail1 = 1,
  kFail2 = 2,
};

}  // namespace

// This template specialization must be in ::bt for name lookup reasons
template <>
struct ProtocolErrorTraits<TestError> {
  static constexpr bool is_success(TestError proto_code) {
    return proto_code == TestError::kSuccess;
  }
};

namespace {

// Build an Error when |proto_code| is guaranteed to be an error
constexpr Error<TestError> MakeError(TestError proto_code) {
  return ToResult(proto_code).error_value();
}

constexpr Error<TestError> MakeError(HostError host_code) {
  return ToResult<TestError>(host_code).error_value();
}

TEST(ErrorTest, ResultFromNonSuccessHostError) {
  // Create a result that can hold TestError but which holds a HostError
  constexpr fitx::result result = ToResult<TestError>(HostError::kFailed);
  ASSERT_TRUE(result.is_error());

  // Unwrap the result then access Error::is(…)
  constexpr Error error = result.error_value();
  ASSERT_TRUE(error.is_host_error());
  EXPECT_FALSE(error.is_protocol_error());
  EXPECT_EQ(HostError::kFailed, error.host_error());
  EXPECT_TRUE(error.is(HostError::kFailed));
  EXPECT_FALSE(error.is(HostError::kNoError));
  EXPECT_FALSE(error.is(HostError::kTimedOut));

  // Compare to protocol error
  EXPECT_FALSE(error.is(TestError::kFail1));
  EXPECT_FALSE(error.is(TestError::kSuccess));

  // Compare result to error
  EXPECT_EQ(error, result);
  EXPECT_EQ(result, error);
}

TEST(ErrorTest, ResultFromSuccessHostError) {
  constexpr fitx::result result = ToResult<TestError>(TestError::kSuccess);
  ASSERT_TRUE(result.is_ok());

  // Compare result to error
  const Error error = MakeError(TestError::kFail1);
  EXPECT_NE(error, result);
  EXPECT_NE(result, error);
}

TEST(ErrorTest, ResultFromNonSuccessProtocolError) {
  constexpr fitx::result result = ToResult(TestError::kFail1);
  ASSERT_TRUE(result.is_error());

  // Unwrap the result then access Error::is(…)
  constexpr Error error = result.error_value();
  ASSERT_TRUE(error.is_protocol_error());
  EXPECT_FALSE(error.is_host_error());
  EXPECT_EQ(TestError::kFail1, error.protocol_error());
  EXPECT_TRUE(error.is(TestError::kFail1));
  EXPECT_FALSE(error.is(TestError::kSuccess));
  EXPECT_FALSE(error.is(TestError::kFail2));

  // Compare to HostError
  EXPECT_FALSE(error.is(HostError::kFailed));
  EXPECT_FALSE(error.is(HostError::kNoError));

  // Compare result to error
  EXPECT_EQ(error, result);
  EXPECT_EQ(result, error);
}

TEST(ErrorTest, ResultFromSuccessProtocolError) {
  constexpr fitx::result result = ToResult(TestError::kSuccess);
  ASSERT_TRUE(result.is_ok());

  // Compare result to error
  const Error error = MakeError(TestError::kFail1);
  EXPECT_NE(error, result);
  EXPECT_NE(result, error);
}

TEST(ErrorDeathTest, ReadingHostErrorThatIsNotPresentIsFatal) {
  const Error error = MakeError(TestError::kFail1);
  ASSERT_DEATH_IF_SUPPORTED([[maybe_unused]] auto _ = error.host_error(), "HostError");
}

TEST(ErrorDeathTest, ReadingProtocolErrorThatIsNotPresentIsFatal) {
  const Error error = MakeError(HostError::kFailed);
  ASSERT_DEATH_IF_SUPPORTED([[maybe_unused]] auto _ = error.protocol_error(), "protocol error");
}

TEST(ErrorTest, ErrorCanBeComparedInTests) {
  const Error error = MakeError(TestError::kFail1);

  // Compare to HostError
  EXPECT_FALSE(error.is(HostError::kFailed));

  // Use operator== through GTest
  EXPECT_EQ(error, error);
  EXPECT_EQ(TestError::kFail1, error);

  // Use operator!= through GTest
  EXPECT_NE(TestError::kSuccess, error);
  EXPECT_NE(TestError::kFail2, error);
  EXPECT_NE(MakeError(TestError::kFail2), error);
}

TEST(ErrorTest, ResultCanBeComparedInTests) {
  constexpr fitx::result result = ToResult(TestError::kFail1);

  // Use operator== through GTest
  EXPECT_EQ(result, result);

  // And explicitly
  EXPECT_FALSE(result == ToResult<TestError>(HostError::kCanceled));

  // Use operator!= through GTest
  EXPECT_NE(ToResult<TestError>(HostError::kCanceled), result);
  EXPECT_NE(ToResult(TestError::kFail2), result);

  // Compare results to fix::success
  EXPECT_NE(fitx::ok(), result);
  EXPECT_EQ(fitx::ok(), ToResult(TestError::kSuccess));

  const fitx::result<Error<TestError>, int> success_with_value = fitx::ok(1);
  const fitx::result<Error<TestError>, int> error_with_value =
      fitx::error(MakeError(TestError::kFail1));
  const fitx::result<Error<TestError>, int> different_error_with_value =
      fitx::error(MakeError(TestError::kFail2));
  EXPECT_NE(success_with_value, error_with_value);
  EXPECT_FALSE(success_with_value == error_with_value);
  EXPECT_NE(error_with_value, different_error_with_value);
}

TEST(ErrorTest, VisitOnHostError) {
  constexpr Error error = MakeError(HostError::kFailed);
  ASSERT_TRUE(error.is_host_error());

  bool host_visited = false;
  bool proto_visited = false;
  error.Visit([&host_visited](HostError) { host_visited = true; },
              [&proto_visited](TestError) { proto_visited = true; });
  EXPECT_TRUE(host_visited);
  EXPECT_FALSE(proto_visited);
}

TEST(ErrorTest, VisitOnProtoError) {
  constexpr Error error = MakeError(TestError::kFail1);
  ASSERT_TRUE(error.is_protocol_error());

  bool host_visited = false;
  bool proto_visited = false;
  error.Visit([&host_visited](HostError) { host_visited = true; },
              [&proto_visited](TestError) { proto_visited = true; });
  EXPECT_FALSE(host_visited);
  EXPECT_TRUE(proto_visited);
}

}  // namespace
}  // namespace bt
