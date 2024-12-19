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

#include "pw_uart/blocking_adapter.h"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "pw_assert/check.h"
#include "pw_bytes/array.h"
#include "pw_log/log.h"
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"
#include "pw_sync/timed_thread_notification.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_unit_test/framework.h"
#include "pw_work_queue/work_queue.h"

// Waits for something critical for test execution.
// We use PW_CHECK to ensure we crash on timeout instead of hanging forever.
// This is a macro so the crash points to the invocation site.
#define ASSERT_WAIT(waitable) PW_CHECK(waitable.try_acquire_for(1000ms))

namespace pw::uart {
namespace {

using namespace std::chrono_literals;

// A mock UartNonBlocking for testing the blocking adapter.
class UartNonBlockingMock : public UartNonBlocking {
 public:
  bool enabled() const { return enabled_; }

  void WaitAndCompleteRead(Status status, ConstByteSpan data) {
    // Wait for a read to start.
    ASSERT_WAIT(read_started_);

    std::optional<ReadTransaction> read = ConsumeCurrentRead();
    PW_CHECK(read.has_value());

    // Copy data into rx buffer;
    PW_CHECK_UINT_GE(read->rx_buffer.size(), data.size());
    std::copy(data.begin(), data.end(), read->rx_buffer.begin());

    read->Complete(status, data.size());
  }

  ConstByteSpan WaitForWrite() PW_LOCKS_EXCLUDED(mutex_) {
    // Wait for a write to start.
    ASSERT_WAIT(write_started_);

    std::lock_guard lock(mutex_);
    PW_CHECK(current_write_.has_value());
    return current_write_->tx_buffer;
  }

  void CompleteWrite(StatusWithSize status_size) {
    std::optional<WriteTransaction> write = ConsumeCurrentWrite();
    PW_CHECK(write.has_value());
    write->Complete(status_size);
  }

  void WaitAndCompleteFlush(Status status) {
    // Wait for a flush to start.
    ASSERT_WAIT(flush_started_);

    std::optional<FlushTransaction> flush = ConsumeCurrentFlush();
    PW_CHECK(flush.has_value());

    flush->Complete(status);
  }

 private:
  sync::Mutex mutex_;
  bool enabled_ = false;

  //
  // UartNonBlocking impl.
  //
  Status DoEnable(bool enabled) override {
    enabled_ = enabled;
    return OkStatus();
  }

  Status DoSetBaudRate(uint32_t) override { return OkStatus(); }
  size_t DoConservativeReadAvailable() override { return 0; }
  Status DoClearPendingReceiveBytes() override { return OkStatus(); }

  // Read
  struct ReadTransaction {
    ByteSpan rx_buffer;
    size_t min_bytes;
    Function<void(Status, ConstByteSpan buffer)> callback;

    void Complete(Status status, size_t num_bytes) {
      callback(status, rx_buffer.first(num_bytes));
    }
  };
  std::optional<ReadTransaction> current_read_ PW_GUARDED_BY(mutex_);
  sync::TimedThreadNotification read_started_;

  std::optional<ReadTransaction> ConsumeCurrentRead()
      PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    return std::exchange(current_read_, std::nullopt);
  }

  Status DoRead(ByteSpan rx_buffer,
                size_t min_bytes,
                Function<void(Status, ConstByteSpan buffer)>&& callback)
      override PW_LOCKS_EXCLUDED(mutex_) {
    {
      std::lock_guard lock(mutex_);

      if (current_read_) {
        return Status::Unavailable();
      }

      current_read_.emplace(ReadTransaction{
          .rx_buffer = rx_buffer,
          .min_bytes = min_bytes,
          .callback = std::move(callback),
      });
    }

    read_started_.release();
    return OkStatus();
  }

  bool DoCancelRead() override {
    std::optional<ReadTransaction> read = ConsumeCurrentRead();
    if (!read.has_value()) {
      return false;
    }
    read->Complete(Status::Cancelled(), 0);
    return true;
  }

  // Write
  struct WriteTransaction {
    ConstByteSpan tx_buffer;
    Function<void(StatusWithSize)> callback;

    void Complete(StatusWithSize status_size) { callback(status_size); }
  };
  std::optional<WriteTransaction> current_write_ PW_GUARDED_BY(mutex_);
  sync::TimedThreadNotification write_started_;

  std::optional<WriteTransaction> ConsumeCurrentWrite()
      PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    return std::exchange(current_write_, std::nullopt);
  }

  Status DoWrite(ConstByteSpan tx_buffer,
                 Function<void(StatusWithSize status)>&& callback) override
      PW_LOCKS_EXCLUDED(mutex_) {
    {
      std::lock_guard lock(mutex_);

      if (current_write_) {
        return Status::Unavailable();
      }

      current_write_.emplace(WriteTransaction{
          .tx_buffer = tx_buffer,
          .callback = std::move(callback),
      });
    }

    write_started_.release();
    return OkStatus();
  }

  bool DoCancelWrite() override {
    std::optional<WriteTransaction> write = ConsumeCurrentWrite();
    if (!write.has_value()) {
      return false;
    }
    write->Complete(StatusWithSize::Cancelled(0));
    return true;
  }

  // Flush
  struct FlushTransaction {
    Function<void(Status)> callback;

    void Complete(Status status) { callback(status); }
  };
  std::optional<FlushTransaction> current_flush_ PW_GUARDED_BY(mutex_);
  sync::TimedThreadNotification flush_started_;

  std::optional<FlushTransaction> ConsumeCurrentFlush()
      PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    return std::exchange(current_flush_, std::nullopt);
  }

  Status DoFlushOutput(Function<void(Status status)>&& callback) override
      PW_LOCKS_EXCLUDED(mutex_) {
    {
      std::lock_guard lock(mutex_);

      if (current_flush_) {
        return Status::Unavailable();
      }

      current_flush_.emplace(FlushTransaction{
          .callback = std::move(callback),
      });
    }

    flush_started_.release();
    return OkStatus();
  }

  bool DoCancelFlushOutput() override {
    std::optional<FlushTransaction> flush = ConsumeCurrentFlush();
    if (!flush.has_value()) {
      return false;
    }
    flush->Complete(Status::Cancelled());
    return true;
  }
};

// Test fixture
class BlockingAdapterTest : public ::testing::Test {
 protected:
  BlockingAdapterTest() : adapter(underlying) {}

  UartNonBlockingMock underlying;
  UartBlockingAdapter adapter;

  work_queue::WorkQueueWithBuffer<2> work_queue;

  // State used by tests.
  // Ideally these would be locals, but that would require capturing more than
  // one pointer worth of data, exceeding PW_FUNCTION_INLINE_CALLABLE_SIZE.
  sync::TimedThreadNotification blocking_action_complete;
  static constexpr auto kReadBufferSize = 16;
  std::array<std::byte, kReadBufferSize> read_buffer;
  StatusWithSize read_result;
  Status write_result;

  void SetUp() override { StartWorkQueueThread(); }

  void TearDown() override { StopWorkQueueThread(); }

  void StartWorkQueueThread() {
    PW_CHECK(!work_queue_thread_, "WorkQueue thread already started");
    work_queue_thread_context_ =
        std::make_unique<thread::test::TestThreadContext>();
    work_queue_thread_.emplace(work_queue_thread_context_->options(),
                               work_queue);
  }

  void StopWorkQueueThread() {
    if (work_queue_thread_) {
      PW_LOG_DEBUG("Stopping work queue...");
      work_queue.RequestStop();
#if PW_THREAD_JOINING_ENABLED
      work_queue_thread_->join();
#else
      work_queue_thread_->detach();
#endif
      // Once stopped, the WorkQueue cannot be started again (stop_requested_
      // latches), so we don't set work_queue_thread_ to std::nullopt here.
      // work_queue_thread_ = std::nullopt;
    }
  }

 private:
  std::unique_ptr<thread::test::TestThreadContext> work_queue_thread_context_;
  std::optional<thread::Thread> work_queue_thread_;
};

//
// Enable
//

TEST_F(BlockingAdapterTest, EnableWorks) {
  // Start out disabled
  ASSERT_FALSE(underlying.enabled());

  // Can enable
  PW_TEST_EXPECT_OK(adapter.Enable());
  EXPECT_TRUE(underlying.enabled());
}

TEST_F(BlockingAdapterTest, DisableWorks) {
  // Start out enabled
  PW_TEST_ASSERT_OK(underlying.Enable());
  ASSERT_TRUE(underlying.enabled());

  // Can disable
  PW_TEST_EXPECT_OK(adapter.Disable());
  EXPECT_FALSE(underlying.enabled());
}

//
// Read
//

TEST_F(BlockingAdapterTest, ReadWorks) {
  // Call blocking ReadExactly on the work queue.
  work_queue.CheckPushWork([this]() {
    PW_LOG_DEBUG("Calling adapter.ReadExactly()...");
    read_result = adapter.ReadExactly(read_buffer);
    blocking_action_complete.release();
  });

  constexpr auto kRxData = bytes::Array<0x12, 0x34, 0x56>();
  static_assert(kRxData.size() <= kReadBufferSize);

  underlying.WaitAndCompleteRead(OkStatus(), kRxData);

  // Wait for the read to complete.
  ASSERT_WAIT(blocking_action_complete);

  PW_TEST_EXPECT_OK(read_result.status());
  EXPECT_EQ(read_result.size(), kRxData.size());
  EXPECT_TRUE(std::equal(kRxData.begin(), kRxData.end(), read_buffer.begin()));
}

TEST_F(BlockingAdapterTest, ReadHandlesTimeouts) {
  // Call blocking TryReadExactlyFor on the work queue.
  work_queue.CheckPushWork([this]() {
    PW_LOG_DEBUG("Calling adapter.TryReadExactlyFor()...");
    read_result = adapter.TryReadExactlyFor(read_buffer, 100ms);
    blocking_action_complete.release();
  });

  // Don't complete the transaction; let it time out.

  // Wait for the read to complete.
  ASSERT_WAIT(blocking_action_complete);

  EXPECT_EQ(read_result.status(), Status::DeadlineExceeded());
}

//
// Write
//
TEST_F(BlockingAdapterTest, WriteWorks) {
  static constexpr auto kTxData = bytes::Array<0x12, 0x34, 0x56>();

  // Call blocking Write on the work queue.
  work_queue.CheckPushWork([this]() {
    PW_LOG_DEBUG("Calling adapter.Write()...");
    write_result = adapter.Write(kTxData);
    blocking_action_complete.release();
  });

  ConstByteSpan tx_buffer = underlying.WaitForWrite();
  EXPECT_EQ(tx_buffer.size(), kTxData.size());
  EXPECT_TRUE(std::equal(tx_buffer.begin(), tx_buffer.end(), kTxData.begin()));

  underlying.CompleteWrite(StatusWithSize(tx_buffer.size()));

  // Wait for the write to complete.
  ASSERT_WAIT(blocking_action_complete);
  PW_TEST_EXPECT_OK(write_result);
}

TEST_F(BlockingAdapterTest, WriteHandlesTimeouts) {
  static constexpr auto kTxData = bytes::Array<0x12, 0x34, 0x56>();

  // Call blocking TryWriteFor on the work queue.
  work_queue.CheckPushWork([this]() {
    PW_LOG_DEBUG("Calling adapter.TryWriteFor()...");
    write_result = adapter.TryWriteFor(kTxData, 100ms).status();
    blocking_action_complete.release();
  });

  // Don't complete the transaction; let it time out.

  // Wait for the write to complete.
  ASSERT_WAIT(blocking_action_complete);
  EXPECT_EQ(write_result, Status::DeadlineExceeded());
}

//
// FlushOutput
//
TEST_F(BlockingAdapterTest, FlushOutputWorks) {
  // Call blocking FlushOutput on the work queue.
  work_queue.CheckPushWork([this]() {
    PW_LOG_DEBUG("Calling adapter.FlushOutput()...");
    write_result = adapter.FlushOutput();
    blocking_action_complete.release();
  });

  underlying.WaitAndCompleteFlush(OkStatus());

  // Wait for the flush to complete.
  ASSERT_WAIT(blocking_action_complete);
  PW_TEST_EXPECT_OK(write_result);
}

// FlushOutput does not provide a variant with timeout.

}  // namespace
}  // namespace pw::uart
