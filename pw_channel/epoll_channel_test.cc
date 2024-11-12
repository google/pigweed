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

#include "pw_channel/epoll_channel.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "pw_assert/check.h"
#include "pw_async2/dispatcher.h"
#include "pw_bytes/array.h"
#include "pw_bytes/suffix.h"
#include "pw_channel/channel.h"
#include "pw_multibuf/simple_allocator_for_test.h"
#include "pw_status/status.h"
#include "pw_thread/sleep.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

namespace {

using namespace std::chrono_literals;

using ::pw::async2::Context;
using ::pw::async2::Dispatcher;
using ::pw::async2::Pending;
using ::pw::async2::Poll;
using ::pw::async2::Ready;
using ::pw::async2::Task;
using ::pw::channel::ByteReader;
using ::pw::channel::ByteWriter;
using ::pw::channel::EpollChannel;
using ::pw::multibuf::MultiBuf;
using ::pw::multibuf::test::SimpleAllocatorForTest;

template <typename ChannelKind>
class ReaderTask : public Task {
 public:
  ReaderTask(ChannelKind& channel, int num_reads)
      : channel_(channel), num_reads_(num_reads) {}

  int poll_count = 0;
  int read_count = 0;
  int bytes_read = 0;
  pw::Status read_status = pw::Status::Unknown();

 private:
  Poll<> DoPend(Context& cx) final {
    ++poll_count;
    while (read_count < num_reads_) {
      auto result = channel_.PendRead(cx);
      if (result.IsPending()) {
        return Pending();
      }
      read_status = result->status();
      if (!result->ok()) {
        // We hit an error-- call it quits.
        return Ready();
      }
      ++read_count;
      bytes_read += (**result).size();

      (**result).Release();
    }

    return Ready();
  }

  ChannelKind& channel_;
  int num_reads_;
};

template <typename ChannelKind>
class CloseTask : public Task {
 public:
  CloseTask(ChannelKind& channel) : channel_(channel) {}

  pw::Status close_status = pw::Status::Unknown();

 private:
  Poll<> DoPend(Context& cx) final {
    auto result = channel_.PendClose(cx);
    if (result.IsPending()) {
      return Pending();
    }

    close_status = *result;
    return Ready();
  }

  ChannelKind& channel_;
};

class EpollChannelTest : public ::testing::Test {
 protected:
  EpollChannelTest() {
    int pipefd[2];
    PW_CHECK_INT_NE(pipe(pipefd), -1);
    read_fd_ = pipefd[0];
    write_fd_ = pipefd[1];
  }

  ~EpollChannelTest() override {
    close(read_fd_);
    close(write_fd_);
  }

  int read_fd_;
  int write_fd_;
};

template <typename Func>
class FunctionThread : public pw::thread::ThreadCore {
 public:
  explicit FunctionThread(Func&& func) : func_(std::move(func)) {}

 private:
  void Run() override { func_(); }

  Func func_;
};

TEST_F(EpollChannelTest, Read_ValidData_Succeeds) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;

  EpollChannel channel(read_fd_, dispatcher, alloc);
  ASSERT_TRUE(channel.is_read_open());
  ASSERT_TRUE(channel.is_write_open());

  ReaderTask<ByteReader> read_task(channel.channel(), 1);
  dispatcher.Post(read_task);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(read_task.poll_count, 1);
  EXPECT_EQ(read_task.read_count, 0);
  EXPECT_EQ(read_task.bytes_read, 0);

  FunctionThread delayed_write([this]() {
    pw::this_thread::sleep_for(500ms);
    const char* data = "hello world";
    PW_CHECK_INT_EQ(write(write_fd_, data, 11), 11);
  });

  pw::Thread work_thread(pw::thread::stl::Options(), delayed_write);
  work_thread.join();

  dispatcher.RunToCompletion();
  EXPECT_EQ(read_task.read_status, pw::OkStatus());
  EXPECT_EQ(read_task.poll_count, 2);
  EXPECT_EQ(read_task.read_count, 1);
  EXPECT_EQ(read_task.bytes_read, 11);

  CloseTask close_task(channel);
  dispatcher.Post(close_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(close_task.close_status, pw::OkStatus());
}

TEST_F(EpollChannelTest, Read_Closed_ReturnsFailedPrecondition) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;

  EpollChannel channel(read_fd_, dispatcher, alloc);
  ASSERT_TRUE(channel.is_read_open());
  ASSERT_TRUE(channel.is_write_open());

  CloseTask close_task(channel);
  dispatcher.Post(close_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(close_task.close_status, pw::OkStatus());

  ReaderTask<ByteReader> read_task(channel.channel(), 1);
  dispatcher.Post(read_task);

  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(read_task.read_status, pw::Status::FailedPrecondition());
}

template <typename ChannelKind>
class WriterTask : public Task {
 public:
  WriterTask(ChannelKind& channel,
             int num_writes,
             pw::ConstByteSpan data_to_write)
      : max_writes(num_writes),
        channel_(channel),
        data_to_write_(data_to_write) {}

  int poll_count = 0;
  int write_pending_count = 0;
  int write_count = 0;
  int max_writes = 0;
  pw::Status last_write_status = pw::Status::Unknown();

 private:
  Poll<> DoPend(Context& cx) final {
    ++poll_count;

    while (write_count < max_writes) {
      auto result = channel_.PendReadyToWrite(cx);
      if (result.IsPending()) {
        ++write_pending_count;
        return Pending();
      }
      last_write_status = *result;
      if (!result->ok()) {
        // We hit an error-- call it quits.
        return Ready();
      }
      ++write_count;

      Poll<std::optional<MultiBuf>> multibuf_result =
          channel_.PendAllocateWriteBuffer(cx, data_to_write_.size());
      PW_CHECK(multibuf_result.IsReady());
      PW_CHECK(multibuf_result->has_value());
      MultiBuf& multibuf = **multibuf_result;
      std::copy(data_to_write_.begin(), data_to_write_.end(), multibuf.begin());

      last_write_status = channel_.StageWrite(std::move(multibuf));

      Poll<pw::Status> write_status = channel_.PendWrite(cx);
      if (write_status.IsPending()) {
        return Pending();
      }

      PW_CHECK_OK(*write_status);
    }

    return Ready();
  }

  ChannelKind& channel_;
  pw::ConstByteSpan data_to_write_;
};

TEST_F(EpollChannelTest, Write_ValidData_Succeeds) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;

  EpollChannel channel(write_fd_, dispatcher, alloc);
  ASSERT_TRUE(channel.is_read_open());
  ASSERT_TRUE(channel.is_write_open());

  constexpr auto kData = pw::bytes::Initialized<32>(0x3f);
  WriterTask<ByteWriter> write_task(channel.channel(), 1, kData);
  dispatcher.Post(write_task);

  dispatcher.RunToCompletion();
  EXPECT_EQ(write_task.last_write_status, pw::OkStatus());

  std::array<std::byte, 64> buffer;
  EXPECT_EQ(read(read_fd_, buffer.data(), buffer.size()),
            static_cast<int>(kData.size()));
  EXPECT_EQ(std::memcmp(buffer.data(), kData.data(), kData.size()), 0);

  CloseTask close_task(channel);
  dispatcher.Post(close_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(close_task.close_status, pw::OkStatus());
}

TEST_F(EpollChannelTest, Write_EmptyData_Succeeds) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;

  EpollChannel channel(write_fd_, dispatcher, alloc);
  ASSERT_TRUE(channel.is_read_open());
  ASSERT_TRUE(channel.is_write_open());

  WriterTask<ByteWriter> write_task(channel.channel(), 1, {});
  dispatcher.Post(write_task);

  dispatcher.RunToCompletion();
  EXPECT_EQ(write_task.last_write_status, pw::OkStatus());

  CloseTask close_task(channel);
  dispatcher.Post(close_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(close_task.close_status, pw::OkStatus());
}

TEST_F(EpollChannelTest, Write_Closed_ReturnsFailedPrecondition) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;

  EpollChannel channel(write_fd_, dispatcher, alloc);
  ASSERT_TRUE(channel.is_read_open());
  ASSERT_TRUE(channel.is_write_open());

  CloseTask close_task(channel);
  dispatcher.Post(close_task);
  EXPECT_EQ(dispatcher.RunUntilStalled(), Ready());
  EXPECT_EQ(close_task.close_status, pw::OkStatus());

  WriterTask<ByteWriter> write_task(channel.channel(), 1, {});
  dispatcher.Post(write_task);

  dispatcher.RunToCompletion();
  EXPECT_EQ(write_task.last_write_status, pw::Status::FailedPrecondition());
}

TEST_F(EpollChannelTest, Destructor_ClosesFileDescriptor) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;

  {
    EpollChannel channel(write_fd_, dispatcher, alloc);
    ASSERT_TRUE(channel.is_read_open());
    ASSERT_TRUE(channel.is_write_open());
  }

  const char kArbitraryByte = 'b';
  EXPECT_EQ(write(write_fd_, &kArbitraryByte, 1), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST_F(EpollChannelTest, PendReadyToWrite_BlocksWhenUnavailable) {
  SimpleAllocatorForTest alloc;
  Dispatcher dispatcher;
  EpollChannel channel(write_fd_, dispatcher, alloc);
  ASSERT_TRUE(channel.is_read_open());
  ASSERT_TRUE(channel.is_write_open());

  constexpr auto kData =
      pw::bytes::Initialized<decltype(alloc)::data_size_bytes()>('c');
  WriterTask<ByteWriter> write_task(
      channel.channel(),
      100,  // Max writes set to some high number so the task fills the pipe.
      pw::ConstByteSpan(kData));
  dispatcher.Post(write_task);

  // Try to write a bunch of data, eventually filling the pipe and blocking the
  // task.
  EXPECT_EQ(dispatcher.RunUntilStalled(), Pending());
  EXPECT_EQ(write_task.poll_count, 1);
  EXPECT_EQ(write_task.write_pending_count, 1);
  EXPECT_EQ(write_task.last_write_status, pw::Status::Unavailable());

  const int writes_to_drain = write_task.write_count;

  // End the task on the next successful write.
  write_task.max_writes = write_task.write_count + 1;

  // Drain the pipe to make it writable again after a delay.
  FunctionThread delayed_read([this, writes_to_drain]() {
    pw::this_thread::sleep_for(500ms);
    for (int i = 0; i < writes_to_drain; ++i) {
      std::array<std::byte, decltype(alloc)::data_size_bytes()> buffer;
      PW_CHECK_INT_GT(read(read_fd_, buffer.data(), buffer.size()), 0);
    }
  });

  pw::Thread work_thread(pw::thread::stl::Options(), delayed_read);

  dispatcher.RunToCompletion();
  work_thread.join();

  EXPECT_EQ(write_task.poll_count, 2);
  EXPECT_EQ(write_task.last_write_status, pw::OkStatus());
}

}  // namespace
