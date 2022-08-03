// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "error.h"

#include <sstream>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/uuid.h"

namespace bt {
namespace {

enum class TestError : uint8_t {
  kSuccess = 0,
  kFail1 = 1,
  kFail2 = 2,
};

enum class TestErrorWithoutSuccess {
  kFail0 = 0,
  kFail1 = 1,
};

// Test detail::IsErrorV
static_assert(detail::IsErrorV<Error<TestError>>);
static_assert(!detail::IsErrorV<TestError>);

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

template <>
struct ProtocolErrorTraits<TestErrorWithoutSuccess> {
  static std::string ToString(TestErrorWithoutSuccess code) {
    switch (code) {
      case TestErrorWithoutSuccess::kFail0:
        return "fail 0 (TestErrorWithoutSuccess 0)";
      case TestErrorWithoutSuccess::kFail1:
        return "fail 1 (TestErrorWithoutSuccess 1)";
      default:
        return "unknown (TestError)";
    }
  }

  // is_success() is omitted
};

namespace {

// Build an Error when |proto_code| is guaranteed to be an error. This could be consteval in C++20
// so that if the code isn't an error, it doesn't compile.
constexpr Error<TestError> MakeError(TestError proto_code) {
  return ToResult(proto_code).error_value();
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
  ASSERT_EQ(fitx::ok(), result);

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
  EXPECT_FALSE(general_error.is(HostError::kTimedOut));

  // Compare result to error
  EXPECT_EQ(general_error, result);
  EXPECT_EQ(result, general_error);

  // Create a specific kind of Error from the only-HostError-holding Error
  constexpr Error<TestError> specific_error = general_error;
  EXPECT_TRUE(specific_error.is(HostError::kFailed));
  EXPECT_EQ(general_error, specific_error);
  EXPECT_EQ(specific_error, general_error);

  // Test operator!=
  constexpr Error different_specific_error = MakeError(TestError::kFail1);
  EXPECT_NE(general_error, different_specific_error);
  EXPECT_NE(different_specific_error, general_error);
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

  // Compare result to error
  EXPECT_EQ(error, result);
  EXPECT_EQ(result, error);
}

TEST(ErrorTest, ResultFromSuccessProtocolError) {
  constexpr fitx::result result = ToResult(TestError::kSuccess);
  ASSERT_EQ(fitx::ok(), result);

  // Compare result to error
  const Error error = MakeError(TestError::kFail1);
  EXPECT_NE(error, result);
  EXPECT_NE(result, error);
}

TEST(ErrorTest, ResultFromNonSuccessProtocolErrorThatOnlyHoldsErrors) {
  // Use public ctor to construct the error directly.
  constexpr Error<TestErrorWithoutSuccess> error(TestErrorWithoutSuccess::kFail0);
  EXPECT_TRUE(error.is(TestErrorWithoutSuccess::kFail0));
}

TEST(ErrorDeathTest, ReadingHostErrorThatIsNotPresentIsFatal) {
  const Error error = MakeError(TestError::kFail1);
  ASSERT_DEATH_IF_SUPPORTED([[maybe_unused]] auto _ = error.host_error(), "HostError");
}

TEST(ErrorDeathTest, ReadingProtocolErrorThatIsNotPresentIsFatal) {
  const Error<TestError> error(HostError::kFailed);
  ASSERT_DEATH_IF_SUPPORTED([[maybe_unused]] auto _ = error.protocol_error(), "protocol error");
}

TEST(ErrorTest, ResultIsAnyOf) {
  constexpr Error error = MakeError(TestError::kFail1);

  // None of the arguments compare equal to error's contents
  EXPECT_FALSE(error.is_any_of(HostError::kFailed, TestError::kFail2, TestError::kSuccess));

  // One argument matches
  EXPECT_TRUE(error.is_any_of(HostError::kFailed, TestError::kFail2, TestError::kFail1));
  EXPECT_TRUE(error.is_any_of(HostError::kFailed, TestError::kFail1, TestError::kFail2));
  EXPECT_TRUE(error.is_any_of(TestError::kFail1));
}

TEST(ErrorTest, ErrorCanBeComparedInTests) {
  const Error error = MakeError(TestError::kFail1);

  // Compare to HostError
  EXPECT_FALSE(error.is(HostError::kFailed));

  // Use operator== through GTest
  EXPECT_EQ(error, error);

  // Use operator!= through GTest
  EXPECT_NE(MakeError(TestError::kFail2), error);
  EXPECT_NE(Error<>(HostError::kFailed), error);
  EXPECT_NE(Error<TestError>(HostError::kFailed), error);
}

TEST(ErrorTest, ResultCanBeComparedInTests) {
  constexpr fitx::result result = ToResult(TestError::kFail1);

  // Use operator== through GTest
  EXPECT_EQ(result, result);

  // And explicitly
  EXPECT_FALSE(result == ToResult<TestError>(HostError::kCanceled));
  EXPECT_FALSE(result == ToResult(HostError::kCanceled));
  EXPECT_FALSE(result == fitx::ok());
  EXPECT_FALSE(result == fitx::result<Error<TestError>>(fitx::ok()));

  // Use operator!= through GTest
  EXPECT_NE(ToResult<TestError>(HostError::kCanceled), result);
  EXPECT_NE(ToResult(TestError::kFail2), result);

  // Compare to a general result
  EXPECT_NE(ToResult(HostError::kCanceled), result);
  EXPECT_NE(fitx::result<Error<NoProtocolError>>(fitx::ok()), result);

  // Compare results to fix::success
  EXPECT_NE(fitx::ok(), result);
  EXPECT_EQ(fitx::ok(), ToResult(TestError::kSuccess));

  const fitx::result<Error<TestError>, int> success_with_value = fitx::ok(1);
  const fitx::result<Error<TestError>, int> error_with_value =
      fitx::error(MakeError(TestError::kFail1));
  const fitx::result<Error<TestError>, int> different_error_with_value =
      fitx::error(MakeError(TestError::kFail2));
  EXPECT_EQ(success_with_value, success_with_value);
  EXPECT_NE(success_with_value, error_with_value);
  EXPECT_FALSE(success_with_value == error_with_value);
  EXPECT_NE(error_with_value, different_error_with_value);

  EXPECT_EQ(ToResult(TestError::kFail1).error_value(), error_with_value);
  EXPECT_NE(ToResult(TestError::kFail2).error_value(), error_with_value);

  const fitx::result<Error<TestError>, int> error_with_value_holding_host_error =
      fitx::error(Error<TestError>(HostError::kFailed));

  // ToResult(HostError) constructs a bt::Error<NoProtocolError> so comparisons must take this into
  // account.
  EXPECT_EQ(Error(HostError::kFailed), error_with_value_holding_host_error);
  EXPECT_NE(Error(HostError::kFailed), error_with_value);
}

TEST(ErrorTest, VisitOnHostError) {
  constexpr Error<TestError> error(HostError::kFailed);
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
  constexpr Error error(HostError::kFailed);
  EXPECT_EQ(HostErrorToString(error.host_error()), error.ToString());
}

TEST(ErrorTest, GeneralHostErrorToString) {
  constexpr Error error(HostError::kFailed);
  EXPECT_EQ(HostErrorToString(error.host_error()), error.ToString());
}

TEST(ErrorTest, ProtocolErrorToString) {
  constexpr Error error = MakeError(TestError::kFail2);
  EXPECT_EQ(ProtocolErrorTraits<TestError>::ToString(TestError::kFail2), error.ToString());

  // Test that GoogleTest's value printer converts to the same string
  EXPECT_EQ(internal::ToString(error), ::testing::PrintToString(error));

  // ostringstream::operator<< returns a ostream&, so test that our operator is compatible
  std::ostringstream oss;
  oss << error;
}

TEST(ErrorTest, ToStringOnResult) {
  constexpr fitx::result proto_error_result = ToResult(TestError::kFail2);
  EXPECT_EQ("[result: error(fail 2 (TestError 2))]", internal::ToString(proto_error_result));
  constexpr fitx::result<Error<TestError>> success_result = fitx::ok();
  EXPECT_EQ("[result: ok()]", internal::ToString(success_result));
  constexpr fitx::result<Error<TestError>, int> success_result_with_value = fitx::ok(1);
  EXPECT_EQ("[result: ok(?)]", internal::ToString(success_result_with_value));
  constexpr fitx::result<Error<TestError>, UUID> success_result_with_printable_value =
      fitx::ok(UUID(uint16_t{}));
  EXPECT_EQ("[result: ok(00000000-0000-1000-8000-00805f9b34fb)]",
            internal::ToString(success_result_with_printable_value));

  // Test that GoogleTest's value printer converts to the same string
  EXPECT_EQ(internal::ToString(proto_error_result), ::testing::PrintToString(proto_error_result));
  EXPECT_EQ(internal::ToString(success_result), ::testing::PrintToString(success_result));
  EXPECT_EQ(internal::ToString(success_result_with_printable_value),
            ::testing::PrintToString(success_result_with_printable_value));

  // The value printer will try to stream types to the GoogleTest ostream if possible, so it may not
  // always match bt::internal::ToString's output.
  EXPECT_EQ("[result: ok(1)]", ::testing::PrintToString(success_result_with_value));
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

TEST(ErrorTest, BtIsErrorOnlyEvaluatesResultOnce) {
  int result_count = 0;
  auto result_func = [&]() {
    result_count++;
    return ToResult(TestError::kFail1);
  };
  bt_is_error(result_func(), ERROR, "ErrorTest", "error message");
  EXPECT_EQ(result_count, 1);
}

TEST(ErrorTest, BtStrMacroOnResult) {
  constexpr fitx::result proto_error_result = ToResult(TestError::kFail2);
  EXPECT_EQ(internal::ToString(proto_error_result), bt_str(proto_error_result));
}

}  // namespace
}  // namespace bt
