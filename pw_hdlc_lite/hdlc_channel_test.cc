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

#include "pw_hdlc_lite/hdlc_channel.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_stream/memory_stream.h"

using std::byte;

namespace pw::rpc {
namespace {
// Size of the in-memory buffer to use for this test.
constexpr size_t kSinkBufferSize = 15;

TEST(HdlcChannelOutput, 1BytePayload) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  std::array<byte, kSinkBufferSize> channel_output_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  HdlcChannelOutput channel_output(
      memory_writer, channel_output_buffer, "HdlcChannelOutput");

  constexpr std::array<byte, 1> test_array = bytes::Array<0x41>();
  memcpy(channel_output.AcquireBuffer().data(),
         test_array.data(),
         test_array.size());

  constexpr std::array<byte, 5> expected_array =
      bytes::Array<0x7E, 0x41, 0x15, 0xB9, 0x7E>();
  channel_output.SendAndReleaseBuffer(test_array.size());

  EXPECT_STREQ("HdlcChannelOutput", channel_output.name());
  EXPECT_EQ(memory_writer.bytes_written(), 5u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

TEST(HdlcChannelOutput, EscapingPayloadTest) {
  std::array<byte, kSinkBufferSize> memory_buffer;
  std::array<byte, kSinkBufferSize> channel_output_buffer;
  stream::MemoryWriter memory_writer(memory_buffer);

  HdlcChannelOutput channel_output(
      memory_writer, channel_output_buffer, "HdlcChannelOutput");

  constexpr std::array<byte, 1> test_array = bytes::Array<0x7D>();
  memcpy(channel_output.AcquireBuffer().data(),
         test_array.data(),
         test_array.size());

  constexpr std::array<byte, 6> expected_array =
      bytes::Array<0x7E, 0x7D, 0x5D, 0xCA, 0x4E, 0x7E>();
  channel_output.SendAndReleaseBuffer(test_array.size());

  EXPECT_STREQ("HdlcChannelOutput", channel_output.name());
  EXPECT_EQ(memory_writer.bytes_written(), 6u);
  EXPECT_EQ(std::memcmp(memory_writer.data(),
                        expected_array.data(),
                        memory_writer.bytes_written()),
            0);
}

}  // namespace
}  // namespace pw::rpc
