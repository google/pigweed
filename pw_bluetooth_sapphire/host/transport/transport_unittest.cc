// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "transport.h"

#include "lib/inspect/cpp/inspector.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"

namespace bt::hci {

namespace {

using TransportTest = bt::testing::ControllerTest<bt::testing::MockController>;
using TransportDeathTest = TransportTest;

class TransportTestWithoutSco : public TransportTest {
 public:
  void SetUp() override { TransportTest::SetUp(/*sco_enabled=*/false); }
};

TEST_F(TransportTest, CommandChannelTimeoutShutsDownChannelAndNotifiesClosedCallback) {
  size_t closed_cb_count = 0;
  transport()->SetTransportClosedCallback([&] { closed_cb_count++; });

  constexpr zx::duration kCommandTimeout = zx::sec(12);

  StaticByteBuffer req_reset(LowerBits(hci_spec::kReset),
                             UpperBits(hci_spec::kReset),  // HCI_Reset opcode
                             0x00                          // parameter_total_size
  );

  // Expect the HCI_Reset command but dont send a reply back to make the command
  // time out.
  EXPECT_CMD_PACKET_OUT(test_device(), req_reset);
  StartTestDevice();

  size_t cb_count = 0;
  CommandChannel::TransactionId id1, id2;
  auto cb = [&cb_count, &id1, &id2](CommandChannel::TransactionId callback_id,
                                    const EventPacket& event) {
    cb_count++;
    EXPECT_TRUE(callback_id == id1 || callback_id == id2);
    EXPECT_EQ(hci_spec::kCommandStatusEventCode, event.event_code());

    const auto params = event.params<hci_spec::CommandStatusEventParams>();
    EXPECT_EQ(hci_spec::StatusCode::kUnspecifiedError, params.status);
    EXPECT_EQ(hci_spec::kReset, params.command_opcode);
  };

  auto packet = CommandPacket::New(hci_spec::kReset);
  id1 = cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id1);

  packet = CommandPacket::New(hci_spec::kReset);
  id2 = cmd_channel()->SendCommand(std::move(packet), cb);
  ASSERT_NE(0u, id2);

  // Run the loop until the command timeout task gets scheduled.
  RunLoopUntilIdle();
  ASSERT_EQ(0u, cb_count);
  EXPECT_EQ(0u, closed_cb_count);

  RunLoopFor(kCommandTimeout);
  EXPECT_EQ(2u, cb_count);
  EXPECT_EQ(1u, closed_cb_count);
}

TEST_F(TransportDeathTest, AttachInspectBeforeInitializeACLDataChannelCrashes) {
  inspect::Inspector inspector;
  EXPECT_DEATH_IF_SUPPORTED(transport()->AttachInspect(inspector.GetRoot()), ".*");
}

TEST_F(TransportTest, ScoChannelClosed) {
  StartTestDevice();

  size_t closed_cb_count = 0;
  transport()->SetTransportClosedCallback([&] { closed_cb_count++; });

  EXPECT_TRUE(transport()->InitializeScoDataChannel(
      DataBufferInfo(/*max_data_length=*/1, /*max_num_packets=*/1)));
  RunLoopUntilIdle();

  test_device()->CloseScoDataChannel();
  RunLoopUntilIdle();
  EXPECT_EQ(closed_cb_count, 1u);
}

TEST_F(TransportTestWithoutSco, GetScoChannelFailure) {
  size_t closed_cb_count = 0;
  transport()->SetTransportClosedCallback([&] { closed_cb_count++; });
  EXPECT_FALSE(transport()->InitializeScoDataChannel(
      DataBufferInfo(/*max_data_length=*/1, /*max_num_packets=*/1)));
  RunLoopUntilIdle();
  EXPECT_EQ(closed_cb_count, 0u);
}

TEST_F(TransportTest, InitializeScoFailsBufferNotAvailable) {
  EXPECT_FALSE(transport()->InitializeScoDataChannel(
      DataBufferInfo(/*max_data_length=*/0, /*max_num_packets=*/0)));
}

}  // namespace
}  // namespace bt::hci
