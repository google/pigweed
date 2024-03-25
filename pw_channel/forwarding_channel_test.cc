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

#include "pw_channel/forwarding_channel.h"

#include <algorithm>
#include <array>

#include "pw_allocator/testing.h"
#include "pw_multibuf/header_chunk_region_tracker.h"
#include "pw_multibuf/simple_allocator.h"
#include "pw_string/string.h"
#include "pw_unit_test/framework.h"

namespace {

using ::pw::Result;
using ::pw::async2::Context;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;
using ::pw::async2::Waker;
using ::pw::channel::ByteReader;
using ::pw::channel::ByteWriter;
using ::pw::channel::DatagramReader;
using ::pw::channel::DatagramWriter;
using ::pw::multibuf::MultiBuf;

// Creates and initializes a MultiBuf to the specified value.
class InitializedMultiBuf {
 public:
  InitializedMultiBuf(std::string_view contents) {
    std::optional<pw::multibuf::OwnedChunk> chunk =
        pw::multibuf::HeaderChunkRegionTracker::AllocateRegionAsChunk(
            allocator_, contents.size());
    std::memcpy(chunk.value().data(), contents.data(), contents.size());
    buf_.PushFrontChunk(std::move(*chunk));
  }

  pw::multibuf::MultiBuf Take() { return std::move(buf_); }

 private:
  pw::allocator::test::AllocatorForTest<2048> allocator_;
  pw::multibuf::MultiBuf buf_;
};

pw::InlineString<128> CopyToString(const pw::multibuf::MultiBuf& mb) {
  pw::InlineString<128> contents(mb.size(), '\0');
  std::copy(
      mb.begin(), mb.end(), reinterpret_cast<std::byte*>(contents.data()));
  return contents;
}

template <pw::channel::DataType kType,
          size_t kDataSize = 128,
          size_t kMetaSize = 128>
class TestChannelPair {
 public:
  TestChannelPair()
      : simple_allocator_(data_area_, meta_alloc_), pair_(simple_allocator_) {}

  pw::channel::ForwardingChannelPair<kType>* operator->() { return &pair_; }

 private:
  std::array<std::byte, kDataSize> data_area_;
  pw::allocator::test::AllocatorForTest<kMetaSize> meta_alloc_;
  pw::multibuf::SimpleAllocator simple_allocator_;

  pw::channel::ForwardingChannelPair<kType> pair_;
};

// TODO: b/330788671 - Have the test tasks run in multiple stages to ensure that
//     wakers are stored / woken properly by ForwardingChannel.
TEST(ForwardingDatagramChannel, ForwardsEmptyDatagrams) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    TestChannelPair<pw::channel::DataType::kDatagram> pair;

    int test_completed = 0;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      // No data yet
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      // Send datagram first->second
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      auto result = pair->first().Write({});  // Write empty datagram
      EXPECT_EQ(pw::OkStatus(), result.status());

      EXPECT_EQ(pw::async2::Pending(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));

      auto empty_chunk_result = pair->second().PollRead(cx);
      EXPECT_TRUE(empty_chunk_result.IsReady());
      EXPECT_TRUE(empty_chunk_result->ok());
      EXPECT_EQ((*empty_chunk_result)->size(), 0u);

      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      // Send datagram second->first
      EXPECT_EQ(pw::async2::Ready(), pair->second().PollReadyToWrite(cx));
      result = pair->second().Write({});  // Write empty datagram
      EXPECT_EQ(pw::OkStatus(), result.status());

      EXPECT_EQ(pw::async2::Pending(), pair->second().PollReadyToWrite(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      empty_chunk_result = pair->first().PollRead(cx);
      EXPECT_TRUE(empty_chunk_result.IsReady());
      EXPECT_TRUE(empty_chunk_result->ok());
      EXPECT_EQ((*empty_chunk_result)->size(), 0u);

      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));

      test_completed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(test_task.test_completed, 1);
}

TEST(ForwardingDatagramChannel, ForwardsNonEmptyDatagrams) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    TestChannelPair<pw::channel::DataType::kDatagram> pair;

    int test_completed = 0;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      InitializedMultiBuf b1("Hello");
      InitializedMultiBuf b2("world!");

      // Send datagram first->second
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write(b1.Take()).status());

      EXPECT_EQ(pw::async2::Pending(), pair->first().PollReadyToWrite(cx));

      EXPECT_EQ(CopyToString(pair->second().PollRead(cx).value().value()),
                "Hello");

      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      EXPECT_EQ(pw::OkStatus(), pair->first().Write(b2.Take()).status());
      EXPECT_EQ(CopyToString(pair->second().PollRead(cx).value().value()),
                "world!");

      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));

      test_completed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(test_task.test_completed, 1);
}

TEST(ForwardingDatagramChannel, ForwardsDatagrams) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    TestChannelPair<pw::channel::DataType::kDatagram> pair;

    int test_completed = 0;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      // No data yet
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      // Send datagram first->second
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      auto result = pair->first().Write({});  // Write empty datagram
      EXPECT_EQ(pw::OkStatus(), result.status());

      EXPECT_EQ(pw::async2::Pending(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));

      auto empty_chunk_result = pair->second().PollRead(cx);
      EXPECT_TRUE(empty_chunk_result.IsReady());
      EXPECT_TRUE(empty_chunk_result->ok());
      EXPECT_EQ((*empty_chunk_result)->size(), 0u);

      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      // Send datagram second->first
      EXPECT_EQ(pw::async2::Ready(), pair->second().PollReadyToWrite(cx));
      result = pair->second().Write({});  // Write empty datagram
      EXPECT_EQ(pw::OkStatus(), result.status());

      EXPECT_EQ(pw::async2::Pending(), pair->second().PollReadyToWrite(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      empty_chunk_result = pair->first().PollRead(cx);
      EXPECT_TRUE(empty_chunk_result.IsReady());
      EXPECT_TRUE(empty_chunk_result->ok());
      EXPECT_EQ((*empty_chunk_result)->size(), 0u);

      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));

      test_completed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(test_task.test_completed, 1);
}

TEST(ForwardingDatagramchannel, PollCloseAwakensAndClosesPeer) {
  class TryToReadUntilClosed : public Task {
   public:
    TryToReadUntilClosed(DatagramReader& reader) : reader_(reader) {}

   private:
    pw::async2::Poll<> DoPend(Context& cx) final {
      Poll<Result<MultiBuf>> read = reader_.PollRead(cx);
      if (read.IsPending()) {
        return Pending();
      }
      EXPECT_EQ(read->status(), pw::Status::FailedPrecondition());
      return Ready();
    }
    DatagramReader& reader_;
  };

  pw::async2::Dispatcher dispatcher;
  TestChannelPair<pw::channel::DataType::kDatagram> pair;
  TryToReadUntilClosed read_task(pair->first());
  dispatcher.Post(read_task);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  Waker empty_waker;
  Context empty_cx(dispatcher, empty_waker);
  EXPECT_EQ(pair->second().PollClose(empty_cx), Ready(pw::OkStatus()));

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

TEST(ForwardingByteChannel, IgnoresEmptyWrites) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    TestChannelPair<pw::channel::DataType::kByte> pair;

    int test_completed = 0;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      // No data yet
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      // Send nothing first->second
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write({}).status());

      // Still no data
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      // Send nothing second->first
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write({}).status());

      // Still no data
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      test_completed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(test_task.test_completed, 1);
}

TEST(ForwardingByteChannel, WriteData) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    TestChannelPair<pw::channel::DataType::kByte> pair;

    int test_completed = 0;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      // No data yet
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      InitializedMultiBuf b1("hello");
      InitializedMultiBuf b2(" ");
      InitializedMultiBuf b3("world");

      // Send "hello world" first->second
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write(b1.Take()).status());
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write(b2.Take()).status());
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write(b3.Take()).status());

      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));

      auto hello_world_result = pair->second().PollRead(cx);
      EXPECT_TRUE(hello_world_result.IsReady());

      EXPECT_EQ(CopyToString(hello_world_result->value()), "hello world");

      // Send nothing second->first
      EXPECT_EQ(pw::async2::Ready(), pair->first().PollReadyToWrite(cx));
      EXPECT_EQ(pw::OkStatus(), pair->first().Write({}).status());

      // Still no data
      EXPECT_EQ(pw::async2::Pending(), pair->first().PollRead(cx));
      EXPECT_EQ(pw::async2::Pending(), pair->second().PollRead(cx));

      test_completed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(test_task.test_completed, 1);
}

TEST(ForwardingByteChannel, PollCloseAwakensAndClosesPeer) {
  class TryToReadUntilClosed : public Task {
   public:
    TryToReadUntilClosed(ByteReader& reader) : reader_(reader) {}

   private:
    pw::async2::Poll<> DoPend(Context& cx) final {
      Poll<Result<MultiBuf>> read = reader_.PollRead(cx);
      if (read.IsPending()) {
        return Pending();
      }
      EXPECT_EQ(read->status(), pw::Status::FailedPrecondition());
      return Ready();
    }
    ByteReader& reader_;
  };

  pw::async2::Dispatcher dispatcher;
  TestChannelPair<pw::channel::DataType::kByte> pair;
  TryToReadUntilClosed read_task(pair->first());
  dispatcher.Post(read_task);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  Waker empty_waker;
  Context empty_cx(dispatcher, empty_waker);
  EXPECT_EQ(pair->second().PollClose(empty_cx), Ready(pw::OkStatus()));

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
}

}  // namespace
