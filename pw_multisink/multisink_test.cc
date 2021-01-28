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
#include "pw_multisink/drain.h"

namespace pw::multisink {

class MultiSinkTest : public ::testing::Test {
 protected:
  static constexpr std::byte kMessage[] = {
      (std::byte)0xDE, (std::byte)0xAD, (std::byte)0xBE, (std::byte)0xEF};
  static constexpr size_t kMaxDrains = 3;
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

  std::byte buffer_[kBufferSize];
  std::byte entry_buffer_[kEntryBufferSize];
  Drain drains_[kMaxDrains];
  MultiSink multisink_;
};

TEST_F(MultiSinkTest, SingleDrain) {
  EXPECT_EQ(OkStatus(), multisink_.AttachDrain(drains_[0]));
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));

  // Single entry push and pop.
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);

  // Multiple entries with intermittent drops.
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  multisink_.HandleDropped();
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);

  // Send drops only.
  multisink_.HandleDropped();
  ExpectMessageAndDropCount(drains_[0], {}, 1u);

  // Confirm out-of-range if no entries are expected.
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

TEST_F(MultiSinkTest, MultipleDrain) {
  EXPECT_EQ(OkStatus(), multisink_.AttachDrain(drains_[0]));
  EXPECT_EQ(OkStatus(), multisink_.AttachDrain(drains_[1]));

  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  multisink_.HandleDropped();
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  multisink_.HandleDropped();

  // Drain one drain entirely.
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);
  ExpectMessageAndDropCount(drains_[0], {}, 1u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);

  // Confirm the other drain can be drained separately.
  ExpectMessageAndDropCount(drains_[1], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[1], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[1], kMessage, 1u);
  ExpectMessageAndDropCount(drains_[1], {}, 1u);
  ExpectMessageAndDropCount(drains_[1], {}, 0u);
}

TEST_F(MultiSinkTest, LateRegistration) {
  // Confirm that entries pushed before attaching a drain are not seen by the
  // drain.
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));

  // The drain does not observe 'drops' as it did not see entries, and only sees
  // the one entry that was added after attach.
  EXPECT_EQ(OkStatus(), multisink_.AttachDrain(drains_[0]));
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

TEST_F(MultiSinkTest, DynamicDrainRegistration) {
  EXPECT_EQ(OkStatus(), multisink_.AttachDrain(drains_[0]));

  multisink_.HandleDropped();
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  multisink_.HandleDropped();
  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));

  // Drain out one message and detach it.
  ExpectMessageAndDropCount(drains_[0], kMessage, 1u);
  EXPECT_EQ(OkStatus(), multisink_.DetachDrain(drains_[0]));

  // Reattach the drain and confirm that you only see events after attaching.
  EXPECT_EQ(OkStatus(), multisink_.AttachDrain(drains_[0]));
  ExpectMessageAndDropCount(drains_[0], {}, 0u);

  EXPECT_EQ(OkStatus(), multisink_.HandleEntry(kMessage));
  ExpectMessageAndDropCount(drains_[0], kMessage, 0u);
  ExpectMessageAndDropCount(drains_[0], {}, 0u);
}

}  // namespace pw::multisink
