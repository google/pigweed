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
#include "pw_channel/channel.h"

#include <optional>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_compilation_testing/negative_compilation.h"

using pw::channel::kReadable;
using pw::channel::kReliable;
using pw::channel::kSeekable;
using pw::channel::kWritable;

static_assert(static_cast<uint8_t>(pw::channel::DataType::kDatagram) >
              (kReliable | kReadable | kWritable | kSeekable));
static_assert(sizeof(::pw::channel::Channel) == 2 * sizeof(void*));

static_assert((kReliable < kReadable) && (kReadable < kWritable) &&
              (kWritable < kSeekable));

class Stub : public pw::channel::ByteChannel<kReliable, kReadable, kWritable> {
 public:
  // Read functions

  // The max_bytes argument is ignored for datagram-oriented channels.
  pw::async2::Poll<pw::Result<pw::multibuf::MultiBuf>> DoPollRead(
      pw::async2::Context&, size_t) override {
    return pw::async2::Pending();
  }

  // Write functions
  pw::async2::Poll<> DoPollReadyToWrite(pw::async2::Context&) override {
    return pw::async2::Pending();
  }

  pw::Result<pw::channel::WriteToken> DoWrite(
      pw::multibuf::MultiBuf&&) override {
    return pw::Status::Unimplemented();
  }

  pw::async2::Poll<pw::Result<pw::channel::WriteToken>> DoPollFlush(
      pw::async2::Context&) override {
    return pw::async2::Ready(
        pw::Result<pw::channel::WriteToken>(pw::Status::Unimplemented()));
  }

  // Common functions
  pw::async2::Poll<pw::Status> DoPollClose(pw::async2::Context&) override {
    return pw::OkStatus();
  }
};

TEST(Channel, MethodsShortCircuitAfterCloseReturnsReady) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    Stub channel;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      EXPECT_TRUE(channel.is_open());
      EXPECT_EQ(pw::async2::Ready(pw::OkStatus()), channel.PollClose(cx));
      EXPECT_FALSE(channel.is_open());

      EXPECT_EQ(pw::Status::FailedPrecondition(),
                channel.PollRead(cx)->status());
      EXPECT_EQ(pw::async2::Ready(), channel.PollReadyToWrite(cx));
      EXPECT_EQ(pw::Status::FailedPrecondition(),
                channel.Write(pw::multibuf::MultiBuf()).status());
      EXPECT_EQ(pw::Status::FailedPrecondition(),
                channel.PollFlush(cx)->status());
      EXPECT_EQ(pw::async2::Ready(pw::Status::FailedPrecondition()),
                channel.PollClose(cx));

      return pw::async2::Ready();
    }
  } test_task;
  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

#if PW_NC_TEST(InvalidOrdering)
PW_NC_EXPECT("Properties must be specified in the following order");
bool Illegal(pw::channel::ByteChannel<kReadable, pw::channel::kReliable>& foo) {
  return foo.is_open();
}
#elif PW_NC_TEST(NoProperties)
PW_NC_EXPECT("At least one of kReadable or kWritable must be provided");
bool Illegal(pw::channel::ByteChannel<>& foo) { return foo.is_open(); }
#elif PW_NC_TEST(NoReadOrWrite)
PW_NC_EXPECT("At least one of kReadable or kWritable must be provided");
bool Illegal(pw::channel::ByteChannel<pw::channel::kReliable>& foo) {
  return foo.is_open();
}
#elif PW_NC_TEST(TooMany)
PW_NC_EXPECT("Too many properties given");
bool Illegal(
    pw::channel::
        ByteChannel<kReliable, kReliable, kReliable, kReadable, kWritable>&
            foo) {
  return foo.is_open();
}
#elif PW_NC_TEST(Duplicates)
PW_NC_EXPECT("duplicates");
bool Illegal(pw::channel::ByteChannel<kReadable, kReadable>& foo) {
  return foo.is_open();
}
#endif  // PW_NC_TEST

#if PW_NC_TEST(PollReadIsHiddenForDatagramChannels)
PW_NC_EXPECT("not supported for DatagramChannels");
void Illegal(pw::channel::DatagramChannel<kReadable>& foo,
             pw::async2::Context& cx) {
  [[maybe_unused]] auto unused = foo.PollRead(cx, 123u);
}
#endif  // PW_NC_TEST

class FixedBufferChunkRegionTracker : public pw::multibuf::ChunkRegionTracker {
 public:
  FixedBufferChunkRegionTracker(pw::ByteSpan region) : region_(region) {}

  ~FixedBufferChunkRegionTracker() override = default;

 private:
  /// Destroys the ``ChunkRegionTracker``.
  ///
  /// Typical implementations will call ``std::destroy_at(this)`` and then free
  /// the memory associated with the region and the tracker.
  void Destroy() override {}

  /// Returns the entire span of the region being managed.
  ///
  /// ``Chunk`` s referencing this tracker will not expand beyond this region,
  /// nor into one another's portions of the region.
  ///
  /// This region must not change for the lifetime of this
  /// ``ChunkRegionTracker``.
  pw::ByteSpan Region() const override { return region_; }

  /// Returns a pointer to ``sizeof(Chunk)`` bytes.
  /// Returns ``nullptr`` on failure.
  void* AllocateChunkClass() override {
    if (chunk_allocated_) {
      return nullptr;
    }
    chunk_allocated_ = true;
    return chunk_;
  }

  /// Deallocates a pointer returned by ``AllocateChunkClass``.
  void DeallocateChunkClass(void* chunk_ptr) override {
    PW_DCHECK(chunk_allocated_);
    if (chunk_ptr == chunk_) {
      chunk_allocated_ = true;
    }
  }

  alignas(pw::multibuf::Chunk) std::byte chunk_[sizeof(pw::multibuf::Chunk)];
  pw::ByteSpan region_;
  bool chunk_allocated_ = false;
};

class TestByteReader : public pw::channel::ByteChannel<kReliable, kReadable> {
 public:
  TestByteReader() : region_(read_data_) {}

  void set_read_data(pw::ConstByteSpan data) {
    PW_CHECK_UINT_LT(read_size_ + data.size(), sizeof(read_data_));
    std::memcpy(&read_data_[read_size_], data.data(), data.size());
    read_size_ = data.size();

    std::move(*read_waker_).Wake();
    read_waker_.reset();
  }

 private:
  pw::async2::Poll<pw::Result<pw::multibuf::MultiBuf>> DoPollRead(
      pw::async2::Context& cx, size_t) override {
    if (read_size_ == 0) {
      read_waker_ = cx.GetWaker(pw::async2::WaitReason::Unspecified());
      return pw::async2::Pending();
    }

    // This seems like a lot of steps to get a multibuf of a span.
    auto chunk = region_.CreateFirstChunk();
    (*chunk)->Truncate(read_size_);
    pw::multibuf::MultiBuf mb;
    mb.PushFrontChunk(std::move(*chunk));
    return pw::Result<pw::multibuf::MultiBuf>(std::move(mb));
  }

  pw::async2::Poll<pw::Status> DoPollClose(pw::async2::Context&) override {
    return pw::async2::Ready(pw::OkStatus());
  }

  std::optional<pw::async2::Waker> read_waker_;
  int read_size_ = 0;
  std::byte read_data_[128];

  FixedBufferChunkRegionTracker region_;
};

class TestDatagramWriter : public pw::channel::DatagramWriter {
 public:
  TestDatagramWriter() : region_(buffer_) {}

  const pw::multibuf::MultiBuf& last_datagram() const {
    PW_CHECK_INT_NE(state_, kWritePending);
    return last_dgram_;
  }

  pw::multibuf::MultiBuf GetMultiBuf() {
    auto chunk = region_.CreateFirstChunk();
    pw::multibuf::MultiBuf mb;
    mb.PushFrontChunk(std::move(*chunk));
    return mb;
  }

  void MakeReadyToWrite() {
    PW_CHECK_INT_EQ(
        state_,
        kUnavailable,
        "Can't make writable when write is pending or already writable");

    if (write_waker_.has_value()) {
      std::move(write_waker_.value()).Wake();
      write_waker_.reset();
    }
    state_ = kReadyToWrite;
  }

  void MakeReadyToFlush() {
    PW_CHECK_INT_EQ(state_,
                    kWritePending,
                    "Can't make flushable unless a write is pending");

    if (flush_waker_.has_value()) {
      std::move(*flush_waker_).Wake();
      flush_waker_.reset();
    }
    state_ = kReadyToFlush;
  }

 private:
  pw::async2::Poll<> DoPollReadyToWrite(pw::async2::Context& cx) override {
    if (state_ == kReadyToWrite) {
      return pw::async2::Ready();
    }

    write_waker_ = cx.GetWaker(pw::async2::WaitReason::Unspecified());
    return pw::async2::Pending();
  }

  pw::Result<pw::channel::WriteToken> DoWrite(
      pw::multibuf::MultiBuf&& buffer) override {
    if (state_ != kReadyToWrite) {
      return pw::Status::Unavailable();
    }

    state_ = kWritePending;
    last_dgram_ = std::move(buffer);
    return CreateWriteToken(++last_write_);
  }

  pw::async2::Poll<pw::Result<pw::channel::WriteToken>> DoPollFlush(
      pw::async2::Context& cx) override {
    if (state_ != kReadyToFlush) {
      flush_waker_ = cx.GetWaker(pw::async2::WaitReason::Unspecified());
      return pw::async2::Pending();
    }
    last_flush_ = last_write_;
    return pw::async2::Ready(
        pw::Result<pw::channel::WriteToken>(CreateWriteToken(last_flush_)));
  }

  pw::async2::Poll<pw::Status> DoPollClose(pw::async2::Context&) override {
    return pw::async2::Ready(pw::OkStatus());
  }

  enum {
    kUnavailable,
    kReadyToWrite,
    kWritePending,
    kReadyToFlush,
  } state_ = kUnavailable;
  std::optional<pw::async2::Waker> write_waker_;
  std::optional<pw::async2::Waker> flush_waker_;

  uint32_t last_write_ = 0;
  uint32_t last_flush_ = 0;

  std::byte buffer_[128];
  FixedBufferChunkRegionTracker region_;
  pw::multibuf::MultiBuf last_dgram_;
};

TEST(Channel, TestByteReader) {
  static constexpr char kReadData[] = "hello, world";

  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    TestByteReader channel;

    int test_executed = 0;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      auto result = channel.PollRead(cx);
      if (!result.IsReady()) {
        return pw::async2::Pending();
      }

      auto actual_result = std::move(*result);
      EXPECT_TRUE(actual_result.ok());

      std::byte contents[64] = {};

      EXPECT_EQ(actual_result->size(), sizeof(kReadData));
      std::copy(actual_result->begin(), actual_result->end(), contents);
      EXPECT_STREQ(reinterpret_cast<const char*>(contents), kReadData);

      test_executed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  test_task.channel.set_read_data(pw::as_bytes(pw::span(kReadData)));
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());

  EXPECT_EQ(test_task.test_executed, 1);
}

TEST(Channel, TestDatagramWriter) {
  pw::async2::Dispatcher dispatcher;

  static constexpr char kWriteData[] = "Hello there";

  class : public pw::async2::Task {
   public:
    TestDatagramWriter channel;

    pw::channel::WriteToken write_token;
    int test_executed = 0;

   private:
    enum { kWaitUntilReady, kFlushPacket } state_ = kWaitUntilReady;

    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      switch (state_) {
        case kWaitUntilReady: {
          if (!channel.PollReadyToWrite(cx).IsReady()) {
            return pw::async2::Pending();
          }

          auto mb = channel.GetMultiBuf();
          pw::ConstByteSpan str(pw::as_bytes(pw::span(kWriteData)));
          std::copy(str.begin(), str.end(), mb.begin());
          mb.ChunkBegin()->Truncate(sizeof(kWriteData));

          auto token = channel.Write(std::move(mb));
          PW_CHECK(token.ok());
          write_token = *token;
          state_ = kFlushPacket;
          [[fallthrough]];
        }
        case kFlushPacket: {
          auto result = channel.PollFlush(cx);
          if (!result.IsReady() || **result < write_token) {
            return pw::async2::Pending();
          }
          test_executed += 1;
          state_ = kWaitUntilReady;
          return pw::async2::Ready();
        }
        default:
          PW_CRASH("Illegal value");
      }

      // This test is INCOMPLETE.

      test_executed += 1;
      return pw::async2::Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());

  test_task.channel.MakeReadyToWrite();

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());

  test_task.channel.MakeReadyToFlush();

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
  EXPECT_EQ(test_task.test_executed, 1);

  std::byte contents[64] = {};
  const auto& dgram = test_task.channel.last_datagram();
  std::copy(dgram.begin(), dgram.end(), contents);
  EXPECT_STREQ(reinterpret_cast<const char*>(contents), kWriteData);
}

void TakesAChannel(const pw::channel::Channel&) {}

void TakesAReadableChannel(
    const pw::channel::ByteChannel<kReliable, kReadable>&) {}

void TakesAWritableChannel(pw::channel::ByteChannel<kReliable, kWritable>&) {}

TEST(Channel, Conversions) {
  const TestByteReader channel;

  TakesAReadableChannel(channel);
  TakesAChannel(channel);

  [[maybe_unused]] const pw::channel::Channel& plain =
      channel.as<pw::channel::Channel>();

#if PW_NC_TEST(CannotLoseWritability)
  PW_NC_EXPECT("Cannot use a non-writable channel as a writable channel");
  TakesAWritableChannel(channel);
#endif  // PW_NC_TEST
}

#if PW_NC_TEST(CannotUseDatagramChannelAsByteChannel)
PW_NC_EXPECT("Datagram and byte channels are not interchangeable");

void DatagramChannelNcTest(
    pw::channel::DatagramChannel<kReliable, kReadable>& dgram) {
  TakesAReadableChannel(dgram);
}

#endif  // PW_NC_TEST
