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

#include "pw_status/status.h"

#include "gtest/gtest.h"

namespace pw {
namespace {

constexpr Status::Code kInvalidCode = static_cast<Status::Code>(30);

TEST(Status, Default) {
  Status status;
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(Status(), status);
}

TEST(Status, ConstructWithStatusCode) {
  Status status(Status::ABORTED);
  EXPECT_EQ(Status::ABORTED, status);
}

TEST(Status, AssignFromStatusCode) {
  Status status;
  status = Status::INTERNAL;
  EXPECT_EQ(Status::INTERNAL, status);
}

TEST(Status, CompareToStatusCode) {
  EXPECT_EQ(Status(), Status::OK);
  EXPECT_EQ(Status::ABORTED, Status(Status::ABORTED));
  EXPECT_NE(Status(), Status::ABORTED);
}

TEST(Status, Ok_OkIsTrue) {
  EXPECT_TRUE(Status().ok());
  EXPECT_TRUE(Status(Status::OK).ok());
}

TEST(Status, NotOk_OkIsFalse) {
  EXPECT_FALSE(Status(Status::DATA_LOSS).ok());
  EXPECT_FALSE(Status(kInvalidCode).ok());
}

TEST(Status, KnownString) {
  EXPECT_STREQ("OK", Status(Status::OK).str());
  EXPECT_STREQ("CANCELLED", Status(Status::CANCELLED).str());
  EXPECT_STREQ("DEADLINE_EXCEEDED", Status(Status::DEADLINE_EXCEEDED).str());
  EXPECT_STREQ("NOT_FOUND", Status(Status::NOT_FOUND).str());
  EXPECT_STREQ("ALREADY_EXISTS", Status(Status::ALREADY_EXISTS).str());
  EXPECT_STREQ("PERMISSION_DENIED", Status(Status::PERMISSION_DENIED).str());
  EXPECT_STREQ("UNAUTHENTICATED", Status(Status::UNAUTHENTICATED).str());
  EXPECT_STREQ("RESOURCE_EXHAUSTED", Status(Status::RESOURCE_EXHAUSTED).str());
  EXPECT_STREQ("FAILED_PRECONDITION",
               Status(Status::FAILED_PRECONDITION).str());
  EXPECT_STREQ("ABORTED", Status(Status::ABORTED).str());
  EXPECT_STREQ("OUT_OF_RANGE", Status(Status::OUT_OF_RANGE).str());
  EXPECT_STREQ("UNIMPLEMENTED", Status(Status::UNIMPLEMENTED).str());
  EXPECT_STREQ("INTERNAL", Status(Status::INTERNAL).str());
  EXPECT_STREQ("UNAVAILABLE", Status(Status::UNAVAILABLE).str());
  EXPECT_STREQ("DATA_LOSS", Status(Status::DATA_LOSS).str());
}

TEST(Status, UnknownString) {
  EXPECT_STREQ("INVALID STATUS", Status(static_cast<Status::Code>(30)).str());
}

// Functions for executing the C pw_Status tests.
extern "C" {

Status::Code PassStatusFromC(Status status);

Status::Code PassStatusFromCpp(Status status) { return status; }

int TestStatusFromC(void);

int TestStatusStringsFromC(void);

}  // extern "C"

TEST(StatusCLinkage, CallCFunctionWithStatus) {
  EXPECT_EQ(Status::ABORTED, PassStatusFromC(Status::ABORTED));
  EXPECT_EQ(Status::UNKNOWN, PassStatusFromC(Status(Status::UNKNOWN)));

  EXPECT_EQ(Status(Status::NOT_FOUND), PassStatusFromC(Status::NOT_FOUND));
  EXPECT_EQ(Status(Status::OK), PassStatusFromC(Status(Status::OK)));
}

TEST(StatusCLinkage, TestStatusFromC) { EXPECT_EQ(0, TestStatusFromC()); }

TEST(StatusCLinkage, TestStatusStringsFromC) {
  EXPECT_EQ(0, TestStatusStringsFromC());
}

}  // namespace
}  // namespace pw
