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

#include "pw_allocator/buffer.h"

#include <array>
#include <cstdint>

#include "lib/stdcompat/bit.h"
#include "pw_bytes/span.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::allocator::WithBuffer;

TEST(WithBuffer, AlignedBytesAreAvailable) {
  static constexpr size_t kBufferSize = 47;
  static constexpr size_t kAlignment = 32;
  WithBuffer<int, kBufferSize, kAlignment> int_with_buffer;
  EXPECT_EQ(int_with_buffer.size(), kBufferSize);
  EXPECT_EQ(int_with_buffer.as_bytes().size(), kBufferSize);
  auto buffer_data_ptr =
      cpp20::bit_cast<uintptr_t>(int_with_buffer.as_bytes().data());
  EXPECT_EQ(buffer_data_ptr % kAlignment, 0u);
}

}  // namespace
