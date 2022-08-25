// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

#include <gtest/gtest.h>

namespace bt {
namespace {

TEST(SlabAllocatorTest, NewBuffer) {
  auto buffer = NewBuffer(kSmallBufferSize);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kSmallBufferSize, buffer->size());

  buffer = NewBuffer(kSmallBufferSize / 2);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kSmallBufferSize / 2, buffer->size());

  buffer = NewBuffer(kLargeBufferSize);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kLargeBufferSize, buffer->size());

  buffer = NewBuffer(kLargeBufferSize / 2);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kLargeBufferSize / 2, buffer->size());

  buffer = NewBuffer(kLargeBufferSize + 1);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(kLargeBufferSize + 1, buffer->size());

  buffer = NewBuffer(0);
  EXPECT_TRUE(buffer);
  EXPECT_EQ(0U, buffer->size());
}

}  // namespace
}  // namespace bt
