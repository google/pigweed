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

#include "pw_stream/limited_stream.h"

#include <cstddef>

#include "pw_bytes/array.h"
#include "pw_stream/memory_stream.h"
#include "pw_stream/null_stream.h"
#include "pw_unit_test/framework.h"

#define EXPECT_SEQ_EQ(seq1, seq2)                                        \
  do {                                                                   \
    EXPECT_EQ(seq1.size(), seq2.size());                                 \
    EXPECT_TRUE(                                                         \
        std::equal(seq1.begin(), seq1.end(), seq2.begin(), seq2.end())); \
  } while (0)

namespace {

using pw::Status;
using pw::bytes::Array;
using pw::stream::LimitedStreamWriter;
using pw::stream::MemoryWriterBuffer;
using pw::stream::NullStream;
using pw::stream::Stream;

TEST(LimitedStreamWriter, DefaultConservativeWriteLimit) {
  NullStream stream;
  ASSERT_EQ(stream.ConservativeWriteLimit(), Stream::kUnlimited);

  LimitedStreamWriter writer(stream);  // unlimited
  EXPECT_EQ(writer.ConservativeWriteLimit(), Stream::kUnlimited);
}

TEST(LimitedStreamWriter, LimitedConservativeWriteLimit) {
  NullStream stream;
  ASSERT_EQ(stream.ConservativeWriteLimit(), Stream::kUnlimited);

  constexpr size_t kLimit = 123;
  LimitedStreamWriter writer(stream, kLimit);
  EXPECT_EQ(writer.ConservativeWriteLimit(), kLimit);
}

TEST(LimitedStreamWriter, LimitedConservativeWriteLimitOverUnderlying) {
  MemoryWriterBuffer<16> buffer;
  ASSERT_EQ(buffer.ConservativeWriteLimit(), buffer.capacity());

  constexpr size_t kLimit = 123;
  LimitedStreamWriter writer(buffer, kLimit);

  // The write limit is the smaller of the writer limit and underlying limit.
  EXPECT_EQ(writer.ConservativeWriteLimit(), buffer.capacity());
}

TEST(LimitedStreamWriter, WritesWhenUnlimited) {
  MemoryWriterBuffer<16> buffer;
  LimitedStreamWriter writer(buffer);
  EXPECT_EQ(writer.limit(), Stream::kUnlimited);

  constexpr auto kData = Array<1, 2, 3, 4>();
  PW_TEST_EXPECT_OK(writer.Write(kData));
  EXPECT_SEQ_EQ(buffer, kData);

  EXPECT_EQ(writer.bytes_written(), kData.size());
  // It respects the underlying stream limit.
  EXPECT_EQ(writer.ConservativeWriteLimit(), buffer.capacity() - kData.size());
}

TEST(LimitedStreamWriter, WritesWhenLimited) {
  MemoryWriterBuffer<16> buffer;
  constexpr size_t kLimit = 8;
  LimitedStreamWriter writer(buffer, kLimit);
  EXPECT_EQ(writer.limit(), kLimit);

  constexpr auto kData = Array<1, 2, 3, 4>();
  PW_TEST_EXPECT_OK(writer.Write(kData));
  EXPECT_SEQ_EQ(buffer, kData);

  EXPECT_EQ(writer.bytes_written(), kData.size());
  EXPECT_EQ(writer.ConservativeWriteLimit(), kLimit - kData.size());
}

TEST(LimitedStreamWriter, CanWriteToLimitAndNotAfter) {
  MemoryWriterBuffer<16> buffer;
  constexpr size_t kLimit = 8;
  LimitedStreamWriter writer(buffer, kLimit);
  EXPECT_EQ(writer.limit(), kLimit);

  constexpr auto kData = Array<1, 2, 3, 4, 5, 6, 7, 8>();
  PW_TEST_EXPECT_OK(writer.Write(kData));
  EXPECT_SEQ_EQ(buffer, kData);

  EXPECT_EQ(writer.bytes_written(), kData.size());
  EXPECT_EQ(writer.ConservativeWriteLimit(), 0u);

  EXPECT_EQ(writer.Write(kData), Status::OutOfRange());  // Cannot write again
  EXPECT_EQ(buffer.bytes_written(), kData.size());       // Nothing more written
}

TEST(LimitedStreamWriter, CannotWritePasteLimit) {
  MemoryWriterBuffer<16> buffer;
  constexpr size_t kLimit = 4;
  LimitedStreamWriter writer(buffer, kLimit);

  constexpr auto kData = Array<1, 2, 3, 4, 99, 99, 99>();
  EXPECT_EQ(writer.Write(kData), Status::OutOfRange());

  // Nothing is written when a Write() would exceed the limit.
  EXPECT_EQ(buffer.bytes_written(), 0u);
}

TEST(LimitedStreamWriter, CannotWritePasteLimitAfterChanged) {
  MemoryWriterBuffer<16> buffer;
  LimitedStreamWriter writer(buffer, 8);

  constexpr auto kData = Array<1, 2, 3, 4>();
  PW_TEST_EXPECT_OK(writer.Write(kData));
  EXPECT_SEQ_EQ(buffer, kData);

  // Change the limit to a value less than what was written already.
  writer.set_limit(2);
  EXPECT_EQ(writer.limit(), 2u);
  EXPECT_EQ(writer.ConservativeWriteLimit(), 0u);  // Does not underflow

  EXPECT_EQ(writer.Write(kData), Status::OutOfRange());  // Cannot write again
  EXPECT_EQ(buffer.bytes_written(), kData.size());       // Nothing more written
}

}  // namespace
