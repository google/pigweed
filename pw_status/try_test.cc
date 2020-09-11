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

#include "pw_status/try.h"

#include "gtest/gtest.h"

namespace pw {
namespace {

Status ReturnStatus(Status status) { return status; }
StatusWithSize ReturnStatusWithSize(StatusWithSize status) { return status; }

Status TryStatus(Status status) {
  TRY(ReturnStatus(status));

  // Any status other than OK should have already returned.
  EXPECT_EQ(status, Status::OK);
  return status;
}

Status TryStatus(StatusWithSize status) {
  TRY(ReturnStatusWithSize(status));

  // Any status other than OK should have already returned.
  EXPECT_EQ(status.status(), Status::OK);
  return status.status();
}

TEST(Status, Try_Status) {
  EXPECT_EQ(TryStatus(Status::OK), Status::OK);

  // Don't need all the status types, just pick a few not-ok ones.
  EXPECT_EQ(TryStatus(Status::CANCELLED), Status::CANCELLED);
  EXPECT_EQ(TryStatus(Status::DATA_LOSS), Status::DATA_LOSS);
  EXPECT_EQ(TryStatus(Status::UNIMPLEMENTED), Status::UNIMPLEMENTED);
}

TEST(Status, Try_StatusWithSizeOk) {
  for (size_t i = 0; i < 32; ++i) {
    StatusWithSize val(Status::OK, 0);
    EXPECT_EQ(TryStatus(val), Status::OK);
  }
}

TEST(Status, Try_StatusWithSizeError) {
  for (size_t i = 0; i < 32; ++i) {
    StatusWithSize val(Status::DATA_LOSS, i);
    EXPECT_EQ(TryStatus(val), Status::DATA_LOSS);
  }
}

TEST(Status, Try_StatusWithSizeFromConstant) {
  // Don't need all the status types, just pick a few not-ok ones.
  EXPECT_EQ(TryStatus(StatusWithSize::CANCELLED), Status::CANCELLED);
  EXPECT_EQ(TryStatus(StatusWithSize::DATA_LOSS), Status::DATA_LOSS);
  EXPECT_EQ(TryStatus(StatusWithSize::UNIMPLEMENTED), Status::UNIMPLEMENTED);
}

Status TryStatusAssign(size_t& size_val, StatusWithSize status) {
  PW_TRY_ASSIGN(size_val, ReturnStatusWithSize(status));

  // Any status other than OK should have already returned.
  EXPECT_EQ(status.status(), Status::OK);
  EXPECT_EQ(size_val, status.size());
  return status.status();
}

TEST(Status, TryAssignOk) {
  size_t size_val = 0;

  for (size_t i = 1; i < 32; ++i) {
    StatusWithSize val(Status::OK, i);
    EXPECT_EQ(TryStatusAssign(size_val, val), Status::OK);
    EXPECT_EQ(size_val, i);
  }
}

TEST(Status, TryAssignError) {
  size_t size_val = 0u;

  for (size_t i = 1; i < 32; ++i) {
    StatusWithSize val(Status::OUT_OF_RANGE, i);
    EXPECT_EQ(TryStatusAssign(size_val, val), Status::OUT_OF_RANGE);
    EXPECT_EQ(size_val, 0u);
  }
}

StatusWithSize TryStatusWithSize(StatusWithSize status) {
  TRY_WITH_SIZE(ReturnStatusWithSize(status));

  // Any status other than OK should have already returned.
  EXPECT_TRUE(status.ok());
  return status;
}

StatusWithSize TryStatusWithSize(Status status) {
  TRY_WITH_SIZE(ReturnStatus(status));

  // Any status other than OK should have already returned.
  EXPECT_EQ(status, Status::OK);

  StatusWithSize return_val(status, 0u);
  return return_val;
}

TEST(Status, TryWithSize_StatusOk) {
  StatusWithSize result = TryStatusWithSize(Status::OK);
  EXPECT_EQ(result.status(), Status::OK);
  EXPECT_EQ(result.size(), 0u);
}

TEST(Status, TryWithSize_StatusError) {
  StatusWithSize result = TryStatusWithSize(Status::PERMISSION_DENIED);
  EXPECT_EQ(result.status(), Status::PERMISSION_DENIED);
  EXPECT_EQ(result.size(), 0u);
}

TEST(Status, TryWithSize_StatusWithSizeOk) {
  for (size_t i = 0; i < 32; ++i) {
    StatusWithSize val(Status::OK, i);
    EXPECT_EQ(TryStatusWithSize(val).status(), Status::OK);
    EXPECT_EQ(TryStatusWithSize(val).size(), i);
  }
}

TEST(Status, TryWithSize_StatusWithSizeError) {
  for (size_t i = 0; i < 32; ++i) {
    StatusWithSize val(Status::DATA_LOSS, i);
    StatusWithSize result = TryStatusWithSize(val);
    EXPECT_EQ(result.status(), Status::DATA_LOSS);
    EXPECT_EQ(result.size(), i);
  }
}

TEST(Status, TryWithSize_StatusWithSizeConst) {
  StatusWithSize result = TryStatusWithSize(StatusWithSize::DATA_LOSS);
  EXPECT_EQ(result.status(), Status::DATA_LOSS);
  EXPECT_EQ(result.size(), 0u);

  result = TryStatusWithSize(StatusWithSize::NOT_FOUND);
  EXPECT_EQ(result.status(), Status::NOT_FOUND);
  EXPECT_EQ(result.size(), 0u);

  result = TryStatusWithSize(StatusWithSize::UNIMPLEMENTED);
  EXPECT_EQ(result.status(), Status::UNIMPLEMENTED);
  EXPECT_EQ(result.size(), 0u);
}

}  // namespace
}  // namespace pw
