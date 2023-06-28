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

#include "pw_rpc_transport/stream_rpc_dispatcher.h"

#include <algorithm>
#include <atomic>

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_log/log.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_stream/stream.h"
#include "pw_sync/mutex.h"
#include "pw_sync/thread_notification.h"
#include "pw_thread/thread.h"
#include "pw_thread_stl/options.h"

namespace pw::rpc {
namespace {

using namespace std::chrono_literals;

class TestIngress : public RpcIngressHandler {
 public:
  explicit TestIngress(size_t num_bytes_expected)
      : num_bytes_expected_(num_bytes_expected) {}

  Status ProcessIncomingData(ConstByteSpan buffer) override {
    if (num_bytes_expected_ > 0) {
      std::copy(buffer.begin(), buffer.end(), std::back_inserter(received_));
      num_bytes_expected_ -= std::min(num_bytes_expected_, buffer.size());
    }
    if (num_bytes_expected_ == 0) {
      done_.release();
    }
    return OkStatus();
  }

  std::vector<std::byte> received() const { return received_; }
  void Wait() { done_.acquire(); }

 private:
  size_t num_bytes_expected_ = 0;
  sync::ThreadNotification done_;
  std::vector<std::byte> received_;
};

class TestStream : public stream::NonSeekableReader {
 public:
  TestStream() : position_(0) {}

  void QueueData(ConstByteSpan data) {
    std::lock_guard lock(send_mutex_);
    std::copy(data.begin(), data.end(), std::back_inserter(to_send_));
    available_.release();
  }

  void Stop() {
    stopped_ = true;
    available_.release();
  }

 private:
  void WaitForData() {
    while (!stopped_) {
      {
        std::lock_guard lock(send_mutex_);
        if (position_ < to_send_.size()) {
          break;
        }
      }

      available_.acquire();
    }
  }

  StatusWithSize DoRead(ByteSpan out) final {
    WaitForData();

    if (stopped_) {
      return StatusWithSize(0);
    }

    std::lock_guard lock(send_mutex_);

    if (position_ == to_send_.size()) {
      return StatusWithSize::OutOfRange();
    }

    size_t to_copy = std::min(out.size(), to_send_.size() - position_);
    std::memcpy(out.data(), to_send_.data() + position_, to_copy);
    position_ += to_copy;

    return StatusWithSize(to_copy);
  }

  sync::Mutex send_mutex_;
  std::vector<std::byte> to_send_;
  std::atomic<bool> stopped_ = false;
  size_t position_;
  sync::ThreadNotification available_;
};

TEST(StreamRpcDispatcherTest, RecvOk) {
  constexpr size_t kWriteSize = 10;
  constexpr std::array<std::byte, kWriteSize> kWriteBuffer = {};

  TestIngress test_ingress(kWriteSize);
  TestStream test_stream;

  auto dispatcher = StreamRpcDispatcher<kWriteSize>(test_stream, test_ingress);
  auto dispatcher_thread = thread::Thread(thread::stl::Options(), dispatcher);

  test_stream.QueueData(kWriteBuffer);

  test_ingress.Wait();

  dispatcher.Stop();
  test_stream.Stop();
  dispatcher_thread.join();

  auto received = test_ingress.received();
  EXPECT_EQ(received.size(), kWriteSize);
  EXPECT_EQ(dispatcher.num_read_errors(), 0U);
}

}  // namespace
}  // namespace pw::rpc
