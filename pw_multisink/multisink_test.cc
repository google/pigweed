// Copyright 2021 The Pigweed Authors
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

#include "pw_multisink/multisink.h"

#include "gtest/gtest.h"

namespace pw::multisink {
using Drain = MultiSink::Drain;
using Listener = MultiSink::Listener;

class CountingListener : public Listener {
 public:
  void OnNewEntryAvailable() override { notification_count_++; }

  size_t GetNotificationCount() { return notification_count_; }

  void ResetNotificationCount() { notification_count_ = 0; }

 private:
  size_t notification_count_ = 0;
};

class MultiSinkTest : public ::testing::Test {
 protected:
  static constexpr std::byte kMessage[] = {
      (std::byte)0xDE, (std::byte)0xAD, (std::byte)0xBE, (std::byte)0xEF};
  static constexpr size_t kMaxDrains = 3;
  static constexpr size_t kMaxListeners = 3;
  static constexpr size_t kEntryBufferSize = 1024;
  static constexpr size_t kBufferSize = 5 * kEntryBufferSize;

  MultiSinkTest() : multisink_(buffer_) {}

  void ExpectMessageAndDropCount(Drain& drain,
                                 std::span<const std::byte> expected_message,
                                 uint32_t expected_drop_count) {
    uint32_t drop_count = 0;
    Result<ConstByteSpan> result = drain.GetEntry(entry_buffer_, drop_count);
    if (expected_message.empty()) {
      EXPECT_EQ(Status::OutOfRange(), result.status());
    } else {
      ASSERT_TRUE(result.ok());
      EXPECT_EQ(memcmp(result.value().data(),
                       expected_message.data(),
                       expected_message.size_bytes()),
                0);
    }
    EXPECT_EQ(drop_count, expected_drop_count);
  }

  void ExpectNotificationCount(CountingListener& listener,
                               size_t expected_notification_count) {
    EXPECT_EQ(listener.GetNotificationCount(), expected_notification_count);
    listener.ResetNotificationCount();
  }

  std::byte buffer_[kBufferSize];
  std::byte entry_buffer_[kEntryBufferSize];
  CountingListener listeners_[kMaxListeners];
  Drain drains_[kMaxDrains];
  MultiSink multisink_;
};

TEST_F(MultiSinkTest, SingleDrain) {
  multisink_.AttachDrain(drains_[0]);
  multisink_.AttachListener(listeners_[0]);
  multisink_.HandleEntry(kMessage);

  // Single entry push and pop.
  ExpectNotificationCount(listeners_[0], 1u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);

  // Multiple entries with intermittent drops.
  multisink_.HandleEntry(kMessage);
  multisink_.HandleDropped();
  multisink_.HandleEntry(kMessage);
  ExpectNotificationCount(listeners_[0], 3u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);

  // Send drops only.
  multisink_.HandleDropped();
  ExpectNotificationCount(listeners_[0], 1u);
  ExpectMessageAndDropCount(drains_[0], {}, 1u);

  // Confirm out-of-range if no entries are expected.
  ExpectNotificationCount(listeners_[0], 0u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

TEST_F(MultiSinkTest, MultipleDrain) {
  multisink_.AttachDrain(drains_[0]);
  multisink_.AttachDrain(drains_[1]);
  multisink_.AttachListener(listeners_[0]);
  multisink_.AttachListener(listeners_[1]);

  multisink_.HandleEntry(kMessage);
  multisink_.HandleEntry(kMessage);
  multisink_.HandleDropped();
  multisink_.HandleEntry(kMessage);
  multisink_.HandleDropped();

  // Drain one drain entirely.
  ExpectNotificationCount(listeners_[0], 5u);
  ExpectNotificationCount(listeners_[1], 5u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);
  ExpectMessageAndDropCount(drains_[0], {}, 1u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);

  // Confirm the other drain can be drained separately.
  ExpectNotificationCount(listeners_[0], 0u);
  ExpectNotificationCount(listeners_[1], 0u);
  ExpectMessageAndDropCount(drains_[1], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[1], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[1], kMessage, 1u);
  ExpectMessageAndDropCount(drains_[1], {}, 1u);
  ExpectMessageAndDropCount(drains_[1], {}, 0u);
}

TEST_F(MultiSinkTest, LateDrainRegistration) {
  // Confirm that entries pushed before attaching a drain or listener are not
  // seen by either.
  multisink_.HandleEntry(kMessage);

  // The drain does not observe 'drops' as it did not see entries, and only sees
  // the one entry that was added after attach.
  multisink_.AttachDrain(drains_[0]);
  multisink_.AttachListener(listeners_[0]);
  ExpectNotificationCount(listeners_[0], 0u);

  multisink_.HandleEntry(kMessage);
  ExpectNotificationCount(listeners_[0], 1u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

TEST_F(MultiSinkTest, DynamicDrainRegistration) {
  multisink_.AttachDrain(drains_[0]);
  multisink_.AttachListener(listeners_[0]);

  multisink_.HandleDropped();
  multisink_.HandleEntry(kMessage);
  multisink_.HandleDropped();
  multisink_.HandleEntry(kMessage);

  // Drain out one message and detach it.
  ExpectNotificationCount(listeners_[0], 4u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);
  multisink_.DetachDrain(drains_[0]);
  multisink_.DetachListener(listeners_[0]);

  // Reattach the drain and confirm that you only see events after attaching.
  multisink_.AttachDrain(drains_[0]);
  multisink_.AttachListener(listeners_[0]);
  ExpectNotificationCount(listeners_[0], 0u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);

  multisink_.HandleEntry(kMessage);
  ExpectNotificationCount(listeners_[0], 1u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

TEST_F(MultiSinkTest, TooSmallBuffer) {
  multisink_.AttachDrain(drains_[0]);

  // Insert an entry and a drop, then try to read into an insufficient buffer.
  uint32_t drop_count = 0;
  multisink_.HandleDropped();
  multisink_.HandleEntry(kMessage);

  // Attempting to acquire an entry should result in RESOURCE_EXHAUSTED.
  Result<ConstByteSpan> result =
      drains_[0].GetEntry(std::span(entry_buffer_, 1), drop_count);
  EXPECT_EQ(result.status(), Status::ResourceExhausted());

  // Verify that the multisink does not move the handled sequence ID counter
  // forward and provides this data on the next call.
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

TEST_F(MultiSinkTest, Iterator) {
  multisink_.AttachDrain(drains_[0]);

  // Insert entries and consume them all.
  multisink_.HandleEntry(kMessage);
  multisink_.HandleEntry(kMessage);
  multisink_.HandleEntry(kMessage);

  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);

  // Confirm that the iterator still observes the messages in the ring buffer.
  size_t iterated_entries = 0;
  for (ConstByteSpan entry : multisink_.UnsafeIteration()) {
    EXPECT_EQ(memcmp(entry.data(), kMessage, sizeof(kMessage)), 0);
    iterated_entries++;
  }
  EXPECT_EQ(iterated_entries, 3u);
}

TEST_F(MultiSinkTest, IteratorNoDrains) {
  // Insert entries with no drains attached. Even though there are no consumers,
  // iterators should still walk from the oldest entry.
  multisink_.HandleEntry(kMessage);
  multisink_.HandleEntry(kMessage);
  multisink_.HandleEntry(kMessage);

  // Confirm that the iterator still observes the messages in the ring buffer.
  size_t iterated_entries = 0;
  for (ConstByteSpan entry : multisink_.UnsafeIteration()) {
    EXPECT_EQ(memcmp(entry.data(), kMessage, sizeof(kMessage)), 0);
    iterated_entries++;
  }
  EXPECT_EQ(iterated_entries, 3u);
}

TEST_F(MultiSinkTest, IteratorNoEntries) {
  // Attach a drain, but don't add any entries.
  multisink_.AttachDrain(drains_[0]);
  // Confirm that the iterator has no entries.
  MultiSink::UnsafeIterationWrapper unsafe_iterator =
      multisink_.UnsafeIteration();
  EXPECT_EQ(unsafe_iterator.begin(), unsafe_iterator.end());
}

}  // namespace pw::multisink
