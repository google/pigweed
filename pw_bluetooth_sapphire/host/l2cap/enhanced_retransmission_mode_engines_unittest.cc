// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enhanced_retransmission_mode_engines.h"

#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

class L2CAP_EnhancedRetransmissionModeEnginesTest : public ::gtest::TestLoopFixture {};

constexpr size_t kMaxTransmissions = 2;
constexpr size_t kTxWindow = 63;

constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr ChannelId kTestChannelId = 0x0001;
constexpr uint8_t kExtendedControlFBitMask = 0b1000'0000;
constexpr uint8_t kExtendedControlRejFunctionMask = 0b0000'0100;

void NoOpTxCallback(ByteBufferPtr){};
void NoOpFailureCallback(){};

TEST_F(L2CAP_EnhancedRetransmissionModeEnginesTest, MakeLinkedERTMEngines) {
  auto [rx_engine, tx_engine] =
      MakeLinkedEnhancedRetransmissionModeEngines(kTestChannelId, kDefaultMTU, kMaxTransmissions,
                                                  kTxWindow, NoOpTxCallback, NoOpFailureCallback);
  EXPECT_TRUE(rx_engine);
  EXPECT_TRUE(tx_engine);
}

// This test that TxEngine sends I-Frames whose acknowledgement sequence numbers match the receive
// sequence number in RxEngine. This also tests that peer acknowledgement restarts our outbound data
// that was paused by hitting TxWindow. This mirrors the L2CAP Test Specification L2CAP/ERM/BV-06-C.
TEST_F(L2CAP_EnhancedRetransmissionModeEnginesTest,
       OutboundInformationFramesAcknowledgeReceivedInformationFrames) {
  int tx_count = 0;
  auto tx_callback = [&tx_count](ByteBufferPtr pdu) {
    ASSERT_TRUE(pdu);
    // Unlike the Test Spec's sequence diagram, we respond to the peer's I-Frame with a Receiver
    // Ready (which is always allowed), so our subsequent I-Frame is actually the third outbound.
    if (tx_count == 0) {
      ASSERT_LE(sizeof(SimpleInformationFrameHeader), pdu->size());
      ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_information_frame());
      const auto& header = pdu->As<SimpleInformationFrameHeader>();
      EXPECT_EQ(0, header.tx_seq());
      EXPECT_EQ(0, header.receive_seq_num());
    } else if (tx_count == 2) {
      ASSERT_LE(sizeof(SimpleInformationFrameHeader), pdu->size());
      ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_information_frame());
      const auto& header = pdu->As<SimpleInformationFrameHeader>();
      EXPECT_EQ(1, header.tx_seq());

      // Acknowledges the I-Frame from the peer.
      EXPECT_EQ(1, header.receive_seq_num());
    }
    tx_count++;
  };
  auto [rx_engine, tx_engine] = MakeLinkedEnhancedRetransmissionModeEngines(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, /*n_frames_in_tx_window=*/1, tx_callback,
      NoOpFailureCallback);
  ASSERT_TRUE(rx_engine);
  ASSERT_TRUE(tx_engine);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer{'p', 'i', 'n', 'g'}));
  RunLoopUntilIdle();
  EXPECT_EQ(1, tx_count);

  // Receive an I-frame containing an acknowledgment including the frame that we transmitted.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, I-Frame Enhanced Control Field.
  auto info_frame = StaticByteBuffer(0, 1, 'p', 'o', 'n', 'g');
  EXPECT_TRUE(rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, info_frame, FrameCheckSequenceOption::kIncludeFcs)));
  RunLoopUntilIdle();
  EXPECT_EQ(2, tx_count);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer{'b', 'y', 'e', 'e'}));
  RunLoopUntilIdle();
  EXPECT_EQ(3, tx_count);
}

// This tests the integration of receiving Reject frames with triggering retransmission of requested
// unacknowledged I-Frames. This mirrors the L2CAP Test Specification v5.0.2 L2CAP/ERM/BV-13-C.
TEST_F(L2CAP_EnhancedRetransmissionModeEnginesTest, RetransmitAfterReceivingRejectSFrame) {
  int tx_count = 0;
  int iframe_0_tx_count = 0;
  int iframe_1_tx_count = 0;
  auto tx_callback = [&tx_count, &iframe_0_tx_count, &iframe_1_tx_count](ByteBufferPtr pdu) {
    tx_count++;
    ASSERT_LE(sizeof(SimpleInformationFrameHeader), pdu->size());
    ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_information_frame());
    const auto& header = pdu->As<SimpleInformationFrameHeader>();
    if (header.tx_seq() == 0) {
      iframe_0_tx_count++;
    } else if (header.tx_seq() == 1) {
      iframe_1_tx_count++;
    }
  };
  auto [rx_engine, tx_engine] = MakeLinkedEnhancedRetransmissionModeEngines(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback, NoOpFailureCallback);
  ASSERT_TRUE(rx_engine);
  ASSERT_TRUE(tx_engine);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('a')));
  EXPECT_EQ(1, tx_count);
  EXPECT_EQ(1, iframe_0_tx_count);
  EXPECT_EQ(0, iframe_1_tx_count);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('b')));
  EXPECT_EQ(2, tx_count);
  EXPECT_EQ(1, iframe_0_tx_count);
  EXPECT_EQ(1, iframe_1_tx_count);

  // Receive an S-frame containing a retransmission request starting at seq = 0. See Core Spec v5.0,
  // Vol 3, Part A, Section 3.3.2 S-Frame Enhanced Control Field.
  auto reject = StaticByteBuffer(0b1 | kExtendedControlRejFunctionMask, 0);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, reject, FrameCheckSequenceOption::kIncludeFcs));
  RunLoopUntilIdle();

  // Check that this caused a retransmission of the two I-Frames.
  EXPECT_EQ(4, tx_count);
  EXPECT_EQ(2, iframe_0_tx_count);
  EXPECT_EQ(2, iframe_1_tx_count);
}

// This tests the integration of receiving an acknowledgment sequence with triggering retransmission
// of unacknowledged I-Frames. This mirrors the L2CAP Test Specification L2CAP/ERM/BV-18-C.
TEST_F(L2CAP_EnhancedRetransmissionModeEnginesTest,
       RetransmitAfterPollResponseDoesNotAcknowledgeSentFrames) {
  DynamicByteBuffer info_frame;
  int tx_count = 0;
  auto tx_callback = [&info_frame, &tx_count](ByteBufferPtr pdu) {
    // The first packet is the I-Frame containing the data that we sent.
    // The second packet is the S-Frame polling for the peer after the Retransmission Timer expires.
    // It is not checked to keep the tests less fragile, as the first two packets are already
    // covered by L2CAP_EnhancedRetransmissionModeTxEngineTest.
    if (tx_count == 0) {
      info_frame = DynamicByteBuffer(*pdu);
    } else if (tx_count == 2) {
      EXPECT_TRUE(ContainersEqual(info_frame, *pdu));
    }
    tx_count++;
  };
  auto [rx_engine, tx_engine] = MakeLinkedEnhancedRetransmissionModeEngines(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback, NoOpFailureCallback);
  ASSERT_TRUE(rx_engine);
  ASSERT_TRUE(tx_engine);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer{'a'}));
  RunLoopUntilIdle();
  EXPECT_EQ(1, tx_count);

  ASSERT_TRUE(RunLoopFor(kErtmReceiverReadyPollTimerDuration));
  EXPECT_EQ(2, tx_count);

  // Receive an S-frame containing an acknowledgment not including the frame that we transmitted. F
  // is set. See Core Spec, v5, Vol 3, Part A, Section 3.3.2, S-Frame Enhanced Control Field.
  auto receiver_ready = StaticByteBuffer(0b1 | kExtendedControlFBitMask, 0);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready, FrameCheckSequenceOption::kIncludeFcs));
  RunLoopUntilIdle();

  // Check that this caused a retransmission of the initial I-Frame.
  EXPECT_EQ(3, tx_count);
}

// This tests that explicitly requested retransmission and receiving a poll response that doesn't
// acknowledge all I-Frames results in only a single set of retransmissions. This mirrors the L2CAP
// Test Specification v5.0.2 L2CAP/ERM/BI-04-C.
TEST_F(L2CAP_EnhancedRetransmissionModeEnginesTest,
       RetransmitAfterReceivingRejectSFrameAndReceiverReadyPollResponseNotAcknowledgingSentFrames) {
  int tx_count = 0;
  int iframe_0_tx_count = 0;
  int iframe_1_tx_count = 0;
  auto tx_callback = [&tx_count, &iframe_0_tx_count, &iframe_1_tx_count](ByteBufferPtr pdu) {
    tx_count++;

    // After outbound I-Frames, expect an outbound poll request S-Frame.
    if (tx_count == 3) {
      ASSERT_EQ(sizeof(SimpleSupervisoryFrame), pdu->size());
      ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_supervisory_frame());
      ASSERT_TRUE(pdu->As<SimpleSupervisoryFrame>().is_poll_request());
      return;
    }

    ASSERT_LE(sizeof(SimpleInformationFrameHeader), pdu->size());
    ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_information_frame());
    const auto& header = pdu->As<SimpleInformationFrameHeader>();
    if (header.tx_seq() == 0) {
      iframe_0_tx_count++;
    } else if (header.tx_seq() == 1) {
      iframe_1_tx_count++;
    }
  };
  auto [rx_engine, tx_engine] = MakeLinkedEnhancedRetransmissionModeEngines(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback, NoOpFailureCallback);
  ASSERT_TRUE(rx_engine);
  ASSERT_TRUE(tx_engine);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('a')));
  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('b')));
  EXPECT_EQ(2, tx_count);
  EXPECT_EQ(1, iframe_0_tx_count);
  EXPECT_EQ(1, iframe_1_tx_count);

  ASSERT_TRUE(RunLoopFor(kErtmReceiverReadyPollTimerDuration));
  EXPECT_EQ(3, tx_count);

  // Receive an S-frame containing a retransmission request starting at seq = 0. See Core Spec v5.0,
  // Vol 3, Part A, Section 3.3.2 S-Frame Enhanced Control Field.
  auto reject = StaticByteBuffer(0b1 | kExtendedControlRejFunctionMask, 0);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, reject, FrameCheckSequenceOption::kIncludeFcs));

  // Receive an S-frame containing an acknowledgment not including the frames that we transmitted. F
  // is set. See Core Spec v5.0, Vol 3, Part A, Section 3.3.2 S-Frame Enhanced Control Field.
  auto receiver_ready_0 = StaticByteBuffer(0b1 | kExtendedControlFBitMask, 0);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready_0, FrameCheckSequenceOption::kIncludeFcs));

  // Check that this caused a retransmission of the two I-Frames only once each.
  EXPECT_EQ(5, tx_count);
  EXPECT_EQ(2, iframe_0_tx_count);
  EXPECT_EQ(2, iframe_1_tx_count);

  // Acknowledge all of the outbound I-Frames per the specification's sequence diagram.
  auto receiver_ready_2 = StaticByteBuffer(0b1, 2);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready_2, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_EQ(5, tx_count);
}

// This tests that explicitly requested retransmission and receiving a I-Frame that doesn't
// acknowledge all I-Frames results in only a single set of retransmissions. This mirrors the L2CAP
// Test Specification v5.0.2 L2CAP/ERM/BI-05-C in which we—the IUT—perform the ALT1 path in the
// sequence diagram Figure 4.96.
TEST_F(L2CAP_EnhancedRetransmissionModeEnginesTest,
       RetransmitAfterReceivingRejectSFrameAndIFramePollResponseNotAcknowledgingSentFrames) {
  int tx_count = 0;
  int iframe_0_tx_count = 0;
  int iframe_1_tx_count = 0;
  auto tx_callback = [&tx_count, &iframe_0_tx_count, &iframe_1_tx_count](ByteBufferPtr pdu) {
    tx_count++;
    SCOPED_TRACE(tx_count);

    if (tx_count == 3) {
      // After outbound I-Frames (not retransmitted), expect an outbound poll request S-Frame.
      ASSERT_EQ(sizeof(SimpleSupervisoryFrame), pdu->size());
      ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_supervisory_frame());
      EXPECT_TRUE(pdu->As<SimpleSupervisoryFrame>().is_poll_request());
      return;
    } else if (tx_count == 6) {
      // After retransmitted I-Frames, expect an outbound ReceiverReady that acknowledges the peer's
      // I-Frame.
      ASSERT_EQ(sizeof(SimpleSupervisoryFrame), pdu->size());
      ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_supervisory_frame());
      EXPECT_FALSE(pdu->As<SimpleSupervisoryFrame>().is_poll_request());
      EXPECT_EQ(1, pdu->As<SimpleSupervisoryFrame>().receive_seq_num());
      return;
    }

    ASSERT_LE(sizeof(SimpleInformationFrameHeader), pdu->size());
    ASSERT_TRUE(pdu->As<EnhancedControlField>().designates_information_frame());
    const auto& header = pdu->As<SimpleInformationFrameHeader>();
    EXPECT_EQ(0, header.receive_seq_num());
    if (header.tx_seq() == 0) {
      iframe_0_tx_count++;
    } else if (header.tx_seq() == 1) {
      iframe_1_tx_count++;
    }
  };
  auto [rx_engine, tx_engine] = MakeLinkedEnhancedRetransmissionModeEngines(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback, NoOpFailureCallback);
  ASSERT_TRUE(rx_engine);
  ASSERT_TRUE(tx_engine);

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('a')));
  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('b')));
  EXPECT_EQ(2, tx_count);
  EXPECT_EQ(1, iframe_0_tx_count);
  EXPECT_EQ(1, iframe_1_tx_count);

  ASSERT_TRUE(RunLoopFor(kErtmReceiverReadyPollTimerDuration));
  EXPECT_EQ(3, tx_count);

  // Receive an S-frame containing a retransmission request starting at seq = 0. See Core Spec v5.0,
  // Vol 3, Part A, Section 3.3.2 S-Frame Enhanced Control Field.
  auto reject = StaticByteBuffer(0b1 | kExtendedControlRejFunctionMask, 0);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, reject, FrameCheckSequenceOption::kIncludeFcs));

  // Receive an I-frame containing an acknowledgment not including the frames that we transmitted. F
  // is set. See Core Spec v5.0, Vol 3, Part A, Section 3.3.2 I-Frame Enhanced Control Field.
  auto inbound_iframe = StaticByteBuffer(kExtendedControlFBitMask, 0, 'A');
  auto inbound_pdu = rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, inbound_iframe, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_TRUE(inbound_pdu);

  // Check that this caused a retransmission of the two I-Frames only once each, plus our
  // acknowledgment of the peer I-Frame.
  EXPECT_EQ(6, tx_count);
  EXPECT_EQ(2, iframe_0_tx_count);
  EXPECT_EQ(2, iframe_1_tx_count);

  // Acknowledge all of the outbound I-Frames per the specification's sequence diagram.
  auto receiver_ready = StaticByteBuffer(0b1, 2);
  rx_engine->ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_EQ(6, tx_count);
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
