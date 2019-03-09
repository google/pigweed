// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

#include "gtest/gtest.h"

namespace btlib {
namespace common {
namespace {

TEST(SlabAllocatorTest, NewSlabBuffer) {
  auto buffer = NewSlabBuffer(kSmallBufferSize);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kSmallBufferSize, buffer->size());

  buffer = NewSlabBuffer(kSmallBufferSize / 2);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kSmallBufferSize / 2, buffer->size());

  buffer = NewSlabBuffer(kLargeBufferSize);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kLargeBufferSize, buffer->size());

  buffer = NewSlabBuffer(kLargeBufferSize / 2);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kLargeBufferSize / 2, buffer->size());
}

}  // namespace
}  // namespace common
}  // namespace btlib
