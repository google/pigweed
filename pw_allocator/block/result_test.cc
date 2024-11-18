// Copyright 2024 The Pigweed Authors
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

#include "pw_allocator/block/result.h"

#include <array>
#include <cstddef>

#include "public/pw_allocator/block/basic.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::Status;

struct FakeBlock : public ::pw::allocator::BasicBlock<FakeBlock> {
  static constexpr size_t DefaultAlignment() { return 1; }
  static constexpr size_t BlockOverhead() { return 0; }
  static constexpr size_t MinInnerSize() { return 0; }
  size_t OuterSizeUnchecked() const { return 1; }
};

using BlockResult = ::pw::allocator::BlockResult<FakeBlock>;

const std::array<Status, 17> kStatuses = {
    pw::OkStatus(),
    Status::Cancelled(),
    Status::Unknown(),
    Status::InvalidArgument(),
    Status::DeadlineExceeded(),
    Status::NotFound(),
    Status::AlreadyExists(),
    Status::PermissionDenied(),
    Status::ResourceExhausted(),
    Status::FailedPrecondition(),
    Status::Aborted(),
    Status::OutOfRange(),
    Status::Unimplemented(),
    Status::Internal(),
    Status::Unavailable(),
    Status::DataLoss(),
    Status::Unauthenticated(),
};

const std::array<BlockResult::Prev, 4> kPrevs = {
    BlockResult::Prev::kUnchanged,
    BlockResult::Prev::kSplitNew,
    BlockResult::Prev::kResizedSmaller,
    BlockResult::Prev::kResizedLarger,
};

const std::array<BlockResult::Next, 4> kNexts = {
    BlockResult::Next::kUnchanged,
    BlockResult::Next::kSplitNew,
    BlockResult::Next::kResized,
    BlockResult::Next::kMerged,
};

const std::array<size_t, 4> kSizes = {
    0,
    1,
    8,
    (1U << 10) - 1,
};

TEST(BlockResultTest, ConstructWithBlockOnly) {
  FakeBlock block;
  BlockResult result(&block);
  EXPECT_EQ(result.block(), &block);
  EXPECT_EQ(result.status(), pw::OkStatus());
  EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
  EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);
  EXPECT_EQ(result.size(), 0U);
}

TEST(BlockResultTest, ConstructWithBlockAndStatus) {
  FakeBlock block;
  for (const auto& status : kStatuses) {
    BlockResult result(&block, status);
    EXPECT_EQ(result.block(), &block);
    EXPECT_EQ(result.status(), status);
    EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
    EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);
    EXPECT_EQ(result.size(), 0U);
  }
}

TEST(BlockResultTest, ConstructWithBlockAndPrev) {
  FakeBlock block;
  for (const auto& prev : kPrevs) {
    BlockResult result(&block, prev);
    EXPECT_EQ(result.block(), &block);
    EXPECT_EQ(result.status(), pw::OkStatus());
    EXPECT_EQ(result.prev(), prev);
    EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);
    EXPECT_EQ(result.size(), 0U);
  }
}

TEST(BlockResultTest, ConstructWithBlockPrevAndSize) {
  FakeBlock block;
  for (const auto& prev : kPrevs) {
    for (const auto& size : kSizes) {
      BlockResult result(&block, prev, size);
      EXPECT_EQ(result.block(), &block);
      EXPECT_EQ(result.status(), pw::OkStatus());
      EXPECT_EQ(result.prev(), prev);
      EXPECT_EQ(result.next(), BlockResult::Next::kUnchanged);
      EXPECT_EQ(result.size(), size);
    }
  }
}

TEST(BlockResultTest, ConstructWithBlockAndNext) {
  FakeBlock block;
  for (const auto& next : kNexts) {
    BlockResult result(&block, next);
    EXPECT_EQ(result.block(), &block);
    EXPECT_EQ(result.status(), pw::OkStatus());
    EXPECT_EQ(result.prev(), BlockResult::Prev::kUnchanged);
    EXPECT_EQ(result.next(), next);
    EXPECT_EQ(result.size(), 0U);
  }
}

TEST(BlockResultTest, ConstructWithBlockPrevAndNext) {
  FakeBlock block;
  for (const auto& prev : kPrevs) {
    for (const auto& next : kNexts) {
      BlockResult result(&block, prev, next);
      EXPECT_EQ(result.block(), &block);
      EXPECT_EQ(result.status(), pw::OkStatus());
      EXPECT_EQ(result.prev(), prev);
      EXPECT_EQ(result.next(), next);
      EXPECT_EQ(result.size(), 0U);
    }
  }
}

TEST(BlockResultTest, ConstructWithBlockPrevNextAndSize) {
  FakeBlock block;
  for (const auto& prev : kPrevs) {
    for (const auto& next : kNexts) {
      for (const auto& size : kSizes) {
        BlockResult result(&block, prev, next, size);
        EXPECT_EQ(result.block(), &block);
        EXPECT_EQ(result.status(), pw::OkStatus());
        EXPECT_EQ(result.prev(), prev);
        EXPECT_EQ(result.next(), next);
        EXPECT_EQ(result.size(), size);
      }
    }
  }
}

}  // namespace
