// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enhanced_retransmission_mode_tx_engine.h"

#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/frame_headers.h"

namespace bt::l2cap::internal {
namespace {

constexpr ChannelId kTestChannelId = 0x0001;

using TxEngine = EnhancedRetransmissionModeTxEngine;

class EnhancedRetransmissionModeTxEngineTest : public ::gtest::TestLoopFixture {
 public:
  EnhancedRetransmissionModeTxEngineTest() : kDefaultPayload('h', 'e', 'l', 'l', 'o') {}

 protected:
  // The default values are provided for use by tests which don't depend on the
  // specific value a given parameter. This should make the tests easier to
  // read, because the reader can focus on only the non-defaulted parameter
  // values.
  static constexpr auto kDefaultMTU = bt::l2cap::kDefaultMTU;
  static constexpr size_t kDefaultMaxTransmissions = 1;
  static constexpr size_t kDefaultTxWindow = 63;

  const StaticByteBuffer<5> kDefaultPayload;

  void VerifyIsReceiverReadyPollFrame(ByteBuffer* buf) {
    ASSERT_TRUE(buf);
    ASSERT_EQ(sizeof(SimpleSupervisoryFrame), buf->size());

    const auto sframe = buf->To<SimpleSupervisoryFrame>();
    EXPECT_EQ(SupervisoryFunction::ReceiverReady, sframe.function());
    EXPECT_TRUE(sframe.is_poll_request());
  }
};

void NoOpTxCallback(ByteBufferPtr) {}
void NoOpFailureCallback() {}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduTransmitsMinimalSizedSdu) {
  ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };

  constexpr size_t kMtu = 10;
  const auto payload = CreateStaticByteBuffer(1);
  TxEngine(kTestChannelId, kMtu, kDefaultMaxTransmissions, kDefaultTxWindow, tx_callback,
           NoOpFailureCallback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);

  // See Core Spec v5.0, Volume 3, Part A, Table 3.2.
  const auto expected_pdu = CreateStaticByteBuffer(0,   // Final Bit, TxSeq, MustBeZeroBit
                                                   0,   // SAR bits, ReqSeq
                                                   1);  // Payload
  EXPECT_TRUE(ContainersEqual(expected_pdu, *last_pdu));
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduTransmitsMaximalSizedSdu) {
  ByteBufferPtr last_pdu;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };

  constexpr size_t kMtu = 1;
  const auto payload = CreateStaticByteBuffer(1);
  TxEngine(kTestChannelId, kMtu, kDefaultMaxTransmissions, kDefaultTxWindow, tx_callback,
           NoOpFailureCallback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  EXPECT_EQ(1u, n_pdus);
  ASSERT_TRUE(last_pdu);

  // See Core Spec v5.0, Volume 3, Part A, Table 3.2.
  const auto expected_pdu = CreateStaticByteBuffer(0,   // Final Bit, TxSeq, MustBeZeroBit
                                                   0,   // SAR bits, ReqSeq
                                                   1);  // Payload
  EXPECT_TRUE(ContainersEqual(expected_pdu, *last_pdu));
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduSurvivesOversizedSdu) {
  // TODO(fxbug.dev/1033): Update this test when we add support for segmentation.
  constexpr size_t kMtu = 1;
  TxEngine(kTestChannelId, kMtu, kDefaultMaxTransmissions, kDefaultTxWindow, NoOpTxCallback,
           NoOpFailureCallback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>(CreateStaticByteBuffer(1, 2)));
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduSurvivesZeroByteSdu) {
  TxEngine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow, NoOpTxCallback,
           NoOpFailureCallback)
      .QueueSdu(std::make_unique<DynamicByteBuffer>());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduAdvancesSequenceNumber) {
  const auto payload = CreateStaticByteBuffer(1);
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  {
    // See Core Spec v5.0, Volume 3, Part A, Table 3.2.
    const auto expected_pdu = CreateStaticByteBuffer(0,   // Final Bit, TxSeq, MustBeZeroBit
                                                     0,   // SAR bits, ReqSeq
                                                     1);  // Payload

    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
    ASSERT_TRUE(last_pdu);
    EXPECT_TRUE(ContainersEqual(expected_pdu, *last_pdu));
  }

  {
    // See Core Spec v5.0, Volume 3, Part A, Table 3.2.
    const auto expected_pdu = CreateStaticByteBuffer(1 << 1,  // Final Bit, TxSeq=1, MustBeZeroBit
                                                     0,       // SAR bits, ReqSeq
                                                     1);      // Payload
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
    ASSERT_TRUE(last_pdu);
    EXPECT_TRUE(ContainersEqual(expected_pdu, *last_pdu));
  }

  {
    // See Core Spec v5.0, Volume 3, Part A, Table 3.2.
    const auto expected_pdu = CreateStaticByteBuffer(2 << 1,  // Final Bit, TxSeq=2, MustBeZeroBit
                                                     0,       // SAR bits, ReqSeq
                                                     1);      // Payload
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
    ASSERT_TRUE(last_pdu);
    EXPECT_TRUE(ContainersEqual(expected_pdu, *last_pdu));
  }
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduRollsOverSequenceNumber) {
  constexpr size_t kTxWindow = 63;  // Max possible value
  const auto payload = CreateStaticByteBuffer(1);
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  constexpr size_t kMaxSeq = 64;
  for (size_t i = 0; i < kMaxSeq; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
    tx_engine.UpdateAckSeq((i + 1) % kMaxSeq, /*is_poll_response=*/false);
  }

  // See Core Spec v5.0, Volume 3, Part A, Table 3.2.
  const auto expected_pdu =
      CreateStaticByteBuffer(0,   // Final Bit, TxSeq (rolls over from 63 to 0), MustBeZeroBit
                             0,   // SAR bits, ReqSeq
                             1);  // Payload
  last_pdu = nullptr;
  // Free up space for more transmissions. We need room for the 64th frame from
  // above (since the TxWindow is 63), and the new 0th frame. Hence we
  // acknowledge original frames 0 and 1.
  tx_engine.UpdateAckSeq(2, /*is_poll_response=*/false);
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(payload));
  ASSERT_TRUE(last_pdu);
  EXPECT_TRUE(ContainersEqual(expected_pdu, *last_pdu));
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduDoesNotTransmitBeyondTxWindow) {
  constexpr size_t kTxWindow = 1;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(1u, n_pdus);

  n_pdus = 0;
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  EXPECT_EQ(0u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       QueueSduDoesNotTransmitBeyondTxWindowEvenIfQueueWrapsSequenceNumbers) {
  constexpr size_t kTxWindow = 1;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(1u, n_pdus);

  constexpr size_t kMaxSeq = 64;
  n_pdus = 0;
  for (size_t i = 0; i < kMaxSeq; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
    ASSERT_EQ(0u, n_pdus);
  }
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, EngineTransmitsReceiverReadyPollAfterTimeout) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  SCOPED_TRACE("");
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineTransmitsReceiverReadyPollOnlyOnceAfterTimeout) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  SCOPED_TRACE("");
  RETURN_IF_FATAL(VerifyIsReceiverReadyPollFrame(last_pdu.get()));
  last_pdu = nullptr;

  // Note: This value is chosen to be at least as long as
  // kReceiverReadyPollTimerDuration, but shorter than kMonitorTimerDuration.
  EXPECT_FALSE(RunLoopFor(zx::sec(2)));  // No tasks were run.
  EXPECT_FALSE(last_pdu);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineAdvancesReceiverReadyPollTimeoutOnNewTransmission) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  ASSERT_FALSE(RunLoopFor(zx::sec(1)));  // No events should fire.
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  last_pdu = nullptr;

  ASSERT_FALSE(RunLoopFor(zx::sec(1)));  // Original timeout should not fire.
  ASSERT_TRUE(RunLoopFor(zx::sec(1)));   // New timeout should fire.
  SCOPED_TRACE("");
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, ReceiverReadyPollIncludesRequestSequenceNumber) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  tx_engine.UpdateReqSeq(1);
  RunLoopUntilIdle();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  last_pdu = nullptr;

  SCOPED_TRACE("");
  EXPECT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_NO_FATAL_FAILURE(VerifyIsReceiverReadyPollFrame(last_pdu.get()));
  EXPECT_EQ(1u, last_pdu->To<SimpleSupervisoryFrame>().receive_seq_num());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       AckOfOnlyOutstandingFrameCancelsReceiverReadyPollTimeout) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();

  EXPECT_FALSE(RunLoopFor(zx::sec(2)));  // No tasks were run.
  EXPECT_FALSE(last_pdu);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       AckOfAllOutstandingFramesCancelsReceiverReadyPollTimeout) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  tx_engine.UpdateAckSeq(3, /*is_poll_response=*/false);
  RunLoopUntilIdle();

  EXPECT_FALSE(RunLoopFor(zx::sec(2)));  // No tasks were run.
  EXPECT_FALSE(last_pdu);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, PartialAckDoesNotCancelReceiverReadyPollTimeout) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();

  // See Core Spec v5.0, Volume 3, Part A, Sec 8.6.5.6, under heading
  // Process-ReqSeq. We should only Stop-RetransTimer if UnackedFrames is 0.
  SCOPED_TRACE("");
  EXPECT_TRUE(RunLoopFor(zx::sec(2)));
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       NewTransmissionAfterAckedFrameReArmsReceiverReadyPollTimeout) {
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  // Send a frame, and get the ACK.
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();

  // Send a new frame.
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  last_pdu = nullptr;

  // Having earlier received an ACK for the previous frame should not have left
  // around any state that would prevent us from sending a receiver-ready poll
  // for the second frame.
  SCOPED_TRACE("");
  EXPECT_TRUE(RunLoopFor(zx::sec(2)));
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineRetransmitsReceiverReadyPollAfterMonitorTimeout) {
  constexpr size_t kMaxTransmissions = 2;  // Allow retransmission
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // First the receiver_ready_poll_task_ fires.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  // Then the monitor_task_ fires.
  EXPECT_TRUE(RunLoopFor(zx::sec(12)));
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineDoesNotRetransmitReceiverReadyPollAfterMonitorTimeoutWhenRetransmissionsAreDisabled) {
  constexpr size_t kMaxTransmissions = 1;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // First the receiver_ready_poll_task_ fires.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_TRUE(last_pdu);
  last_pdu = nullptr;

  // Run the event loop long enough for the monitor task to fire again. Because
  // kMaxTransmissions == 1, the ReceiverReadyPoll should not be retransmitted.
  RunLoopFor(zx::sec(13));
  EXPECT_FALSE(last_pdu);
}

// See Core Spec v5.0, Volume 3, Part A, Sec 5.4, Table 8.6.5.8, for the row
// with "Recv ReqSeqAndFbit" and "F = 1".
TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineStopsPollingReceiverReadyFromMonitorTaskAfterReceivingFinalUpdateForAckSeq) {
  constexpr size_t kMaxTransmissions = 3;  // Allow multiple retransmissions
  ByteBufferPtr last_pdu;
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));   // receiver_ready_poll_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/true);
  EXPECT_FALSE(RunLoopFor(zx::sec(13)));  // No other tasks.
}

// See Core Spec v5.0, Volume 3, Part A, Sec 5.4, Table 8.6.5.8, for the row
// with "Recv ReqSeqAndFbit" and "F = 0".
TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineContinuesPollingReceiverReadyFromMonitorTaskAfterReceivingNonFinalUpdateForAckSeq) {
  constexpr size_t kMaxTransmissions = 2;  // Allow retransmissions
  ByteBufferPtr last_pdu;
  TxEngine tx_engine(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
      [&](auto pdu) { last_pdu = std::move(pdu); }, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));   // receiver_ready_poll_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  EXPECT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineRetransmitsReceiverReadyPollAfterMultipleMonitorTimeouts) {
  constexpr size_t kMaxTransmissions = 3;  // Allow multiple retransmissions
  ByteBufferPtr last_pdu;
  TxEngine tx_engine(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
      [&](auto pdu) { last_pdu = std::move(pdu); }, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));   // receiver_ready_poll_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  ASSERT_FALSE(RunLoopFor(zx::sec(2)));  // RR-poll task does _not_ fire
  last_pdu = nullptr;

  EXPECT_TRUE(RunLoopFor(zx::sec(10)));  // monitor_task_ again
  VerifyIsReceiverReadyPollFrame(last_pdu.get());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineRetransmitsReceiverReadyPollIndefinitelyAfterMonitorTimeoutWhenMaxTransmitsIsZero) {
  constexpr size_t kMaxTransmissions = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // First the receiver_ready_poll_task_ fires.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  EXPECT_TRUE(last_pdu);
  last_pdu = nullptr;

  // Then the monitor_task_ fires.
  EXPECT_TRUE(RunLoopFor(zx::sec(12)));
  EXPECT_TRUE(last_pdu);
  last_pdu = nullptr;

  // And the monitor_task_ fires again.
  EXPECT_TRUE(RunLoopFor(zx::sec(12)));
  EXPECT_TRUE(last_pdu);
  last_pdu = nullptr;

  // And the monitor_task_ fires yet again.
  EXPECT_TRUE(RunLoopFor(zx::sec(12)));
  EXPECT_TRUE(last_pdu);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineStopsTransmittingReceiverReadyPollAfterMaxTransmits) {
  constexpr size_t kMaxTransmissions = 2;
  ByteBufferPtr last_pdu;
  TxEngine tx_engine(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
      [&](auto pdu) { last_pdu = std::move(pdu); }, NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));   // receiver_ready_poll_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  last_pdu = nullptr;

  EXPECT_FALSE(RunLoopFor(zx::sec(13)));
  EXPECT_FALSE(last_pdu);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineClosesChannelAfterMaxTransmitsOfReceiverReadyPoll) {
  constexpr size_t kMaxTransmissions = 2;
  bool connection_failed = false;
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));   // receiver_ready_poll_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineClosesChannelAfterMaxTransmitsOfReceiverReadyPollEvenIfRetransmissionsAreDisabled) {
  constexpr size_t kMaxTransmissions = 1;
  bool connection_failed = false;
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));   // receiver_ready_poll_task_
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));  // monitor_task_
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, EngineClosesChannelAfterMaxTransmitsOfIFrame) {
  constexpr size_t kMaxTransmissions = 2;
  size_t num_info_frames_sent = 0;
  bool connection_failed = false;
  TxEngine tx_engine(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
      [&](ByteBufferPtr pdu) {
        if (pdu->size() >= sizeof(EnhancedControlField) &&
            pdu->To<EnhancedControlField>().designates_information_frame()) {
          ++num_info_frames_sent;
        }
      },
      [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(1u, num_info_frames_sent);

  // Not having received an acknowledgement after 2 seconds,
  // receiver_ready_poll_task_ will fire, and cause us to send a
  // ReceiverReadyPoll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // The peer indicates that it has not received any frames. This causes us to
  // retransmit the frame.
  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  EXPECT_EQ(2u, num_info_frames_sent);

  // Not having received an acknowledgement after 2 seconds,
  // receiver_ready_poll_task_ will fire, and cause us to send another
  // ReceiverReadyPoll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // The connection should remain open, to allow the peer time to respond to our
  // poll, and acknowledge the outstanding frame.
  EXPECT_FALSE(connection_failed);

  // The peer again indicates that it has not received any frames.
  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);

  // Because we've exhausted kMaxTransmissions, the connection will be closed.
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineExhaustsAllRetransmissionsOfIFrameBeforeClosingChannel) {
  constexpr size_t kMaxTransmissions = 255;
  size_t num_info_frames_sent = 0;
  bool connection_failed = false;
  TxEngine tx_engine(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
      [&](ByteBufferPtr pdu) {
        if (pdu->size() >= sizeof(EnhancedControlField) &&
            pdu->To<EnhancedControlField>().designates_information_frame()) {
          ++num_info_frames_sent;
        }
      },
      [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  for (size_t i = 0; i < kMaxTransmissions; ++i) {
    // Not having received an acknowledgement after 2 seconds,
    // receiver_ready_poll_task_ will fire, and cause us to send a
    // ReceiverReadyPoll.
    ASSERT_TRUE(RunLoopFor(zx::sec(2))) << "(i=" << i << ")";

    // The connection should remain open, to allow the peer time to respond to
    // our poll, and acknowledge the outstanding frame.
    EXPECT_FALSE(connection_failed);

    // The peer indicates that it has not received any frames.
    tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  }

  // The connection is closed, and we've exhausted kMaxTransmissions.
  EXPECT_TRUE(connection_failed);
  EXPECT_EQ(255u, num_info_frames_sent);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineRetransmitsIFrameIndefinitelyWhenMaxTransmitsIsZero) {
  constexpr size_t kMaxTransmissions = 0;
  constexpr size_t kTxWindow = 2;
  size_t num_info_frames_sent = 0;
  bool connection_failed = false;
  ByteBufferPtr last_tx_frame = nullptr;
  TxEngine tx_engine(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow,
      [&](ByteBufferPtr pdu) {
        if (pdu->size() >= sizeof(EnhancedControlField) &&
            pdu->To<EnhancedControlField>().designates_information_frame()) {
          ++num_info_frames_sent;
          last_tx_frame = std::move(pdu);
        }
      },
      [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  EXPECT_EQ(1u, num_info_frames_sent);

  // This test should trigger enough transmissions to overflow an 8-bit transmit counter.
  const auto kNumTransmissions = size_t{2} * std::numeric_limits<uint8_t>::max();

  // Retransmissions after the initial one triggered by QueueSdu
  for (size_t i = 0; i < kNumTransmissions - 1; ++i) {
    // Not having received an acknowledgement after 2 seconds, receiver_ready_poll_task_ will fire,
    // and cause us to send a ReceiverReadyPoll.
    ASSERT_TRUE(RunLoopFor(zx::sec(2))) << "(i=" << i << ")";

    // The connection should remain open, to allow the peer time to respond to our poll, and
    // acknowledge the outstanding frame.
    EXPECT_FALSE(connection_failed);

    // The peer indicates that it has not received any frames.
    tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  }

  // The connection is still open and we have transmitted more times than the greatest possible
  // finite MaxTransmit option (which is 8-bit).
  EXPECT_FALSE(connection_failed);
  EXPECT_EQ(kNumTransmissions, num_info_frames_sent);

  // Check that the outbound frame's transmit count hasn't overflowed to zero by transmitting a
  // different payload.
  ASSERT_TRUE(ContainersEqual(kDefaultPayload, last_tx_frame->view(sizeof(EnhancedControlField))));
  num_info_frames_sent = 0;

  // If the first frame's transmit count had overflowed to zero, it would get sent out together
  // with this new SDU because it would be indistinguishable from "never transmitted."
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(StaticByteBuffer('@')));
  EXPECT_EQ(1u, num_info_frames_sent);
  EXPECT_EQ('@', (*last_tx_frame)[sizeof(EnhancedControlField)]);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineClosesChannelAfterMaxTransmitsOfIFrameEvenIfRetransmissionsAreDisabled) {
  constexpr size_t kMaxTransmissions = 1;
  bool connection_failed = false;
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // Not having received an acknowledgement after 2 seconds,
  // receiver_ready_poll_task_ will fire, and cause us to send a
  // ReceiverReadyPoll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // The connection should remain open, to allow the peer time to respond to our
  // poll, and acknowledge the outstanding frame.
  EXPECT_FALSE(connection_failed);

  // The peer indicates that it has not received any frames.
  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, EngineRetransmitsMissingFrameOnPollResponse) {
  constexpr size_t kMaxTransmissions = 2;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) { last_pdu = std::move(pdu); };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  last_pdu = nullptr;

  ASSERT_TRUE(RunLoopFor(zx::sec(2)));  // receiver_ready_poll_task_
  last_pdu = nullptr;

  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  ASSERT_TRUE(last_pdu->To<EnhancedControlField>().designates_information_frame());
  EXPECT_EQ(0u, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, EngineRetransmitsAllMissingFramesOnPollResponse) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 63;
  size_t n_pdus = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  // Send a TxWindow's worth of frames.
  for (size_t i = 0; i < kTxWindow; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, and clear out accumulated callback
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  n_pdus = 0;
  last_pdu = nullptr;

  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  EXPECT_EQ(kTxWindow, n_pdus);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  ASSERT_TRUE(last_pdu->To<EnhancedControlField>().designates_information_frame());
  EXPECT_EQ(kTxWindow - 1, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineRetransmitsAllMissingFramesOnPollResponseWithWrappedSequenceNumber) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 63;
  size_t n_pdus = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  // Send a TxWindow's worth of frames.
  for (size_t i = 0; i < kTxWindow; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }

  // Acknowledge the first 32 of these frames (with sequence numbers 0...31).
  tx_engine.UpdateAckSeq(32, /*is_poll_response=*/false);

  // Queue 32 new frames (with sequence numbers 63, 0 ... 30).
  for (size_t i = 0; i < 32; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, and then clear out accumulated callback
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  n_pdus = 0;
  last_pdu = nullptr;

  // Repeat the earlier acknowledgement. This indicates that the peer has
  // not received frame 32.
  tx_engine.UpdateAckSeq(32, /*is_poll_response=*/true);

  // We expect to retransmit frames 32...63, and then 0...30. That's 63 frames
  // in total.
  EXPECT_EQ(63u, n_pdus);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  ASSERT_TRUE(last_pdu->To<EnhancedControlField>().designates_information_frame());
  EXPECT_EQ(30u, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineProperlyHandlesPartialAckWithWrappedSequenceNumber) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 63;
  size_t n_pdus = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  // Send a TxWindow's worth of frames.
  for (size_t i = 0; i < kTxWindow; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }

  // Acknowledge the first 62 of these frames (with sequence numbers 0...61).
  tx_engine.UpdateAckSeq(62, /*is_poll_response=*/false);

  // Queue 62 new frames. These frames have sequence numebrs 63, 0...60.
  // (Sequence number 62 was used when we queued the first batch of frames
  // above.)
  for (size_t i = 0; i < 62; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, and then clear out accumulated callback
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  n_pdus = 0;
  last_pdu = nullptr;

  // Acknowledge an additional 5 frames (with sequence numbers 62, 63, 0, 1, 2).
  tx_engine.UpdateAckSeq(3, /*is_poll_response=*/true);

  // Verify that all unacknowledged frames are retransmitted.
  EXPECT_EQ(58u, n_pdus);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  ASSERT_TRUE(last_pdu->To<EnhancedControlField>().designates_information_frame());
  EXPECT_EQ(60u, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, EngineDoesNotRetransmitFramesBeyondTxWindow) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 32;
  size_t n_pdus = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  // Queue two TxWindow's worth of frames. These have sequence numbers 0...63.
  for (size_t i = 0; i < 2 * kTxWindow; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, and clear out accumulated callback
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  n_pdus = 0;
  last_pdu = nullptr;

  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  EXPECT_EQ(kTxWindow, n_pdus);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  EXPECT_EQ(kTxWindow - 1, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineDoesNotRetransmitFramesBeyondTxWindowWhenWindowWraps) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 48;
  size_t n_pdus = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  // Queue one TxWindow's worth of frames. This advances the sequence numbers,
  // so that further transmissions can wrap.
  for (size_t i = 0; i < 48; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  tx_engine.UpdateAckSeq(48, /*is_poll_response=*/false);
  RunLoopUntilIdle();

  // Queue another TxWindow's worth of frames. These have sequence
  // numbers 48..63, and 0..31. These _should_ be retransmitted at the next
  // UpdateAckSeq() call.
  for (size_t i = 0; i < 48; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();

  // Queue a few more frames, with sequence numbers 32..39. These should _not_
  // be retransmitted at the next UpdateAckSeq() call.
  for (size_t i = 0; i < 8; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, and clear out accumulated callback
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  n_pdus = 0;
  last_pdu = nullptr;

  // Report that the peer has not received frame 48. This should trigger
  // retranmissions of unacknowledged frames within the TxWindow.
  tx_engine.UpdateAckSeq(48, /*is_poll_response=*/true);

  // We expect to retransmit frames 48..63 and 0..31. The other frames are
  // beyond the transmit window.
  EXPECT_EQ(48u, n_pdus);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  ASSERT_TRUE(last_pdu->To<EnhancedControlField>().designates_information_frame());
  EXPECT_EQ(31u, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineDoesNotRetransmitPreviouslyAckedFramesOnPollResponse) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 2;
  size_t n_pdus = 0;
  ByteBufferPtr last_pdu;
  auto tx_callback = [&](auto pdu) {
    ++n_pdus;
    last_pdu = std::move(pdu);
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, and clear out accumulated callback
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  n_pdus = 0;
  last_pdu = nullptr;

  constexpr size_t kPollResponseReqSeq = 1;
  tx_engine.UpdateAckSeq(kPollResponseReqSeq, /*is_poll_response=*/true);
  EXPECT_EQ(kTxWindow - kPollResponseReqSeq, n_pdus);
  ASSERT_TRUE(last_pdu);
  ASSERT_GE(last_pdu->size(), sizeof(SimpleInformationFrameHeader));
  ASSERT_TRUE(last_pdu->To<EnhancedControlField>().designates_information_frame());
  EXPECT_EQ(1, last_pdu->To<SimpleInformationFrameHeader>().tx_seq());
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, AckOfFrameWithNoneOutstandingClosesChannel) {
  bool connection_failed = false;
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, [&] { connection_failed = true; });

  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, AckOfMoreFramesThanAreOutstandingClosesChannel) {
  bool connection_failed = false;
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, [&] { connection_failed = true; });

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.UpdateAckSeq(2, /*is_poll_response=*/true);
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, EngineDoesNotCrashOnSpuriousAckAfterValidAck) {
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, NoOpFailureCallback);
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/true);
  tx_engine.UpdateAckSeq(2, /*is_poll_response=*/true);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineDoesNotCrashOnSpuriousAckBeforeAnyDataHasBeenSent) {
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, NoOpFailureCallback);
  for (size_t i = 0; i <= EnhancedControlField::kMaxSeqNum; ++i) {
    tx_engine.UpdateAckSeq(i, /*is_poll_response=*/true);
  }
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduDoesNotTransmitFramesWhenRemoteIsBusy) {
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.SetRemoteBusy();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  EXPECT_EQ(0u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, UpdateAckSeqTransmitsQueuedDataWhenPossible) {
  constexpr size_t kTxWindow = 1;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(1u, n_pdus);

  n_pdus = 0;
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       UpdateAckSeqTransmissionOfQueuedDataRespectsTxWindow) {
  constexpr size_t kTxWindow = 1;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(1u, n_pdus);

  n_pdus = 0;
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       NonFinalUpdateAckSeqDoesNotTransmitQueuedFramesWhenRemoteIsBusy) {
  constexpr size_t kTxWindow = 1;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(1u, n_pdus);

  n_pdus = 0;
  tx_engine.SetRemoteBusy();
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       FinalUpdateAckSeqDoesNotTransmitQueuedFramesWhenRemoteIsBusy) {
  constexpr size_t kTxWindow = 1;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(1u, n_pdus);

  n_pdus = 0;
  tx_engine.SetRemoteBusy();
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/true);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       MaybeSendQueuedDataTransmitsAllQueuedFramesWithinTxWindow) {
  constexpr size_t kTxWindow = 63;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.SetRemoteBusy();
  for (size_t i = 0; i < kTxWindow; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();
  ASSERT_EQ(0u, n_pdus);

  tx_engine.ClearRemoteBusy();
  tx_engine.MaybeSendQueuedData();
  RunLoopUntilIdle();
  EXPECT_EQ(kTxWindow, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, MaybeSendQueuedDataDoesNotTransmitBeyondTxWindow) {
  constexpr size_t kTxWindow = 32;
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.SetRemoteBusy();
  for (size_t i = 0; i < kTxWindow + 1; ++i) {
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }
  RunLoopUntilIdle();
  ASSERT_EQ(0u, n_pdus);

  tx_engine.ClearRemoteBusy();
  tx_engine.MaybeSendQueuedData();
  RunLoopUntilIdle();
  EXPECT_EQ(kTxWindow, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, MaybeSendQueuedDataRespectsRemoteBusy) {
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.SetRemoteBusy();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(0u, n_pdus);

  tx_engine.MaybeSendQueuedData();
  RunLoopUntilIdle();
  EXPECT_EQ(0u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       MaybeSendQueuedDataDoesNotCrashWhenCalledWithoutPendingPdus) {
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     NoOpTxCallback, NoOpFailureCallback);
  tx_engine.MaybeSendQueuedData();
  RunLoopUntilIdle();
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduCanSendMoreFramesAfterClearingRemoteBusy) {
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.SetRemoteBusy();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(0u, n_pdus);

  tx_engine.ClearRemoteBusy();
  tx_engine.MaybeSendQueuedData();
  RunLoopUntilIdle();
  ASSERT_EQ(1u, n_pdus);
  n_pdus = 0;

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  EXPECT_EQ(1u, n_pdus);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, QueueSduMaintainsSduOrderingAfterClearRemoteBusy) {
  std::vector<uint8_t> pdu_seq_numbers;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      pdu_seq_numbers.push_back(pdu->To<SimpleInformationFrameHeader>().tx_seq());
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.SetRemoteBusy();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));  // seq=0
  RunLoopUntilIdle();
  ASSERT_TRUE(pdu_seq_numbers.empty());

  tx_engine.ClearRemoteBusy();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));  // seq=1
  RunLoopUntilIdle();

  // This requirement isn't in the specification directly. But it seems
  // necessary given that we can sometimes exit the remote-busy condition
  // without transmitting the queued data. See Core Spec v5.0, Volume 3, Part A,
  // Table 8.7, row for "Recv RR (P=1)" for an example of such an operation.
  EXPECT_EQ((std::vector<uint8_t>{0, 1}), pdu_seq_numbers);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       UpdateAckSeqRetransmitsUnackedFramesBeforeTransmittingQueuedFrames) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 63;
  std::vector<uint8_t> pdu_seq_numbers;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      pdu_seq_numbers.push_back(pdu->To<SimpleInformationFrameHeader>().tx_seq());
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  // Send out two frames.
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_EQ(2u, pdu_seq_numbers.size());
  pdu_seq_numbers.clear();

  // Indicate the remote is busy, and queue a third frame.
  tx_engine.SetRemoteBusy();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  ASSERT_TRUE(pdu_seq_numbers.empty());

  // Clear the busy condition, and then report the new ack sequence number.
  // Because the ack only acknowledges the first frame, the second frame should
  // be retransmitted. And that retransmission should come before the (initial)
  // transmission of the third frame.
  tx_engine.ClearRemoteBusy();
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/true);
  RunLoopUntilIdle();
  EXPECT_EQ((std::vector{uint8_t{1}, uint8_t{2}}), pdu_seq_numbers);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       QueueSduDoesNotTransmitNewFrameWhenEngineIsAwaitingPollResponse) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 3;
  std::vector<uint8_t> pdu_seq_numbers;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      pdu_seq_numbers.push_back(pdu->To<SimpleInformationFrameHeader>().tx_seq());
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire. This moves the engine into the 'WAIT_F'
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Queue a new frame.
  pdu_seq_numbers.clear();
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  EXPECT_EQ(std::vector<uint8_t>(), pdu_seq_numbers);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       NonFinalUpdateAckSeqDoesNotTransmitNewFrameWhenEngineIsAwaitingPollResponse) {
  constexpr size_t kMaxTransmissions = 2;
  constexpr size_t kTxWindow = 1;
  std::vector<uint8_t> pdu_seq_numbers;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      pdu_seq_numbers.push_back(pdu->To<SimpleInformationFrameHeader>().tx_seq());
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire. This moves the engine into the 'WAIT_F'
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Acknowledge the first frame, making room for the transmission of the second
  // frame.
  pdu_seq_numbers.clear();
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/false);
  RunLoopUntilIdle();

  // Because we're still in the WAIT_F state, the second frame should _not_ be
  // transmitted.
  EXPECT_EQ(pdu_seq_numbers.end(), std::find(pdu_seq_numbers.begin(), pdu_seq_numbers.end(), 1));
}

// Note: to make the most of this test, the unit tests should be built with
// ASAN.
TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineDoesNotCrashIfExhaustionOfMaxTransmitForIFrameCausesEngineDestruction) {
  constexpr size_t kMaxTransmissions = 1;
  constexpr size_t kTxWindow = 2;
  bool connection_failed = false;
  std::unique_ptr<TxEngine> tx_engine = std::make_unique<TxEngine>(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kTxWindow, NoOpTxCallback, [&] {
        connection_failed = true;
        tx_engine.reset();
      });

  // Queue three SDUs, of which two should be transmitted immediately.
  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire. This moves the engine into the 'WAIT_F'
  // state.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Acknowledge the first frame, making room for the transmission of the third
  // frame. If the code is buggy, we may encounter use-after-free errors here.
  tx_engine->UpdateAckSeq(1, /*is_poll_response=*/true);
  RunLoopUntilIdle();

  // Because we only allow one transmission, the connection should have failed.
  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       EngineDoesNotCrashIfExhaustionOfMaxTransmitForReceiverReadyPollCausesEngineDestruction) {
  constexpr size_t kMaxTransmissions = 1;
  bool connection_failed = false;
  std::unique_ptr<TxEngine> tx_engine = std::make_unique<TxEngine>(
      kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow, NoOpTxCallback, [&] {
        connection_failed = true;
        tx_engine.reset();
      });

  tx_engine->QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Let monitor_task_ fire, to attempt retransmission of the poll. The
  // retransmission should fail, because we have exhausted kMaxTransmissions. If
  // the code is buggy, we may encounter use-after-free errors here.
  ASSERT_TRUE(RunLoopFor(zx::sec(12)));

  EXPECT_TRUE(connection_failed);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, TransmissionOfPduIncludesRequestSeqNum) {
  uint8_t outbound_req_seq = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      outbound_req_seq = pdu->To<SimpleInformationFrameHeader>().receive_seq_num();
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kDefaultTxWindow,
                     tx_callback, NoOpFailureCallback);

  tx_engine.UpdateReqSeq(5);
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  EXPECT_EQ(5u, outbound_req_seq);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       DeferredTransmissionOfPduIncludesCurrentRequestSeqNum) {
  constexpr size_t kTxWindow = 1;
  uint8_t outbound_req_seq = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      outbound_req_seq = pdu->To<SimpleInformationFrameHeader>().receive_seq_num();
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kDefaultMaxTransmissions, kTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();

  tx_engine.UpdateReqSeq(5);
  tx_engine.UpdateAckSeq(1, /*is_poll_response=*/true);  // Peer acks first PDU.
  RunLoopUntilIdle();

  // The second PDU should have been transmitted with ReqSeq = 5.
  EXPECT_EQ(5u, outbound_req_seq);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, RetransmissionOfPduIncludesCurrentSeqNum) {
  constexpr size_t kMaxTransmissions = 2;
  uint8_t outbound_req_seq = 0;
  size_t n_info_frames = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
      outbound_req_seq = pdu->To<SimpleInformationFrameHeader>().receive_seq_num();
    }
  };
  TxEngine tx_engine(kTestChannelId, kDefaultMTU, kMaxTransmissions, kDefaultTxWindow, tx_callback,
                     NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  RunLoopUntilIdle();
  EXPECT_EQ(0u, outbound_req_seq);

  // The receive engine reports that it has received new data from our peer.
  tx_engine.UpdateReqSeq(10);

  // Let receiver_ready_poll_task_ fire. This triggers us to query if our peer
  // has received our data.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Our peer indicates that it has not received our data.
  tx_engine.UpdateAckSeq(0, /*is_poll_response=*/true);
  RunLoopUntilIdle();

  // Our retransmission should include our current request sequence number.
  EXPECT_EQ(2u, n_info_frames);
  EXPECT_EQ(10u, outbound_req_seq);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, PollTaskLoopsEvenWhenRemoteIsBusy) {
  size_t n_pdus = 0;
  auto tx_callback = [&](auto pdu) { ++n_pdus; };
  bool failure = false;
  auto connection_failure_callback = [&] { failure = true; };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/2, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/connection_failure_callback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(1u, n_pdus);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_EQ(2u, n_pdus);

  // Receive a "set RemoteBusy" in the poll response (RNR with F=1).
  n_pdus = 0;
  tx_engine.SetRemoteBusy();
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/true);
  ASSERT_EQ(0u, n_pdus);

  // No polls should have been sent out and the monitor task should not have fired.
  RunLoopFor(zx::sec(12));
  EXPECT_EQ(0u, n_pdus);
  EXPECT_FALSE(failure);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, SetRangeRetransmitCausesUpdateAckSeqToRetransmit) {
  size_t n_info_frames = 0;
  std::optional<uint8_t> tx_seq;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
      tx_seq = pdu->To<SimpleInformationFrameHeader>().tx_seq();
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(3u, n_info_frames);

  // Request a retransmission of unacked data starting with TxSeq=1 (so the second and third
  // frames).
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetRangeRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  EXPECT_EQ(2u, n_info_frames);
  EXPECT_EQ(2u, tx_seq.value_or(0));

  // Subsequent UpdateAckSeqs do not perform retransmission.
  n_info_frames = 0;
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  EXPECT_EQ(0u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetRangeRetransmitWithPollRequestTriggersPollResponseOnFirstIFrame) {
  size_t n_info_frames = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      // We send two I-Frames to start, then retransmit both but the third frame is in response to a
      // poll request so its and only its poll response bit should be set.
      SCOPED_TRACE(n_info_frames);
      EXPECT_EQ(n_info_frames == 2, pdu->To<EnhancedControlField>().is_poll_response());
      ++n_info_frames;
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/3, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(2u, n_info_frames);

  // Request a retransmission of unacked data starting with TxSeq=0 (all frames).
  tx_engine.SetRangeRetransmit(/*is_poll_request=*/true);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  EXPECT_EQ(4u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetRangeRetransmitAfterPollTaskSuppressesSubsequentRetransmitByPollResponse) {
  size_t n_info_frames = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(1u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Request a retransmission of unacked data starting with TxSeq=0.
  tx_engine.SetRangeRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  ASSERT_EQ(2u, n_info_frames);

  // Don't acknowledge data at TxSeq=0 again in the poll response, which _would_ retransmit them if
  // they hadn't already been retransmitted.
  n_info_frames = 0;
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/true);
  EXPECT_EQ(0u, n_info_frames);

  // And further non-acknowledgments that don't call for retransmission shouldn't cause
  // retransmission.
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  EXPECT_EQ(0u, n_info_frames);

  // Or further acknowledgments, for that matter.
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  EXPECT_EQ(0u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetRangeRetransmitWithPollResponseSetDoesNotSuppressSubsequentRetransmissions) {
  size_t n_info_frames = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(1u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Request a retransmission of unacked data starting with TxSeq=0 that also acknowledges the poll.
  tx_engine.SetRangeRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/true);
  ASSERT_EQ(2u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Don't acknowledge data at TxSeq=0 again in the next poll response, which causes another
  // retransmission.
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/true);
  EXPECT_EQ(3u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetRangeRetransmitSuppressesRetransmissionsByRetransmitRangeWithPollResponse) {
  size_t n_info_frames = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(1u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  // Request a retransmission of unacked data starting with TxSeq=0.
  tx_engine.SetRangeRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  ASSERT_EQ(2u, n_info_frames);

  // Request retransmission _and_ don't acknowledge data at TxSeq=0, which _would_ retransmit them
  // if they hadn't already been retransmitted.
  n_info_frames = 0;
  tx_engine.SetRangeRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/true);
  EXPECT_EQ(0u, n_info_frames);

  // And further non-acknowledgments that don't call for retransmission shouldn't cause
  // retransmission.
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  EXPECT_EQ(0u, n_info_frames);

  // Or further acknowledgments, for that matter.
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  EXPECT_EQ(0u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest, SetSingleRetransmitCausesUpdateAckSeqToRetransmit) {
  size_t n_info_frames = 0;
  std::optional<uint8_t> tx_seq;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
      tx_seq = pdu->To<SimpleInformationFrameHeader>().tx_seq();
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(3u, n_info_frames);

  // Request a retransmission of unacked data with TxSeq=1 (so only the second frame).
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  EXPECT_EQ(1u, n_info_frames);
  EXPECT_EQ(1u, tx_seq.value_or(0));

  // Subsequent UpdateAckSeqs do not perform retransmission.
  n_info_frames = 0;
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  EXPECT_EQ(0u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetSingleRetransmitWithPollRequestTriggersPollResponseOnIFrame) {
  size_t n_info_frames = 0;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      // We send two I-Frames to start, then retransmit both but the third frame is in response to a
      // poll request so its and only its poll response bit should be set.
      SCOPED_TRACE(n_info_frames);
      EXPECT_EQ(n_info_frames == 2, pdu->To<EnhancedControlField>().is_poll_response());
      ++n_info_frames;
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/3, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(2u, n_info_frames);

  // Request a retransmission of unacked data with TxSeq=0 (first frame).
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/true);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  EXPECT_EQ(3u, n_info_frames);

  // Request a retransmission of unacked data with TxSeq=1 (second frame).
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/false);
  EXPECT_EQ(4u, n_info_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetSingleRetransmitOnlyAcksIFramesIfPollRequestIsSet) {
  size_t n_info_frames = 0;
  std::optional<uint8_t> tx_seq;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField) &&
        pdu->To<EnhancedControlField>().designates_information_frame() &&
        pdu->size() >= sizeof(SimpleInformationFrameHeader)) {
      ++n_info_frames;
      tx_seq = pdu->To<SimpleInformationFrameHeader>().tx_seq();
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/2,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  // TxWindow prevents third I-Frame from going out.
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(2u, n_info_frames);

  // Request a retransmission of unacked data with TxSeq=1, not a poll request.
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);

  // Only the second I-Frame is retransmitted.
  EXPECT_EQ(1u, n_info_frames);
  EXPECT_EQ(1u, tx_seq.value_or(0));

  // Request a retransmission of unacked data with TxSeq=1, this time a poll request.
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/true);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);

  // The first frame is acked, the second I-Frame is retransmitted, and the third frame (which was
  // pending because of TxWindow) is transmitted for the first time.
  EXPECT_EQ(2u, n_info_frames);
  EXPECT_EQ(2u, tx_seq.value_or(0));
}

TEST_F(
    EnhancedRetransmissionModeTxEngineTest,
    SetSingleRetransmitDuringPollRequestThenAgainWithPollResponseAndDifferentAckSeqBothCauseRetransmit) {
  size_t n_info_frames = 0;
  size_t n_supervisory_frames = 0;
  std::optional<uint8_t> tx_seq;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField)) {
      if (pdu->To<EnhancedControlField>().designates_information_frame()) {
        ++n_info_frames;
        tx_seq = pdu->To<SimpleInformationFrameHeader>().tx_seq();
      } else if (pdu->To<EnhancedControlField>().designates_supervisory_frame()) {
        ++n_supervisory_frames;
      }
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(2u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_EQ(1U, n_supervisory_frames);

  // Request a retransmission of unacked data with TxSeq=1, not a poll response.
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);

  // The second I-Frame is retransmitted.
  EXPECT_EQ(1u, n_info_frames);
  EXPECT_EQ(1u, tx_seq.value_or(0));

  // Request a retransmission of unacked data with TxSeq=0, this time a poll response.
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(0u, /*is_poll_response=*/true);

  // The first I-Frame is retransmitted.
  EXPECT_EQ(1u, n_info_frames);
  EXPECT_EQ(0u, tx_seq.value_or(1));

  // Test that the second poll request stopped the MonitorTimer by checking that the ReceiverReady
  // poll was sent.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  EXPECT_EQ(2U, n_supervisory_frames);
}

TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetSingleRetransmitDuringPollRequestSuppressesSingleRetransmitPollResponseWithSameAckSeq) {
  size_t n_info_frames = 0;
  size_t n_supervisory_frames = 0;
  std::optional<uint8_t> tx_seq;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField)) {
      if (pdu->To<EnhancedControlField>().designates_information_frame()) {
        ++n_info_frames;
        tx_seq = pdu->To<SimpleInformationFrameHeader>().tx_seq();
      } else if (pdu->To<EnhancedControlField>().designates_supervisory_frame()) {
        ++n_supervisory_frames;
      }
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(2u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_EQ(1U, n_supervisory_frames);

  // Request a retransmission of unacked data with TxSeq=1, not a poll response.
  n_info_frames = 0;
  tx_seq.reset();
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);

  // The second I-Frame is retransmitted.
  EXPECT_EQ(1u, n_info_frames);
  EXPECT_EQ(1u, tx_seq.value_or(0));

  // Request a retransmission of unacked data with TxSeq=1, this time a poll response.
  n_info_frames = 0;
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/true);
  EXPECT_EQ(0u, n_info_frames);  // No duplicate retransmission occurs.

  // Test that the second poll request stopped the MonitorTimer by checking that the ReceiverReady
  // poll was sent.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  EXPECT_EQ(2U, n_supervisory_frames);
}

// This is a possible bug introduced by implementing SrejActioned to the letter per Core Spec
// v5.{0–2} Vol 3, Part A, Sec 8.6.5.9–11.
TEST_F(EnhancedRetransmissionModeTxEngineTest,
       SetSingleRetransmitDuringPollRequestSuppressesRetransmissionErroneously) {
  size_t n_info_frames = 0;
  size_t n_supervisory_frames = 0;
  std::optional<uint8_t> tx_seq;
  auto tx_callback = [&](ByteBufferPtr pdu) {
    if (pdu && pdu->size() >= sizeof(EnhancedControlField)) {
      if (pdu->To<EnhancedControlField>().designates_information_frame()) {
        ++n_info_frames;
        tx_seq = pdu->To<SimpleInformationFrameHeader>().tx_seq();
      } else if (pdu->To<EnhancedControlField>().designates_supervisory_frame()) {
        ++n_supervisory_frames;
      }
    }
  };
  TxEngine tx_engine(/*channel_id=*/kTestChannelId, /*max_tx_sdu_size=*/kDefaultMTU,
                     /*max_transmissions=*/4, /*n_frames_in_tx_window=*/kDefaultTxWindow,
                     /*send_frame_callback=*/tx_callback,
                     /*connection_failure_callback=*/NoOpFailureCallback);

  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  ASSERT_EQ(2u, n_info_frames);

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));
  ASSERT_EQ(1U, n_supervisory_frames);

  // Request a retransmission of unacked data with TxSeq=1, not a poll response.
  n_info_frames = 0;
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/false);
  ASSERT_EQ(1u, n_info_frames);

  // Ack the first frame and complete the poll.
  n_info_frames = 0;
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/true);
  ASSERT_EQ(1u, n_info_frames);  // Retransmission is not suppressed.

  // Send and acknowledge enough frames to roll over AckSeq.
  n_info_frames = 0;
  for (size_t i = 0; i < EnhancedControlField::kMaxSeqNum + 1; i++) {
    const auto ack_seq = (tx_seq.value() + 1) % (EnhancedControlField::kMaxSeqNum + 1);
    tx_engine.UpdateAckSeq(ack_seq, /*is_poll_response=*/false);
    tx_engine.QueueSdu(std::make_unique<DynamicByteBuffer>(kDefaultPayload));
  }

  // Now there's a new frame with TxSeq=1.
  ASSERT_EQ(uintmax_t{EnhancedControlField::kMaxSeqNum} + 1, n_info_frames);
  ASSERT_EQ(1u, tx_seq.value());

  // Let receiver_ready_poll_task_ fire, to transmit the poll.
  ASSERT_TRUE(RunLoopFor(zx::sec(2)));

  n_info_frames = 0;
  tx_engine.SetSingleRetransmit(/*is_poll_request=*/false);
  tx_engine.UpdateAckSeq(1u, /*is_poll_response=*/true);

  // Even though this is a different frame than the first retransmission in this test, the latter's
  // SrejActioned state suppresses this retransmission.
  EXPECT_EQ(0u, n_info_frames);
}

}  // namespace
}  // namespace bt::l2cap::internal
