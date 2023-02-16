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

#include "pw_stream/null_stream.h"

#include <cstddef>

#include "gtest/gtest.h"

namespace pw::stream {
namespace {

TEST(NullStream, DefaultConservativeWriteLimit) {
  NullStream stream;
  EXPECT_EQ(stream.ConservativeWriteLimit(), Stream::kUnlimited);
}

TEST(NullStream, DefaultConservativeReadLimit) {
  NullStream stream;
  EXPECT_EQ(stream.ConservativeReadLimit(), Stream::kUnlimited);
}

TEST(NullStream, DefaultConservativeReadWriteLimit) {
  NullStream stream;
  EXPECT_EQ(stream.ConservativeWriteLimit(), Stream::kUnlimited);
  EXPECT_EQ(stream.ConservativeReadLimit(), Stream::kUnlimited);
}

TEST(NullStream, DefaultTell) {
  NullStream stream;
  EXPECT_EQ(stream.Tell(), Stream::kUnknownPosition);
}

TEST(CountingNullStream, DefaultConservativeWriteLimit) {
  CountingNullStream stream;
  EXPECT_EQ(stream.ConservativeWriteLimit(), Stream::kUnlimited);
}

TEST(CountingNullStream, DefaultConservativeReadLimit) {
  CountingNullStream stream;
  EXPECT_EQ(stream.ConservativeReadLimit(), Stream::kUnlimited);
}

TEST(CountingNullStream, DefaultConservativeReadWriteLimit) {
  CountingNullStream stream;
  EXPECT_EQ(stream.ConservativeWriteLimit(), Stream::kUnlimited);
  EXPECT_EQ(stream.ConservativeReadLimit(), Stream::kUnlimited);
}

TEST(CountingNullStream, DefaultTell) {
  CountingNullStream stream;
  EXPECT_EQ(stream.Tell(), Stream::kUnknownPosition);
}

std::byte data[32];

TEST(CountingNullStream, CountsWrites) {
  CountingNullStream stream;
  EXPECT_EQ(OkStatus(), stream.Write(data));
  EXPECT_EQ(sizeof(data), stream.bytes_written());

  EXPECT_EQ(OkStatus(), stream.Write(span(data).first(1)));
  EXPECT_EQ(sizeof(data) + 1, stream.bytes_written());

  EXPECT_EQ(OkStatus(), stream.Write(span<std::byte>()));
  EXPECT_EQ(sizeof(data) + 1, stream.bytes_written());

  EXPECT_EQ(OkStatus(), stream.Write(data));
  EXPECT_EQ(2 * sizeof(data) + 1, stream.bytes_written());
}

}  // namespace
}  // namespace pw::stream
