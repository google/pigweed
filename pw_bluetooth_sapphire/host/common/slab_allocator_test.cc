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

#include "pw_bluetooth_sapphire/internal/host/common/slab_allocator.h"

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
