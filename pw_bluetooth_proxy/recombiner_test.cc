// Copyright 2025 The Pigweed Authors
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

#include "pw_bluetooth_proxy/internal/recombiner.h"

#include <cstdint>

#include "pw_containers/to_array.h"
#include "pw_multibuf/simple_allocator_for_test.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::bluetooth::proxy {
namespace {

using pw::containers::to_array;

class RecombinerTest : public testing::Test {
 protected:
  pw::multibuf::test::SimpleAllocatorForTest</*kDataSizeBytes=*/512,
                                             /*kMetaSizeBytes=*/512>
      allocator_;
};

TEST_F(RecombinerTest, InactiveAtCreation) {
  Recombiner recombiner{};

  EXPECT_FALSE(recombiner.IsActive());
}

TEST_F(RecombinerTest, Start) {
  Recombiner recombiner{};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(0x20, allocator_, 8u));

  EXPECT_TRUE(recombiner.IsActive());
  EXPECT_FALSE(recombiner.IsComplete());
}

TEST_F(RecombinerTest, GetLocalCid) {
  constexpr uint16_t kLocalCid = 0x20;
  Recombiner recombiner{};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(kLocalCid, allocator_, 8u));

  EXPECT_EQ(recombiner.local_cid(), kLocalCid);
}

TEST_F(RecombinerTest, End) {
  Recombiner recombiner{};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(0x20, allocator_, 8u));

  EXPECT_TRUE(recombiner.IsActive());

  recombiner.EndRecombination();

  EXPECT_FALSE(recombiner.IsActive());
}

TEST_F(RecombinerTest, WriteTakeEnd) {
  Recombiner recombiner{};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(0x20, allocator_, 8u));

  static constexpr std::array<uint8_t, 8> kExpectedData = {
      0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

  // Write first chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));

  EXPECT_FALSE(recombiner.IsComplete());

  // Write second chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      to_array<uint8_t>({0x55, 0x66, 0x77, 0x88})));

  // Test combined result
  EXPECT_TRUE(recombiner.IsComplete());

  multibuf::MultiBuf mbuf = recombiner.TakeAndEnd();
  EXPECT_TRUE(mbuf.IsContiguous());

  EXPECT_FALSE(recombiner.IsActive());

  std::optional<ByteSpan> mbuf_span = mbuf.ContiguousSpan();
  EXPECT_TRUE(mbuf_span);

  pw::span<uint8_t> span = pw::span(
      reinterpret_cast<uint8_t*>(mbuf_span->data()), mbuf_span->size());

  EXPECT_TRUE(std::equal(
      span.begin(), span.end(), kExpectedData.begin(), kExpectedData.end()));
}

TEST_F(RecombinerTest, CannotOverwrite) {
  Recombiner recombiner{};

  PW_TEST_EXPECT_OK(recombiner.StartRecombination(0x20, allocator_, 8u));

  // Write first chunk
  PW_TEST_EXPECT_OK(recombiner.RecombineFragment(
      to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));

  EXPECT_FALSE(recombiner.IsComplete());

  // Try to write too large a second chunk.
  EXPECT_EQ(recombiner.RecombineFragment(
                to_array<uint8_t>({0x55, 0x66, 0x77, 0x88, 0x99})),
            pw::Status::ResourceExhausted());

  // Should still not be complete.
  EXPECT_FALSE(recombiner.IsComplete());

  // Typically client ends recombination at this point.
  recombiner.EndRecombination();
  EXPECT_FALSE(recombiner.IsActive());
}

}  // namespace
}  // namespace pw::bluetooth::proxy
