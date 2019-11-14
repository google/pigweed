// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.

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
  EXPECT_EQ(Status::ABORTED, status.code());
}

TEST(Status, AssignFromStatusCode) {
  Status status;
  status = Status::INTERNAL;
  EXPECT_EQ(Status::INTERNAL, status.code());
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
  EXPECT_EQ("OK", Status(Status::OK).str());
  EXPECT_EQ("CANCELLED", Status(Status::CANCELLED).str());
  EXPECT_EQ("DEADLINE_EXCEEDED", Status(Status::DEADLINE_EXCEEDED).str());
  EXPECT_EQ("NOT_FOUND", Status(Status::NOT_FOUND).str());
  EXPECT_EQ("ALREADY_EXISTS", Status(Status::ALREADY_EXISTS).str());
  EXPECT_EQ("PERMISSION_DENIED", Status(Status::PERMISSION_DENIED).str());
  EXPECT_EQ("UNAUTHENTICATED", Status(Status::UNAUTHENTICATED).str());
  EXPECT_EQ("RESOURCE_EXHAUSTED", Status(Status::RESOURCE_EXHAUSTED).str());
  EXPECT_EQ("FAILED_PRECONDITION", Status(Status::FAILED_PRECONDITION).str());
  EXPECT_EQ("ABORTED", Status(Status::ABORTED).str());
  EXPECT_EQ("OUT_OF_RANGE", Status(Status::OUT_OF_RANGE).str());
  EXPECT_EQ("UNIMPLEMENTED", Status(Status::UNIMPLEMENTED).str());
  EXPECT_EQ("INTERNAL", Status(Status::INTERNAL).str());
  EXPECT_EQ("UNAVAILABLE", Status(Status::UNAVAILABLE).str());
  EXPECT_EQ("DATA_LOSS", Status(Status::DATA_LOSS).str());
}

TEST(Status, UnknownString) {
  EXPECT_EQ("INVALID STATUS", Status(static_cast<Status::Code>(30)).str());
}

}  // namespace
}  // namespace pw
