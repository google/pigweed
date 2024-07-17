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

#include "pw_bluetooth_sapphire/internal/host/l2cap/credit_based_flow_control_rx_engine.h"

#include "pw_bluetooth_sapphire/internal/host/l2cap/fragmenter.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_unit_test/framework.h"

namespace bt::l2cap::internal {
namespace {

using Engine = CreditBasedFlowControlRxEngine;

class CreditBasedFlowControlRxEngineTest : public ::testing::Test {
 protected:
  static constexpr hci_spec::ConnectionHandle kTestHandle = 0x0001;
  static constexpr ChannelId kTestChannelId = 0x0001;

  Engine& engine() { return *engine_; }
  size_t failure_callback_count() { return failure_callback_count_; }
  PDU ToPdu(const ByteBuffer& buffer) {
    return Fragmenter(kTestHandle)
        .BuildFrame(kTestChannelId, buffer, FrameCheckSequenceOption::kNoFcs);
  }

  ByteBufferPtr ProcessPayload(const ByteBuffer& buffer) {
    return engine().ProcessPdu(ToPdu(buffer));
  }

 private:
  size_t failure_callback_count_ = 0;
  std::unique_ptr<Engine> engine_ =
      std::make_unique<Engine>([this] { ++failure_callback_count_; });
};

TEST_F(CreditBasedFlowControlRxEngineTest, SmallUnsegmentedSdu) {
  // clang-format off
  const StaticByteBuffer payload(
      // SDU size field (LE u16)
      4, 0,
      // Payload
      't', 'e', 's', 't'
  );
  // clang-format on

  const ByteBufferPtr sdu = ProcessPayload(payload);

  ASSERT_TRUE(sdu);
  EXPECT_TRUE(ContainersEqual(StaticByteBuffer('t', 'e', 's', 't'), *sdu));
  EXPECT_EQ(0u, failure_callback_count());
}

TEST_F(CreditBasedFlowControlRxEngineTest, LargeUnsegmentedSdu) {
  // clang-format off
  const StaticByteBuffer payload(
      // SDU size field (LE u16)
      58, 0,
      // Payload
      'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p', 's', 'u', 'm', ' ', 'd', 'o', 'l',
      'o', 'r', ' ', 's', 'i', 't', ' ', 'a', 'm', 'e', 't', ',', ' ', 'c', 'o',
      'n', 's', 'e', 'c', 't', 'e', 't', 'u', 'r', ' ', 'a', 'd', 'i', 'p', 'i',
      's', 'c', 'i', 'n', 'g', ' ', 'e', 'l', 'i', 't', '.', ' ', 'S'
  );
  // clang-format on

  const ByteBufferPtr sdu = ProcessPayload(payload);
  ASSERT_TRUE(sdu);
  EXPECT_TRUE(ContainersEqual(payload.view(2), *sdu));
  EXPECT_EQ(0u, failure_callback_count());
}

TEST_F(CreditBasedFlowControlRxEngineTest, SduSegmentedIntoManySmallPdus) {
  // clang-format off
  EXPECT_FALSE(ProcessPayload(StaticByteBuffer(
      // SDU size field (LE u16)
      16, 0,
      // First four bytes of payload
      't', 'e', 's', 't'
  )));
  // clang-format on

  EXPECT_FALSE(ProcessPayload(StaticByteBuffer('i', 'n', 'g', ' ')));
  EXPECT_FALSE(ProcessPayload(StaticByteBuffer('f', 'o', 'r', ' ')));
  const ByteBufferPtr sdu =
      ProcessPayload(StaticByteBuffer('b', 'u', 'g', 's'));

  // clang-format off
  StaticByteBuffer expected(
      't', 'e', 's', 't', 'i', 'n', 'g', ' ',
      'f', 'o', 'r', ' ', 'b', 'u', 'g', 's'
  );
  // clang-format on

  ASSERT_TRUE(sdu);
  EXPECT_TRUE(ContainersEqual(expected, *sdu));
  EXPECT_EQ(0u, failure_callback_count());
}

TEST_F(CreditBasedFlowControlRxEngineTest, FailSduSmallerThanPayload) {
  // clang-format off
  const StaticByteBuffer payload(
      // SDU size field (LE u16)
      4, 0,
      // Payload
      'f', 'a', 'i', 'l', 'i', 'u', 'r', 'e'
  );
  // clang-format on

  EXPECT_FALSE(ProcessPayload(payload));
  EXPECT_EQ(1u, failure_callback_count());
}

TEST_F(CreditBasedFlowControlRxEngineTest, FailSduSmallerThanPayloadSegmented) {
  // clang-format off
  const StaticByteBuffer payload(
      // SDU size field (LE u16)
      5, 0,
      // Payload
      'f', 'a', 'i', 'l'
  );
  // clang-format on

  EXPECT_FALSE(ProcessPayload(payload));
  EXPECT_EQ(0u, failure_callback_count());
  EXPECT_FALSE(ProcessPayload(StaticByteBuffer('i', 'u', 'r', 'e')));
  EXPECT_EQ(1u, failure_callback_count());
}

TEST_F(CreditBasedFlowControlRxEngineTest, InitialFrameMissingSduSize) {
  const ByteBufferPtr sdu = ProcessPayload(StaticByteBuffer(0));
  EXPECT_FALSE(sdu);
  EXPECT_EQ(1u, failure_callback_count());
}

}  // namespace
}  // namespace bt::l2cap::internal
