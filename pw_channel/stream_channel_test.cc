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

#include "pw_channel/stream_channel.h"

#include <algorithm>
#include <array>

#include "pw_async2/pend_func_task.h"
#include "pw_bytes/suffix.h"
#include "pw_multibuf/simple_allocator_for_test.h"
#include "pw_status/status.h"
#include "pw_stream/mpsc_stream.h"
#include "pw_thread/test_thread_context.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::Result;
using ::pw::async2::Context;
using ::pw::async2::PendFuncTask;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;
using ::pw::async2::Waker;
using ::pw::multibuf::MultiBuf;
using ::pw::multibuf::test::SimpleAllocatorForTest;
using ::pw::operator""_b;

template <typename ActualIterable, typename ExpectedIterable>
void ExpectElementsEqual(const ActualIterable& actual,
                         const ExpectedIterable& expected) {
  auto actual_iter = actual.begin();
  auto expected_iter = expected.begin();
  for (; expected_iter != expected.end(); ++actual_iter, ++expected_iter) {
    ASSERT_NE(actual_iter, actual.end());
    EXPECT_EQ(*actual_iter, *expected_iter);
  }
}

template <typename ActualIterable, typename T>
void ExpectElementsEqual(const ActualIterable& actual,
                         std::initializer_list<T> expected) {
  ExpectElementsEqual<ActualIterable, std::initializer_list<T>>(actual,
                                                                expected);
}

TEST(StreamChannel, ReadsAndWritesData) {
  static pw::stream::BufferedMpscReader<512> channel_input_reader;
  static pw::stream::MpscWriter channel_input_writer;
  pw::stream::CreateMpscStream(channel_input_reader, channel_input_writer);

  static pw::stream::BufferedMpscReader<512> channel_output_reader;
  static pw::stream::MpscWriter channel_output_writer;
  pw::stream::CreateMpscStream(channel_output_reader, channel_output_writer);

  static pw::NoDestructor<SimpleAllocatorForTest<>> allocator;
  static pw::thread::test::TestThreadContext read_thread_cx;
  static pw::thread::test::TestThreadContext write_thread_cx;
  static pw::NoDestructor<pw::channel::StreamChannel> stream_channel(
      *allocator,
      channel_input_reader,
      read_thread_cx.options(),
      channel_output_writer,
      write_thread_cx.options());

  PendFuncTask read_task([&](Context& cx) -> Poll<> {
    auto read = stream_channel->PendRead(cx);
    if (read.IsPending()) {
      return Pending();
    }
    EXPECT_EQ(read->status(), pw::OkStatus());
    if (read->ok()) {
      ExpectElementsEqual(**read, {1_b, 2_b, 3_b});
    }
    return Ready();
  });

  MultiBuf to_send = allocator->BufWith({4_b, 5_b, 6_b});
  PendFuncTask write_task([&](Context& cx) -> Poll<> {
    if (stream_channel->PendReadyToWrite(cx).IsPending()) {
      return Pending();
    }
    EXPECT_EQ(stream_channel->Write(std::move(to_send)).status(),
              pw::OkStatus());
    return Ready();
  });

  pw::async2::Dispatcher dispatcher;
  dispatcher.Post(write_task);
  dispatcher.Post(read_task);

  EXPECT_EQ(Pending(), dispatcher.RunUntilStalled());
  std::array<const std::byte, 3> data_to_send({1_b, 2_b, 3_b});
  ASSERT_EQ(pw::OkStatus(), channel_input_writer.Write(data_to_send));
  dispatcher.RunToCompletion();
}

}  // namespace
