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

#include "pw_hdlc_lite/rpc_channel.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_stream/memory_stream.h"

using std::byte;

namespace pw::hdlc_lite {
namespace {

constexpr byte kFlag = byte{0x7E};
constexpr uint8_t kAddress = 0x7b;  // 123
constexpr byte kControl = byte{0};

// Size of the in-memory buffer to use for this test.
constexpr size_t kSinkBufferSize = 15;

TEST(RpcChannelOutput, 1BytePayload) {
  std::array<byte, kSinkBufferSize> channel_output_buffer;
  stream::MemoryWriterBuffer<kSinkBufferSize> memory_writer;

  RpcChannelOutput output(
      memory_writer, channel_output_buffer, kAddress, "RpcChannelOutput");

  constexpr byte test_data = byte{'A'};
  auto buffer = output.AcquireBuffer();
  std::memcpy(buffer.data(), &test_data, sizeof(test_data));

  constexpr auto expected = bytes::Concat(
      kFlag, kAddress, kControl, 'A', uint32_t{0xA63E2FA5}, kFlag);

  EXPECT_EQ(Status::Ok(),
            output.SendAndReleaseBuffer(buffer.first(sizeof(test_data))));

  ASSERT_EQ(memory_writer.bytes_written(), expected.size());
  EXPECT_EQ(
      std::memcmp(
          memory_writer.data(), expected.data(), memory_writer.bytes_written()),
      0);
}

TEST(RpcChannelOutput, EscapingPayloadTest) {
  std::array<byte, kSinkBufferSize> channel_output_buffer;
  stream::MemoryWriterBuffer<kSinkBufferSize> memory_writer;

  RpcChannelOutput output(
      memory_writer, channel_output_buffer, kAddress, "RpcChannelOutput");

  constexpr auto test_data = bytes::Array<0x7D>();
  auto buffer = output.AcquireBuffer();
  std::memcpy(buffer.data(), test_data.data(), test_data.size());

  constexpr auto expected = bytes::Concat(kFlag,
                                          kAddress,
                                          kControl,
                                          byte{0x7d},
                                          byte{0x7d} ^ byte{0x20},
                                          uint32_t{0x89515322},
                                          kFlag);
  EXPECT_EQ(Status::Ok(),
            output.SendAndReleaseBuffer(buffer.first(test_data.size())));

  ASSERT_EQ(memory_writer.bytes_written(), 10u);
  EXPECT_EQ(
      std::memcmp(
          memory_writer.data(), expected.data(), memory_writer.bytes_written()),
      0);
}

TEST(RpcChannelOutputBuffer, 1BytePayload) {
  stream::MemoryWriterBuffer<kSinkBufferSize> memory_writer;

  RpcChannelOutputBuffer<kSinkBufferSize> output(
      memory_writer, kAddress, "RpcChannelOutput");

  constexpr byte test_data = byte{'A'};
  auto buffer = output.AcquireBuffer();
  std::memcpy(buffer.data(), &test_data, sizeof(test_data));

  constexpr auto expected = bytes::Concat(
      kFlag, kAddress, kControl, 'A', uint32_t{0xA63E2FA5}, kFlag);

  EXPECT_EQ(Status::Ok(),
            output.SendAndReleaseBuffer(buffer.first(sizeof(test_data))));

  ASSERT_EQ(memory_writer.bytes_written(), expected.size());
  EXPECT_EQ(
      std::memcmp(
          memory_writer.data(), expected.data(), memory_writer.bytes_written()),
      0);
}

}  // namespace
}  // namespace pw::hdlc_lite
