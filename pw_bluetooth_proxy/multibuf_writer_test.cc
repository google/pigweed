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

#include "pw_bluetooth_proxy/internal/multibuf_writer.h"

#include <array>

#include "pw_containers/to_array.h"
#include "pw_multibuf/simple_allocator_for_test.h"
#include "pw_unit_test/framework.h"  // IWYU pragma: keep

namespace pw::bluetooth::proxy {
namespace {

using pw::containers::to_array;

class MultiBufWriterTest : public testing::Test {
 protected:
  pw::multibuf::test::SimpleAllocatorForTest</*kDataSizeBytes=*/512,
                                             /*kMetaSizeBytes=*/512>
      allocator;
};

TEST_F(MultiBufWriterTest, CanCreate) {
  pw::Result<MultiBufWriter> writer = MultiBufWriter::Create(allocator, 8u);
  PW_TEST_ASSERT_OK(writer);
  EXPECT_FALSE(writer->IsComplete());
  EXPECT_EQ(writer->U8Span().size(), 0u);
}

TEST_F(MultiBufWriterTest, CanWrite) {
  pw::Result<MultiBufWriter> writer = MultiBufWriter::Create(allocator, 8u);
  PW_TEST_ASSERT_OK(writer);

  static constexpr auto kExpectedData =
      to_array<uint8_t>({0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88});

  // Write first chunk
  PW_TEST_EXPECT_OK(writer->Write(to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));
  EXPECT_FALSE(writer->IsComplete());
  EXPECT_EQ(writer->U8Span().size(), 4u);

  // Write second chunk
  PW_TEST_EXPECT_OK(writer->Write(to_array<uint8_t>({0x55, 0x66, 0x77, 0x88})));
  EXPECT_TRUE(writer->IsComplete());
  EXPECT_EQ(writer->U8Span().size(), 8u);
  EXPECT_TRUE(std::equal(writer->U8Span().begin(),
                         writer->U8Span().end(),
                         kExpectedData.begin(),
                         kExpectedData.end()));
}

TEST_F(MultiBufWriterTest, CannotWriteMoreThanRemains) {
  pw::Result<MultiBufWriter> writer = MultiBufWriter::Create(allocator, 5u);
  PW_TEST_ASSERT_OK(writer);

  // Write first chunk
  PW_TEST_EXPECT_OK(writer->Write(to_array<uint8_t>({0x11, 0x22, 0x33, 0x44})));
  EXPECT_FALSE(writer->IsComplete());
  EXPECT_EQ(writer->U8Span().size(), 4u);

  // Cannot write second chunk (one byte too big)
  EXPECT_EQ(writer->Write(to_array<uint8_t>({0x55, 0x66})),
            Status::ResourceExhausted());
  EXPECT_FALSE(writer->IsComplete());
  EXPECT_EQ(writer->U8Span().size(), 4u);
}

TEST_F(MultiBufWriterTest, CanTakeMultiBuf) {
  pw::Result<MultiBufWriter> writer = MultiBufWriter::Create(allocator, 8u);
  PW_TEST_ASSERT_OK(writer);

  static constexpr auto kData =
      to_array<uint8_t>({0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88});

  PW_TEST_EXPECT_OK(writer->Write(kData));
  ASSERT_TRUE(writer->IsComplete());

  // Take the MultiBuf from the MultiBufWriter
  multibuf::MultiBuf mbuf = std::move(writer->TakeMultiBuf());
  EXPECT_EQ(mbuf.size(), 8u);

  std::optional<ByteSpan> mbuf_span = mbuf.ContiguousSpan();
  ASSERT_TRUE(bool(mbuf_span));
  pw::span<uint8_t> mbuf_u8_span(reinterpret_cast<uint8_t*>(mbuf_span->data()),
                                 mbuf_span->size());
  EXPECT_TRUE(std::equal(
      mbuf_u8_span.begin(), mbuf_u8_span.end(), kData.begin(), kData.end()));

  // IsComplete() returns true
  EXPECT_TRUE(writer->IsComplete());

  // Can no longer write
  EXPECT_NE(writer->Write(kData), OkStatus());

  // Calling it again results in an empty multibuf.
  multibuf::MultiBuf mbuf2 = std::move(writer->TakeMultiBuf());
  EXPECT_EQ(mbuf2.size(), 0u);
}

}  // namespace
}  // namespace pw::bluetooth::proxy
