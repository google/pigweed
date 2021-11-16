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
  static std::string ToString(TestError code) {
    switch (code) {
      case TestError::kSuccess:
        return "success (TestError 0)";
      case TestError::kFail1:
        return "fail 1 (TestError 1)";
      case TestError::kFail2:
        return "fail 2 (TestError 2)";
      default:
        return "unknown (TestError)";
    }
  }

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

TEST(ErrorTest, ResultFromNonSuccessGeneralHostError) {
  // Create a result that can hold and only holds a HostError
  constexpr fitx::result result = ToResult(HostError::kFailed);
  ASSERT_TRUE(result.is_error());

  // Unwrap the result then access Error::is(…)
  constexpr Error general_error = result.error_value();
  ASSERT_TRUE(general_error.is_host_error());
  EXPECT_FALSE(general_error.is_protocol_error());
  EXPECT_EQ(HostError::kFailed, general_error.host_error());
  EXPECT_TRUE(general_error.is(HostError::kFailed));
  EXPECT_FALSE(general_error.is(HostError::kNoError));
  EXPECT_FALSE(general_error.is(HostError::kTimedOut));

  // Compare result to error
  EXPECT_EQ(general_error, result);
  EXPECT_EQ(result, general_error);

  // Create a specific kind of Error from the only-HostError-holding Error
  constexpr Error<TestError> specific_error = general_error;
  EXPECT_TRUE(specific_error.is(HostError::kFailed));
  EXPECT_EQ(general_error, specific_error);
  EXPECT_EQ(specific_error, general_error);
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
  EXPECT_FALSE(result == ToResult(HostError::kCanceled));
  EXPECT_FALSE(result == ToResult(HostError::kNoError));

  // Use operator!= through GTest
  EXPECT_NE(ToResult<TestError>(HostError::kCanceled), result);
  EXPECT_NE(ToResult(TestError::kFail2), result);

  // Compare to a general result
  EXPECT_NE(ToResult(HostError::kCanceled), result);
  EXPECT_NE(ToResult(HostError::kNoError), result);

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

TEST(ErrorTest, ToResultFromLegacyStatusType) {
  EXPECT_EQ(fitx::ok(), ToResult(Status<TestError>()));
  EXPECT_EQ(ToResult(TestError::kFail1), ToResult(Status(TestError::kFail1)));
  EXPECT_EQ(ToResult(HostError::kCanceled), ToResult(Status<TestError>(HostError::kCanceled)));
  EXPECT_EQ(ToResult<TestError>(HostError::kCanceled),
            ToResult(Status<TestError>(HostError::kCanceled)));
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

TEST(ErrorTest, HostErrorToString) {
  constexpr Error error = MakeError(HostError::kFailed);
  EXPECT_EQ(HostErrorToString(error.host_error()), error.ToString());
}

TEST(ErrorTest, GeneralHostErrorToString) {
  constexpr Error error = ToResult(HostError::kFailed).error_value();
  EXPECT_EQ(HostErrorToString(error.host_error()), error.ToString());
}

TEST(ErrorTest, ProtocolErrorToString) {
  constexpr Error error = MakeError(TestError::kFail2);
  EXPECT_EQ(ProtocolErrorTraits<TestError>::ToString(TestError::kFail2), error.ToString());
}

TEST(ErrorTest, ToStringOnResult) {
  constexpr fitx::result proto_error_result = ToResult(TestError::kFail2);
  EXPECT_EQ("[result: fail 2 (TestError 2)]", internal::ToString(proto_error_result));
  constexpr fitx::result<Error<TestError>> success_result = fitx::ok();
  EXPECT_EQ("[result: success]", internal::ToString(success_result));
  constexpr fitx::result<Error<TestError>, int> success_result_with_value = fitx::ok(1);
  EXPECT_EQ("[result: success with value]", internal::ToString(success_result_with_value));
}

TEST(ErrorTest, BtIsErrorMacroCompiles) {
  const fitx::result general_error = ToResult(HostError::kFailed);
  EXPECT_TRUE(bt_is_error(general_error, ERROR, "ErrorTest", "error message"));
  const fitx::result<Error<TestError>, int> success_with_value = fitx::ok(1);
  EXPECT_FALSE(bt_is_error(success_with_value, ERROR, "ErrorTest", "error message"));
  const fitx::result<Error<TestError>, int> error_with_value =
      fitx::error(MakeError(TestError::kFail1));
  EXPECT_TRUE(bt_is_error(error_with_value, ERROR, "ErrorTest", "error message"));
}

TEST(ErrorTest, BtStrMacroOnResult) {
  constexpr fitx::result proto_error_result = ToResult(TestError::kFail2);
  EXPECT_EQ(internal::ToString(proto_error_result), bt_str(proto_error_result));
}

}  // namespace
}  // namespace bt
