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
#include "pw_unit_test/googletest_test_matchers.h"

#include <cstdlib>

#include "gtest/gtest-spi.h"
#include "pw_status/status.h"

namespace {

using ::pw::OkStatus;
using ::pw::Result;
using ::pw::Status;
using ::pw::StatusWithSize;
using ::pw::unit_test::IsOkAndHolds;
using ::pw::unit_test::StatusIs;
using ::testing::Eq;
using ::testing::Not;

TEST(TestMatchers, AssertOk) { ASSERT_OK(OkStatus()); }
TEST(TestMatchers, AssertOkStatusWithSize) { ASSERT_OK(StatusWithSize(123)); }
TEST(TestMatchers, AssertOkResult) { ASSERT_OK(Result<int>(123)); }

TEST(TestMatchers, ExpectOk) { EXPECT_OK(OkStatus()); }
TEST(TestMatchers, ExpectOkStatusWithSize) { EXPECT_OK(StatusWithSize(123)); }
TEST(TestMatchers, ExpectOkResult) { EXPECT_OK(Result<int>(123)); }

TEST(TestMatchers, StatusIsSuccess) {
  EXPECT_THAT(OkStatus(), StatusIs(OkStatus()));
  EXPECT_THAT(Status::Cancelled(), StatusIs(Status::Cancelled()));
  EXPECT_THAT(Status::Unknown(), StatusIs(Status::Unknown()));
  EXPECT_THAT(Status::InvalidArgument(), StatusIs(Status::InvalidArgument()));
  EXPECT_THAT(Status::DeadlineExceeded(), StatusIs(Status::DeadlineExceeded()));
  EXPECT_THAT(Status::NotFound(), StatusIs(Status::NotFound()));
  EXPECT_THAT(Status::AlreadyExists(), StatusIs(Status::AlreadyExists()));
  EXPECT_THAT(Status::PermissionDenied(), StatusIs(Status::PermissionDenied()));
  EXPECT_THAT(Status::ResourceExhausted(),
              StatusIs(Status::ResourceExhausted()));
  EXPECT_THAT(Status::FailedPrecondition(),
              StatusIs(Status::FailedPrecondition()));
  EXPECT_THAT(Status::Aborted(), StatusIs(Status::Aborted()));
  EXPECT_THAT(Status::OutOfRange(), StatusIs(Status::OutOfRange()));
  EXPECT_THAT(Status::Unimplemented(), StatusIs(Status::Unimplemented()));
  EXPECT_THAT(Status::Internal(), StatusIs(Status::Internal()));
  EXPECT_THAT(Status::Unavailable(), StatusIs(Status::Unavailable()));
  EXPECT_THAT(Status::DataLoss(), StatusIs(Status::DataLoss()));
  EXPECT_THAT(Status::Unauthenticated(), StatusIs(Status::Unauthenticated()));
}

TEST(TestMatchers, StatusIsSuccessStatusWithSize) {
  EXPECT_THAT(StatusWithSize(), StatusIs(OkStatus()));
  EXPECT_THAT(StatusWithSize::Cancelled(), StatusIs(Status::Cancelled()));
  EXPECT_THAT(StatusWithSize::Unknown(), StatusIs(Status::Unknown()));
  EXPECT_THAT(StatusWithSize::InvalidArgument(),
              StatusIs(Status::InvalidArgument()));
  EXPECT_THAT(StatusWithSize::DeadlineExceeded(),
              StatusIs(Status::DeadlineExceeded()));
  EXPECT_THAT(StatusWithSize::NotFound(), StatusIs(Status::NotFound()));
  EXPECT_THAT(StatusWithSize::AlreadyExists(),
              StatusIs(Status::AlreadyExists()));
  EXPECT_THAT(StatusWithSize::PermissionDenied(),
              StatusIs(Status::PermissionDenied()));
  EXPECT_THAT(StatusWithSize::ResourceExhausted(),
              StatusIs(Status::ResourceExhausted()));
  EXPECT_THAT(StatusWithSize::FailedPrecondition(),
              StatusIs(Status::FailedPrecondition()));
  EXPECT_THAT(StatusWithSize::Aborted(), StatusIs(Status::Aborted()));
  EXPECT_THAT(StatusWithSize::OutOfRange(), StatusIs(Status::OutOfRange()));
  EXPECT_THAT(StatusWithSize::Unimplemented(),
              StatusIs(Status::Unimplemented()));
  EXPECT_THAT(StatusWithSize::Internal(), StatusIs(Status::Internal()));
  EXPECT_THAT(StatusWithSize::Unavailable(), StatusIs(Status::Unavailable()));
  EXPECT_THAT(StatusWithSize::DataLoss(), StatusIs(Status::DataLoss()));
  EXPECT_THAT(StatusWithSize::Unauthenticated(),
              StatusIs(Status::Unauthenticated()));
}

TEST(TestMatchers, StatusIsSuccessOkResult) {
  const Result<int> result = 46;
  EXPECT_THAT(result, StatusIs(OkStatus()));
}

TEST(TestMatchers, StatusIsSuccessResult) {
  EXPECT_THAT(Result<int>(Status::Cancelled()), StatusIs(Status::Cancelled()));
  EXPECT_THAT(Result<int>(Status::Unknown()), StatusIs(Status::Unknown()));
  EXPECT_THAT(Result<int>(Status::InvalidArgument()),
              StatusIs(Status::InvalidArgument()));
  EXPECT_THAT(Result<int>(Status::DeadlineExceeded()),
              StatusIs(Status::DeadlineExceeded()));
  EXPECT_THAT(Result<int>(Status::NotFound()), StatusIs(Status::NotFound()));
  EXPECT_THAT(Result<int>(Status::AlreadyExists()),
              StatusIs(Status::AlreadyExists()));
  EXPECT_THAT(Result<int>(Status::PermissionDenied()),
              StatusIs(Status::PermissionDenied()));
  EXPECT_THAT(Result<int>(Status::ResourceExhausted()),
              StatusIs(Status::ResourceExhausted()));
  EXPECT_THAT(Result<int>(Status::FailedPrecondition()),
              StatusIs(Status::FailedPrecondition()));
  EXPECT_THAT(Result<int>(Status::Aborted()), StatusIs(Status::Aborted()));
  EXPECT_THAT(Result<int>(Status::OutOfRange()),
              StatusIs(Status::OutOfRange()));
  EXPECT_THAT(Result<int>(Status::Unimplemented()),
              StatusIs(Status::Unimplemented()));
  EXPECT_THAT(Result<int>(Status::Internal()), StatusIs(Status::Internal()));
  EXPECT_THAT(Result<int>(Status::Unavailable()),
              StatusIs(Status::Unavailable()));
  EXPECT_THAT(Result<int>(Status::DataLoss()), StatusIs(Status::DataLoss()));
  EXPECT_THAT(Result<int>(Status::Unauthenticated()),
              StatusIs(Status::Unauthenticated()));
}

TEST(IsOkAndHolds, StatusWithSize) {
  const auto status_with_size = StatusWithSize{OkStatus(), 42};
  EXPECT_THAT(status_with_size, IsOkAndHolds(Eq(42u)));
}

TEST(IsOkAndHolds, Result) {
  auto value = Result<int>{42};
  EXPECT_THAT(value, IsOkAndHolds(Eq(42)));
}

TEST(IsOkAndHolds, BadStatusWithSize) {
  const auto status_with_size = StatusWithSize{Status::InvalidArgument(), 0};
  EXPECT_THAT(status_with_size, Not(IsOkAndHolds(Eq(42u))));
}

TEST(IsOkAndHolds, WrongStatusWithSize) {
  const auto status_with_size = StatusWithSize{OkStatus(), 100};
  EXPECT_THAT(status_with_size, IsOkAndHolds(Not(Eq(42u))));
  EXPECT_THAT(status_with_size, Not(IsOkAndHolds(Eq(42u))));
}

TEST(IsOkAndHolds, BadResult) {
  const auto value = Result<int>{Status::InvalidArgument()};
  EXPECT_THAT(value, Not(IsOkAndHolds(Eq(42))));
}

TEST(IsOkAndHolds, WrongResult) {
  const auto value = Result<int>{100};
  EXPECT_THAT(value, IsOkAndHolds(Not(Eq(42))));
  EXPECT_THAT(value, Not(IsOkAndHolds(Eq(42))));
}

TEST(AssertOkAndAssign, AssignsOkValueToNewLvalue) {
  const auto value = Result<int>(5);
  ASSERT_OK_AND_ASSIGN(int declare_and_assign, value);
  EXPECT_EQ(5, declare_and_assign);
}

TEST(AssertOkAndAssign, AssignsOkValueToExistingLvalue) {
  const auto value = Result<int>(5);
  int existing_value = 0;
  ASSERT_OK_AND_ASSIGN(existing_value, value);
  EXPECT_EQ(5, existing_value);
}

TEST(AssertOkAndAssign, AssignsExistingLvalueToConstReference) {
  const auto value = Result<int>(5);
  ASSERT_OK_AND_ASSIGN(const auto& ref, value);
  EXPECT_EQ(5, ref);
}

void AssertOkAndAssignInternally(Result<int> my_result_name) {
  ASSERT_OK_AND_ASSIGN([[maybe_unused]] int _unused, my_result_name);
}

TEST(AssertOkAndAssign, AssertFailsOnNonOkStatus) {
  EXPECT_FATAL_FAILURE(AssertOkAndAssignInternally(Status::InvalidArgument()),
                       "`my_result_name` is not OK: INVALID_ARGUMENT");
}

class CopyMoveCounter {
 public:
  CopyMoveCounter() = delete;
  CopyMoveCounter(int& copies, int& moves) : copies_(&copies), moves_(&moves) {}
  CopyMoveCounter(const CopyMoveCounter& other)
      : copies_(other.copies_), moves_(other.moves_) {
    ++(*copies_);
  }
  CopyMoveCounter(CopyMoveCounter&& other)
      : copies_(other.copies_), moves_(other.moves_) {
    ++(*moves_);
  }
  CopyMoveCounter& operator=(const CopyMoveCounter& other) {
    copies_ = other.copies_;
    moves_ = other.moves_;
    ++(*copies_);
    return *this;
  }
  CopyMoveCounter& operator=(CopyMoveCounter&& other) {
    copies_ = other.copies_;
    moves_ = other.moves_;
    ++(*moves_);
    return *this;
  }

 private:
  int* copies_;
  int* moves_;
};

TEST(AssertOkAndAssign, OkRvalueDoesNotCopy) {
  int copies = 0;
  int moves = 0;
  ASSERT_OK_AND_ASSIGN([[maybe_unused]] CopyMoveCounter cm,
                       Result(CopyMoveCounter(copies, moves)));
  EXPECT_EQ(copies, 0);
  EXPECT_EQ(moves, 2);
}

TEST(AssertOkAndAssign, OkLvalueMovedDoesNotCopy) {
  int copies = 0;
  int moves = 0;
  Result result(CopyMoveCounter(copies, moves));
  ASSERT_OK_AND_ASSIGN([[maybe_unused]] CopyMoveCounter cm, std::move(result));
  EXPECT_EQ(copies, 0);
  EXPECT_EQ(moves, 3);
}

TEST(AssertOkAndAssign, OkLvalueCopiesOnce) {
  int copies = 0;
  int moves = 0;
  Result result(CopyMoveCounter(copies, moves));
  ASSERT_OK_AND_ASSIGN([[maybe_unused]] CopyMoveCounter cm, result);
  EXPECT_EQ(copies, 1);
  EXPECT_EQ(moves, 2);
}

// The following test is commented out and is only for checking what
// failure cases would look like. For example, when uncommenting the test,
// the output is:
//
// ERR  pw_unit_test/googletest_test_matchers_test.cc:50: Failure
// ERR        Expected:
// ERR          Actual: Value of: OkStatus()
// Expected: has status UNKNOWN
//   Actual: 4-byte object <00-00 00-00>, which has status OK

// ERR  pw_unit_test/googletest_test_matchers_test.cc:51: Failure
// ERR        Expected:
// ERR          Actual: Value of: Status::Unknown()
// Expected: is OK
//   Actual: 4-byte object <02-00 00-00>, which has status UNKNOWN

// ERR  pw_unit_test/googletest_test_matchers_test.cc:52: Failure
// ERR        Expected:
// ERR          Actual: Value of: Status::Unknown()
// Expected: is OK
//   Actual: 4-byte object <02-00 00-00>, which has status UNKNOWN
//
// TEST(TestMatchers, SampleFailures) {
//   EXPECT_THAT(OkStatus(), StatusIs(Status::Unknown()));
//   EXPECT_OK(Status::Unknown());
//   ASSERT_OK(Status::Unknown());
// }

}  // namespace
