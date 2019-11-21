// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "status.h"

#include "gtest/gtest.h"

namespace bt {
namespace {

enum class TestError : uint8_t {
  kFoo = 0,
  kBar = 1,
};

using TestStatus = Status<TestError>;

TEST(StatusTest, Success) {
  TestStatus status;
  EXPECT_TRUE(status);
  EXPECT_EQ(HostError::kNoError, status.error());
}

TEST(StatusTest, HostError) {
  TestStatus status(HostError::kTimedOut);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kTimedOut, status.error());
}

TEST(StatusTest, ProtocolError) {
  TestStatus status(TestError::kFoo);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kProtocolError, status.error());
  EXPECT_EQ(TestError::kFoo, status.protocol_error());
}

TEST(StatusTest, ProtocolErrorAsInt) {
  constexpr uint8_t kError = 1;
  Status<uint8_t> status(kError);
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kProtocolError, status.error());
  EXPECT_EQ(kError, status.protocol_error());
}

TEST(StatusTest, CompareEquality) {
  EXPECT_EQ(TestStatus(), TestStatus());
  EXPECT_NE(TestStatus(TestError::kFoo), TestStatus());

  EXPECT_NE(TestStatus(HostError::kFailed), TestStatus(HostError::kTimedOut));
  EXPECT_NE(TestStatus(HostError::kFailed), TestStatus(TestError::kFoo));

  EXPECT_EQ(TestStatus(TestError::kFoo), TestStatus(TestError::kFoo));
  EXPECT_NE(TestStatus(TestError::kFoo), TestStatus(TestError::kBar));

  // This should not compile.
  // EXPECT_EQ(TestStatus(), Status<uint8_t>());
}

}  // namespace
}  // namespace bt
