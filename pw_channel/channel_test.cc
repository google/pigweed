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
#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_compilation_testing/negative_compilation.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/simple_allocator.h"
#include "pw_preprocessor/compiler.h"

namespace {

using ::pw::allocator::test::AllocatorForTest;
using ::pw::async2::Context;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Waker;
using ::pw::channel::ByteChannel;
using ::pw::channel::DatagramWriter;
using ::pw::channel::kReadable;
using ::pw::channel::kReliable;
using ::pw::channel::kSeekable;
using ::pw::channel::kWritable;
using ::pw::multibuf::MultiBuf;
using ::pw::multibuf::MultiBufAllocationFuture;
using ::pw::multibuf::MultiBufAllocator;
using ::pw::multibuf::SimpleAllocator;

static_assert(sizeof(::pw::channel::AnyChannel) == 2 * sizeof(void*));

static_assert((kReliable < kReadable) && (kReadable < kWritable) &&
              (kWritable < kSeekable));

class ReliableByteReaderWriterStub
    : public pw::channel::ByteChannel<kReliable, kReadable, kWritable> {
 private:
  // Read functions

  // The max_bytes argument is ignored for datagram-oriented channels.
  pw::async2::Poll<pw::Result<pw::multibuf::MultiBuf>> DoPendRead(
      pw::async2::Context&) override {
    return pw::async2::Pending();
  }

  // Write functions

  // Disable maybe-uninitialized: this check fails erroniously on Windows GCC.
  PW_MODIFY_DIAGNOSTICS_PUSH();
  PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wmaybe-uninitialized");
  pw::async2::Poll<pw::Status> DoPendReadyToWrite(
      pw::async2::Context&) override {
    return pw::async2::Pending();
  }
  PW_MODIFY_DIAGNOSTICS_POP();

  pw::multibuf::MultiBufAllocator& DoGetWriteAllocator() override {
    // ``DoPendReadyToWrite`` will never return ``Ready``, so this is not
    // callable.
    PW_CHECK(false);
  }

  pw::Result<pw::channel::WriteToken> DoWrite(
      pw::multibuf::MultiBuf&&) override {
    return pw::Status::Unimplemented();
  }

  pw::async2::Poll<pw::Result<pw::channel::WriteToken>> DoPendFlush(
      pw::async2::Context&) override {
    return pw::async2::Ready(
        pw::Result<pw::channel::WriteToken>(pw::Status::Unimplemented()));
  }

  // Common functions
  pw::async2::Poll<pw::Status> DoPendClose(pw::async2::Context&) override {
    return pw::OkStatus();
  }
};

TEST(Channel, MethodsShortCircuitAfterCloseReturnsReady) {
  pw::async2::Dispatcher dispatcher;

  class : public pw::async2::Task {
   public:
    ReliableByteReaderWriterStub channel;

   private:
    pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
      EXPECT_TRUE(channel.is_read_open());
      EXPECT_TRUE(channel.is_write_open());
      EXPECT_EQ(pw::async2::Ready(pw::OkStatus()), channel.PendClose(cx));
      EXPECT_FALSE(channel.is_read_open());
      EXPECT_FALSE(channel.is_write_open());

      EXPECT_EQ(pw::Status::FailedPrecondition(),
                channel.PendRead(cx)->status());
      EXPECT_EQ(Ready(pw::Status::FailedPrecondition()),
                channel.PendReadyToWrite(cx));
      EXPECT_EQ(pw::Status::FailedPrecondition(),
                channel.PendFlush(cx)->status());
      EXPECT_EQ(pw::async2::Ready(pw::Status::FailedPrecondition()),
                channel.PendClose(cx));

      return pw::async2::Ready();
    }
  } test_task;
  dispatcher.Post(test_task);

  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());
}

#if PW_NC_TEST(InvalidOrdering)
PW_NC_EXPECT("Properties must be specified in the following order");
bool Illegal(pw::channel::ByteChannel<kReadable, pw::channel::kReliable>& foo) {
  return foo.is_read_open();
}
#elif PW_NC_TEST(NoProperties)
PW_NC_EXPECT("At least one of kReadable or kWritable must be provided");
bool Illegal(pw::channel::ByteChannel<>& foo) { return foo.is_read_open(); }
#elif PW_NC_TEST(NoReadOrWrite)
PW_NC_EXPECT("At least one of kReadable or kWritable must be provided");
bool Illegal(pw::channel::ByteChannel<pw::channel::kReliable>& foo) {
  return foo.is_read_open();
}
#elif PW_NC_TEST(TooMany)
PW_NC_EXPECT("Too many properties given");
bool Illegal(
    pw::channel::
        ByteChannel<kReliable, kReliable, kReliable, kReadable, kWritable>&
            foo) {
  return foo.is_read_open();
}
#elif PW_NC_TEST(Duplicates)
PW_NC_EXPECT("duplicates");
bool Illegal(pw::channel::ByteChannel<kReadable, kReadable>& foo) {
  return foo.is_read_open();
}
#endif  // PW_NC_TEST

class TestByteReader : public ByteChannel<kReliable, kReadable> {
 public:
  TestByteReader() {}

  void PushData(MultiBuf data) {
    bool was_empty = data_.empty();
    data_.PushSuffix(std::move(data));
    if (was_empty) {
      std::move(read_waker_).Wake();
    }
  }

 private:
  Poll<pw::Result<MultiBuf>> DoPendRead(Context& cx) override {
    if (data_.empty()) {
      read_waker_ = cx.GetWaker(pw::async2::WaitReason::Unspecified());
      return Pending();
    }
    return std::move(data_);
  }

  Poll<pw::Status> DoPendClose(Context&) override {
    return Ready(pw::OkStatus());
  }

  Waker read_waker_;
  MultiBuf data_;
};

class TestDatagramWriter : public DatagramWriter {
 public:
  TestDatagramWriter(MultiBufAllocator& alloc) : alloc_(alloc) {}

  const pw::multibuf::MultiBuf& last_datagram() const { return last_dgram_; }

  void MakeReadyToWrite() {
    PW_CHECK_INT_EQ(
        state_,
        kUnavailable,
        "Can't make writable when write is pending or already writable");

    state_ = kReadyToWrite;
    std::move(waker_).Wake();
  }

  void MakeReadyToFlush() {
    PW_CHECK_INT_EQ(state_,
                    kWritePending,
                    "Can't make flushable unless a write is pending");

    state_ = kReadyToFlush;
    std::move(waker_).Wake();
  }

 private:
  Poll<pw::Status> DoPendReadyToWrite(Context& cx) override {
    if (state_ == kReadyToWrite) {
      return Ready(pw::OkStatus());
    }

    waker_ = cx.GetWaker(pw::async2::WaitReason::Unspecified());
    return Pending();
  }

  pw::Result<pw::channel::WriteToken> DoWrite(MultiBuf&& buffer) override {
    if (state_ != kReadyToWrite) {
      return pw::Status::Unavailable();
    }

    state_ = kWritePending;
    last_dgram_ = std::move(buffer);
    return CreateWriteToken(++last_write_);
  }

  pw::multibuf::MultiBufAllocator& DoGetWriteAllocator() override {
    return alloc_;
  }

  Poll<pw::Result<pw::channel::WriteToken>> DoPendFlush(Context& cx) override {
    if (state_ != kReadyToFlush) {
      waker_ = cx.GetWaker(pw::async2::WaitReason::Unspecified());
      return Pending();
    }
    last_flush_ = last_write_;
    return Ready(
        pw::Result<pw::channel::WriteToken>(CreateWriteToken(last_flush_)));
  }

  Poll<pw::Status> DoPendClose(Context&) override {
    return Ready(pw::OkStatus());
  }

  enum {
    kUnavailable,
    kReadyToWrite,
    kWritePending,
    kReadyToFlush,
  } state_ = kUnavailable;
  Waker waker_;
  uint32_t last_write_ = 0;
  uint32_t last_flush_ = 0;
  MultiBuf last_dgram_;
  MultiBufAllocator& alloc_;
};

TEST(Channel, TestByteReader) {
  static constexpr char kReadData[] = "hello, world";
  static constexpr size_t kReadDataSize = sizeof(kReadData);
  static constexpr size_t kArbitraryMetaSize = 512;

  pw::async2::Dispatcher dispatcher;
  std::array<std::byte, kReadDataSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> read_buf_opt =
      simple_allocator.Allocate(kReadDataSize);
  ASSERT_TRUE(read_buf_opt.has_value());
  MultiBuf& read_buf = *read_buf_opt;

  class : public pw::async2::Task {
   public:
    TestByteReader channel;
    int test_executed = 0;

   private:
    Poll<> DoPend(Context& cx) override {
      auto result = channel.PendRead(cx);
      if (!result.IsReady()) {
        return Pending();
      }

      auto actual_result = std::move(*result);
      EXPECT_TRUE(actual_result.ok());

      std::byte contents[kReadDataSize] = {};

      EXPECT_EQ(actual_result->size(), sizeof(kReadData));
      std::copy(actual_result->begin(), actual_result->end(), contents);
      EXPECT_STREQ(reinterpret_cast<const char*>(contents), kReadData);

      test_executed += 1;
      return Ready();
    }
  } test_task;

  dispatcher.Post(test_task);

  EXPECT_FALSE(dispatcher.RunUntilStalled().IsReady());

  auto kReadDataBytes = reinterpret_cast<const std::byte*>(kReadData);
  std::copy(kReadDataBytes, kReadDataBytes + kReadDataSize, read_buf.begin());
  test_task.channel.PushData(std::move(read_buf));
  EXPECT_TRUE(dispatcher.RunUntilStalled().IsReady());

  EXPECT_EQ(test_task.test_executed, 1);
}

TEST(Channel, TestDatagramWriter) {
  pw::async2::Dispatcher dispatcher;
  static constexpr size_t kArbitraryDataSize = 128;
  static constexpr size_t kArbitraryMetaSize = 512;
  std::array<std::byte, kArbitraryDataSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  TestDatagramWriter write_channel(simple_allocator);

  static constexpr char kWriteData[] = "Hello there";

  class SendWriteDataAndFlush : public pw::async2::Task {
   public:
    explicit SendWriteDataAndFlush(DatagramWriter& channel, size_t)
        : channel_(channel) {}
    int test_executed = 0;

   private:
    Poll<> DoPend(Context& cx) override {
      switch (state_) {
        case kWaitUntilReady: {
          if (channel_.PendReadyToWrite(cx).IsPending()) {
            return Pending();
          }
          if (!allocation_future_.has_value()) {
            allocation_future_ =
                channel_.GetWriteAllocator().AllocateAsync(sizeof(kWriteData));
          }
          Poll<std::optional<MultiBuf>> buffer = allocation_future_->Pend(cx);
          if (buffer.IsPending()) {
            return Pending();
          }
          allocation_future_ = std::nullopt;
          if (!buffer->has_value()) {
            // Allocator should have enough space for `kWriteData`.
            ADD_FAILURE();
            return Ready();
          }
          pw::ConstByteSpan str(pw::as_bytes(pw::span(kWriteData)));
          std::copy(str.begin(), str.end(), (**buffer).begin());
          auto token = channel_.Write(std::move(**buffer));
          PW_CHECK(token.ok());
          write_token_ = *token;
          state_ = kFlushPacket;
          [[fallthrough]];
        }
        case kFlushPacket: {
          auto result = channel_.PendFlush(cx);
          if (result.IsPending() || **result < write_token_) {
            return Pending();
          }
          test_executed += 1;
          state_ = kWaitUntilReady;
          return Ready();
        }
        default:
          PW_CRASH("Illegal value");
      }

      // This test is INCOMPLETE.

      test_executed += 1;
      return Ready();
    }

    enum { kWaitUntilReady, kFlushPacket } state_ = kWaitUntilReady;
    std::optional<MultiBufAllocationFuture> allocation_future_;
    DatagramWriter& channel_;
    pw::channel::WriteToken write_token_;
  };

  SendWriteDataAndFlush test_task(write_channel, 24601);
  dispatcher.Post(test_task);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  write_channel.MakeReadyToWrite();

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());

  write_channel.MakeReadyToFlush();

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(test_task.test_executed, 1);

  std::byte contents[64] = {};
  const MultiBuf& dgram = write_channel.last_datagram();
  std::copy(dgram.begin(), dgram.end(), contents);
  EXPECT_STREQ(reinterpret_cast<const char*>(contents), kWriteData);
}

void TakesAChannel(const pw::channel::AnyChannel&) {}

const pw::channel::ByteChannel<kReadable>& TakesAReadableByteChannel(
    const pw::channel::ByteChannel<kReadable>& channel) {
  return channel;
}

void TakesAWritableByteChannel(const pw::channel::ByteChannel<kWritable>&) {}

TEST(Channel, Conversions) {
  static constexpr size_t kArbitraryDataSize = 128;
  static constexpr size_t kArbitraryMetaSize = 128;
  std::array<std::byte, kArbitraryDataSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);

  const TestByteReader byte_channel;
  const TestDatagramWriter datagram_channel(simple_allocator);

  TakesAReadableByteChannel(byte_channel);
  TakesAReadableByteChannel(byte_channel.as<kReadable>());
  TakesAReadableByteChannel(byte_channel.as<pw::channel::ByteReader>());
  TakesAReadableByteChannel(
      byte_channel.as<pw::channel::ByteChannel<kReliable, kReadable>>());
  TakesAChannel(byte_channel);

  TakesAWritableByteChannel(datagram_channel.IgnoreDatagramBoundaries());

  [[maybe_unused]] const pw::channel::AnyChannel& plain = byte_channel;

#if PW_NC_TEST(CannotImplicitlyLoseWritability)
  PW_NC_EXPECT("no matching function for call");
  TakesAWritableByteChannel(byte_channel);
#elif PW_NC_TEST(CannotExplicitlyLoseWritability)
  PW_NC_EXPECT("Cannot use a non-writable channel as a writable channel");
  TakesAWritableByteChannel(byte_channel.as<kWritable>());
#endif  // PW_NC_TEST
}

#if PW_NC_TEST(CannotImplicitlyUseDatagramChannelAsByteChannel)
PW_NC_EXPECT("no matching function for call");
void DatagramChannelNcTest(
    pw::channel::DatagramChannel<kReliable, kReadable>& dgram) {
  TakesAReadableByteChannel(dgram);
}
#elif PW_NC_TEST(CannotExplicitlyUseDatagramChannelAsByteChannel)
PW_NC_EXPECT("Datagram and byte channels are not interchangeable");
void DatagramChannelNcTest(
    pw::channel::DatagramChannel<kReliable, kReadable>& dgram) {
  TakesAReadableByteChannel(dgram.as<pw::channel::ByteChannel<kReadable>>());
}
#endif  // PW_NC_TEST

class Foo {
 public:
  Foo(pw::channel::ByteChannel<kReadable>&) {}
  Foo(const Foo&) = default;
};

// Define additional overloads to ensure the right overload is selected with the
// implicit conversion.
[[maybe_unused]] void TakesAReadableByteChannel(const Foo&) {}
[[maybe_unused]] void TakesAReadableByteChannel(int) {}
[[maybe_unused]] void TakesAReadableByteChannel(
    const pw::channel::DatagramReaderWriter&) {}

TEST(Channel, SelectsCorrectOverloadWhenRelyingOnImplicitConversion) {
  TestByteReader byte_channel;

  [[maybe_unused]] Foo selects_channel_ctor_not_copy_ctor(byte_channel);
  EXPECT_EQ(&byte_channel.as<pw::channel::ByteChannel<kReadable>>(),
            &TakesAReadableByteChannel(byte_channel));
}

}  // namespace
