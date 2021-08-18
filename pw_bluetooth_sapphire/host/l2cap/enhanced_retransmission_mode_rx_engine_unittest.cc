// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enhanced_retransmission_mode_rx_engine.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fragmenter.h"

namespace bt::l2cap::internal {
namespace {

constexpr hci::ConnectionHandle kTestHandle = 0x0001;
constexpr ChannelId kTestChannelId = 0x0001;
constexpr uint8_t kExtendedControlPBitMask = 0b0001'0000;
constexpr uint8_t kExtendedControlFBitMask = 0b1000'0000;
constexpr uint8_t kExtendedControlReceiverReadyBits = 0b0000'0000;
constexpr uint8_t kExtendedControlReceiverNotReadyBits = 0b0000'1000;
constexpr uint8_t kExtendedControlRejectBits = 0b0000'0100;
constexpr uint8_t kExtendedControlSelectiveRejectBits = 0b0000'1100;

using Engine = EnhancedRetransmissionModeRxEngine;

void NoOpTxCallback(ByteBufferPtr) {}
void NoOpFailureCallback() {}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduImmediatelyReturnsDataForUnsegmentedSdu) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const auto payload = CreateStaticByteBuffer(0, 0, 'h', 'e', 'l', 'l', 'o');
  const ByteBufferPtr sdu = Engine(NoOpTxCallback, NoOpFailureCallback)
                                .ProcessPdu(Fragmenter(kTestHandle)
                                                .BuildFrame(kTestChannelId, payload,
                                                            FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(sdu);
  EXPECT_TRUE(ContainersEqual(CreateStaticByteBuffer('h', 'e', 'l', 'l', 'o'), *sdu));
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduCanHandleZeroBytePayload) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const auto payload = CreateStaticByteBuffer(0, 0);
  const ByteBufferPtr sdu = Engine(NoOpTxCallback, NoOpFailureCallback)
                                .ProcessPdu(Fragmenter(kTestHandle)
                                                .BuildFrame(kTestChannelId, payload,
                                                            FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(sdu);
  EXPECT_EQ(0u, sdu->size());
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduCanHandleZeroBytePdu) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const StaticByteBuffer payload{0, 0};

  // FCS footer is entirely omitted.
  const ByteBufferPtr sdu =
      Engine(NoOpTxCallback, NoOpFailureCallback)
          .ProcessPdu(Fragmenter(kTestHandle)
                          .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kNoFcs));
  EXPECT_FALSE(sdu);
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduCanHandleIncompleteFcsFooter) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  // No payload is present and only one byte of the FCS footer is present.
  const StaticByteBuffer payload{0, 0, 0};
  const ByteBufferPtr sdu =
      Engine(NoOpTxCallback, NoOpFailureCallback)
          .ProcessPdu(Fragmenter(kTestHandle)
                          .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kNoFcs));
  EXPECT_FALSE(sdu);
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduDoesNotGenerateSduForOutOfSequencePdu) {
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const auto payload = CreateStaticByteBuffer(  //
      1 << 1,                                   // TxSeq = 1, R=0
      0,                                        // SAR and ReqSeq
      'h', 'e', 'l', 'l', 'o');
  EXPECT_FALSE(Engine(NoOpTxCallback, NoOpFailureCallback)
                   .ProcessPdu(Fragmenter(kTestHandle)
                                   .BuildFrame(kTestChannelId, payload,
                                               FrameCheckSequenceOption::kIncludeFcs)));
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduAdvancesSequenceNumberOnInSequenceFrame) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  // Send with sequence 0.
  {
    const auto payload = CreateStaticByteBuffer(  //
        0 << 1,                                   // TxSeq=0, R=0
        0,                                        // SAR and ReqSeq
        'h', 'e', 'l', 'l', 'o');
    ASSERT_TRUE(rx_engine.ProcessPdu(
        Fragmenter(kTestHandle)
            .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kIncludeFcs)));
  }

  // Send with sequence 1.
  {
    const auto payload = CreateStaticByteBuffer(  //
        1 << 1,                                   // TxSeq=1, R=0
        0,                                        // SAR and ReqSeq
        'h', 'e', 'l', 'l', 'o');
    ASSERT_TRUE(rx_engine.ProcessPdu(
        Fragmenter(kTestHandle)
            .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kIncludeFcs)));
  }

  // Send with sequence 2.
  {
    const auto payload = CreateStaticByteBuffer(  //
        2 << 1,                                   // TxSeq=2, R=0
        0,                                        // SAR and ReqSeq
        'h', 'e', 'l', 'l', 'o');
    EXPECT_TRUE(rx_engine.ProcessPdu(
        Fragmenter(kTestHandle)
            .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kIncludeFcs)));
  }
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduRollsOverSequenceNumber) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);
  auto payload = CreateStaticByteBuffer(  //
      0 << 1,                             // TxSeq=0, R=0
      0,                                  // SAR and ReqSeq
      'h', 'e', 'l', 'l', 'o');
  for (size_t i = 0; i < 64; ++i) {
    payload[0] = i << 1;  // Set TxSeq
    ASSERT_TRUE(rx_engine.ProcessPdu(
        Fragmenter(kTestHandle)
            .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kIncludeFcs)))
        << " (i=" << i << ")";
  }

  // Per Core Spec v5, Vol 3, Part A, Sec 8.3, the sequence number should now
  // roll over to 0.
  payload[0] = 0 << 1;
  EXPECT_TRUE(rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kIncludeFcs)));
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduDoesNotAdvanceSequenceNumberForOutOfSequencePdu) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);
  const auto out_of_seq = CreateStaticByteBuffer(  //
      1 << 1,                                      // TxSeq=1, R=0
      0,                                           // SAR and ReqSeq
      'h', 'e', 'l', 'l', 'o');
  ASSERT_FALSE(rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, out_of_seq, FrameCheckSequenceOption::kIncludeFcs)));

  const auto in_seq = CreateStaticByteBuffer(  //
      0 << 1,                                  // TxSeq=0, R=0
      0,                                       // SAR and ReqSeq
      'h', 'e', 'l', 'l', 'o');
  EXPECT_TRUE(rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, in_seq, FrameCheckSequenceOption::kIncludeFcs)));
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduImmediatelyAcksUnsegmentedSdu) {
  size_t n_acks = 0;
  ByteBufferPtr outbound_ack;
  auto tx_callback = [&](auto pdu) {
    outbound_ack = std::move(pdu);
    ++n_acks;
  };

  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const auto payload = CreateStaticByteBuffer(0, 0, 'h', 'e', 'l', 'l', 'o');
  ASSERT_TRUE(Engine(tx_callback, NoOpFailureCallback)
                  .ProcessPdu(Fragmenter(kTestHandle)
                                  .BuildFrame(kTestChannelId, payload,
                                              FrameCheckSequenceOption::kIncludeFcs)));
  EXPECT_EQ(1u, n_acks);
  ASSERT_TRUE(outbound_ack);
  ASSERT_EQ(sizeof(SimpleReceiverReadyFrame), outbound_ack->size());

  auto ack_frame = *reinterpret_cast<const SimpleReceiverReadyFrame *>(outbound_ack->data());
  EXPECT_EQ(SupervisoryFunction::ReceiverReady, ack_frame.function());
  EXPECT_EQ(1u, ack_frame.receive_seq_num());
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduSendsCorrectReqSeqOnRollover) {
  size_t n_acks = 0;
  ByteBufferPtr last_ack;
  auto tx_callback = [&](auto pdu) {
    last_ack = std::move(pdu);
    ++n_acks;
  };

  Engine rx_engine(tx_callback, NoOpFailureCallback);
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  for (size_t i = 0; i < 64; ++i) {
    const auto payload = CreateStaticByteBuffer(i << 1, 0, 'h', 'e', 'l', 'l', 'o');
    ASSERT_TRUE(rx_engine.ProcessPdu(
        Fragmenter(kTestHandle)
            .BuildFrame(kTestChannelId, payload, FrameCheckSequenceOption::kIncludeFcs)))
        << " (i=" << i << ")";
  }
  EXPECT_EQ(64u, n_acks);
  ASSERT_TRUE(last_ack);
  ASSERT_EQ(sizeof(SimpleReceiverReadyFrame), last_ack->size());

  auto ack_frame = *reinterpret_cast<const SimpleReceiverReadyFrame *>(last_ack->data());
  EXPECT_EQ(SupervisoryFunction::ReceiverReady, ack_frame.function());
  EXPECT_EQ(0u, ack_frame.receive_seq_num());
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduDoesNotAckOutOfSequenceFrame) {
  size_t n_acks = 0;
  ByteBufferPtr outbound_ack;
  auto tx_callback = [&](auto pdu) {
    outbound_ack = std::move(pdu);
    ++n_acks;
  };

  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const auto payload = CreateStaticByteBuffer(1, 0, 'h', 'e', 'l', 'l', 'o');

  // Per Core Spec, v5, Vol 3, Part A, Sec 8.4.7.1, receipt of an
  // out-of-sequence frame should cause us to transmit a Reject frame. We assume
  // that we should _not_ also transmit a ReceiverReady frame.
  //
  // TODO(fxbug.dev/1041): Revise this test when we start sending Reject frames.
  ASSERT_FALSE(Engine(tx_callback, NoOpFailureCallback)
                   .ProcessPdu(Fragmenter(kTestHandle)
                                   .BuildFrame(kTestChannelId, payload,
                                               FrameCheckSequenceOption::kIncludeFcs)));
  EXPECT_EQ(0u, n_acks);
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduRespondsToReceiverReadyPollRequest) {
  size_t n_outbound_frames = 0;
  ByteBufferPtr last_outbound_frame;
  auto tx_callback = [&](auto pdu) {
    last_outbound_frame = std::move(pdu);
    ++n_outbound_frames;
  };

  Engine rx_engine(tx_callback, NoOpFailureCallback);
  // Send an I-frame to advance the receiver's sequence number.
  // See Core Spec, v5, Vol 3, Part A, Table 3.2 for the first two bytes.
  const auto info_frame = CreateStaticByteBuffer(0, 0, 'h', 'e', 'l', 'l', 'o');
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, info_frame, FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_EQ(1u, n_outbound_frames);

  // Now send a ReceiverReady poll request. See Core Spec, v5, Vol 3, Part A,
  // Table 3.2 and Table 3.5 for frame format.
  const auto receiver_ready_poll_request =
      CreateStaticByteBuffer(0b1 | kExtendedControlPBitMask, 0);
  auto local_sdu = rx_engine.ProcessPdu(Fragmenter(kTestHandle)
                                            .BuildFrame(kTestChannelId, receiver_ready_poll_request,
                                                        FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverReady frame.
  EXPECT_EQ(2u, n_outbound_frames);
  ASSERT_TRUE(last_outbound_frame);
  ASSERT_EQ(sizeof(SimpleSupervisoryFrame), last_outbound_frame->size());

  auto sframe = *reinterpret_cast<const SimpleSupervisoryFrame *>(last_outbound_frame->data());
  EXPECT_EQ(SupervisoryFunction::ReceiverReady, sframe.function());
  EXPECT_EQ(1u, sframe.receive_seq_num());
  EXPECT_TRUE(sframe.is_poll_response());
  EXPECT_FALSE(sframe.is_poll_request());
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduCallsReceiveSeqNumCallback) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  std::optional<uint8_t> receive_seq_num;
  std::optional<bool> receive_is_poll_response;
  auto receive_seq_num_callback = [&receive_seq_num, &receive_is_poll_response](
                                      uint8_t seq_num, bool is_poll_response) {
    receive_seq_num = seq_num;
    receive_is_poll_response = is_poll_response;
  };
  rx_engine.set_receive_seq_num_callback(receive_seq_num_callback);

  // Send an I-frame containing an acknowledgment up to the 3rd frame that we transmitted.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, Table 3.2 for the first two bytes.
  auto info_frame = StaticByteBuffer(0, 3, 'h', 'e', 'l', 'l', 'o');
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, info_frame, FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(receive_seq_num.has_value());
  EXPECT_EQ(3, receive_seq_num.value());
  ASSERT_TRUE(receive_is_poll_response.has_value());
  EXPECT_FALSE(receive_is_poll_response.value());

  receive_is_poll_response.reset();

  // Same as above but the 'F' bit is set.
  info_frame[0] |= kExtendedControlFBitMask;
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, info_frame, FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(receive_is_poll_response.has_value());
  EXPECT_TRUE(receive_is_poll_response.value());

  receive_seq_num.reset();
  receive_is_poll_response.reset();

  // Send an S-frame containing an acknowledgment up to the 4th frame that we transmitted. F is set.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, Table 3.2 for the frame format.
  auto receiver_ready = StaticByteBuffer(0b1 | kExtendedControlFBitMask, 4);
  auto local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready, FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(receive_seq_num.has_value());
  EXPECT_EQ(4, receive_seq_num.value());
  ASSERT_TRUE(receive_is_poll_response.has_value());
  EXPECT_TRUE(receive_is_poll_response.value());

  receive_is_poll_response.reset();

  // Same as above but the 'F' bit is clear.
  receiver_ready[0] &= ~kExtendedControlFBitMask;
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready, FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(receive_is_poll_response.has_value());
  EXPECT_FALSE(receive_is_poll_response.value());
}

TEST(EnhancedRetransmissionModeRxEngineTest, ProcessPduCallsAckSeqNumCallback) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  std::optional<uint8_t> ack_seq_num;
  auto ack_seq_num_callback = [&ack_seq_num](uint8_t seq_num) { ack_seq_num = seq_num; };
  rx_engine.set_ack_seq_num_callback(ack_seq_num_callback);

  // Send an I-frame containing a sequence number for the first frame the receiver has sent.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, Table 3.2 for the first two bytes.
  auto info_frame = StaticByteBuffer(0, 0, 'h', 'e', 'l', 'l', 'o');
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, info_frame, FrameCheckSequenceOption::kIncludeFcs));
  ASSERT_TRUE(ack_seq_num.has_value());

  // We should now expect the next (second) frame to have a sequence number of 1.
  EXPECT_EQ(1, ack_seq_num.value());
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduCallsRemoteBusySetCallbackOnReceiverNotReady) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  bool receive_seq_num_called = false;
  rx_engine.set_receive_seq_num_callback([&](uint8_t, bool) { receive_seq_num_called = true; });

  bool remote_busy_set_called = false;
  auto remote_busy_set_callback = [&receive_seq_num_called, &remote_busy_set_called] {
    // RemoteBusy state should be updated before ReceiveSeqNum to immediately suppress
    // retransmissions.
    EXPECT_FALSE(receive_seq_num_called);
    remote_busy_set_called = true;
  };
  rx_engine.set_remote_busy_set_callback(remote_busy_set_callback);

  const StaticByteBuffer receiver_not_ready(0b1 | kExtendedControlReceiverNotReadyBits, 0);
  auto local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_not_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverNotReady frame.
  EXPECT_TRUE(remote_busy_set_called);
  EXPECT_TRUE(receive_seq_num_called);

  remote_busy_set_called = false;
  local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_not_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverNotReady frame.

  // Second RNR should not invoke the callback.
  EXPECT_FALSE(remote_busy_set_called);
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduCallsRemoteBusySetCallbackOnReceiverNotReadyAfterReceiverReadyClearedBusy) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  const StaticByteBuffer receiver_not_ready(0b1 | kExtendedControlReceiverNotReadyBits, 0);
  auto local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_not_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverNotReady frame.

  // This RR should clear RemoteBusy.
  const StaticByteBuffer receiver_ready(0b1, 0);
  local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverReady frame.

  bool remote_busy_set_called = false;
  auto remote_busy_set_callback = [&remote_busy_set_called] { remote_busy_set_called = true; };
  rx_engine.set_remote_busy_set_callback(remote_busy_set_callback);

  // Receive a second RNR.
  local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_not_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverNotReady frame.

  // Second RNR should invoke the callback because it's setting RemoteBusy once again.
  EXPECT_TRUE(remote_busy_set_called);
}

// Test parameter is a bitmask to the Extended Control Field.
class ExtendedControlFieldBitsTest : public testing::TestWithParam<uint16_t> {};

TEST_P(ExtendedControlFieldBitsTest,
       ProcessPduCallsRemoteBusyClearedCallbackOnNonRnrSFrameAfterReceiverNotReady) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  bool receive_seq_num_called = false;
  int remote_busy_set_calls = 0;
  rx_engine.set_remote_busy_set_callback([&] { remote_busy_set_calls++; });

  int remote_busy_cleared_calls = 0;
  auto remote_busy_cleared_callback = [&receive_seq_num_called, &remote_busy_cleared_calls] {
    // RemoteBusy state should be updated before ReceiveSeqNum to immediately resume
    // retransmissions.
    EXPECT_FALSE(receive_seq_num_called);
    remote_busy_cleared_calls++;
  };
  rx_engine.set_remote_busy_cleared_callback(remote_busy_cleared_callback);

  const StaticByteBuffer receiver_not_ready(0b1 | kExtendedControlReceiverNotReadyBits, 0);
  auto local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, receiver_not_ready, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in a ReceiverNotReady frame.

  EXPECT_EQ(1, remote_busy_set_calls);
  EXPECT_EQ(0, remote_busy_cleared_calls);

  // The RNR invokes this callback but we only care about the callback ordering of the next frame.
  rx_engine.set_receive_seq_num_callback([&](uint8_t, bool) { receive_seq_num_called = true; });

  // This non-RNR S-Frame should clear RemoteBusy.
  const uint16_t control_bits_to_set = GetParam();
  const StaticByteBuffer non_rnr_s_frame(0b1 | LowerBits(control_bits_to_set), 0);
  local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, non_rnr_s_frame, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in an S-Frame.

  EXPECT_EQ(1, remote_busy_set_calls);
  EXPECT_EQ(1, remote_busy_cleared_calls);
  EXPECT_TRUE(receive_seq_num_called);

  // Receive a second non-RNR.
  local_sdu = rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, non_rnr_s_frame, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_FALSE(local_sdu);  // No payload in an S-Frame.

  EXPECT_EQ(1, remote_busy_set_calls);
  EXPECT_EQ(1, remote_busy_cleared_calls);

  // Second S-Frame shouldn't invoke either callback because RemoteBusy remains cleared.
}

INSTANTIATE_TEST_SUITE_P(EnhancedRetransmissionModeRxEngineTestNonRnrSFrames,
                         ExtendedControlFieldBitsTest,
                         testing::Values(kExtendedControlReceiverReadyBits,
                                         kExtendedControlRejectBits,
                                         kExtendedControlSelectiveRejectBits));

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduCallsRangeRetransmitSetCallbackThenReceiveSeqNumCallbackOnReject) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  std::optional<bool> receive_is_poll_request;
  rx_engine.set_range_retransmit_set_callback(
      [&](bool is_poll_request) { receive_is_poll_request = is_poll_request; });

  std::optional<uint8_t> receive_seq_num;
  std::optional<bool> receive_is_poll_response;
  auto receive_seq_num_callback = [&](uint8_t seq_num, bool is_poll_response) {
    // RangeRetransmitCallback should be called before ReceiveSeqNumCallback.
    EXPECT_TRUE(receive_is_poll_request.has_value());
    receive_seq_num = seq_num;
    receive_is_poll_response = is_poll_response;
  };
  rx_engine.set_receive_seq_num_callback(receive_seq_num_callback);

  // Send a REJ S-frame containing acknowledgment up to the 3rd frame that we transmitted.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, Table 3.2 for the first two bytes.
  auto rej_frame = StaticByteBuffer(0b1 | kExtendedControlRejectBits, 3);
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, rej_frame, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_TRUE(receive_seq_num.has_value());
  EXPECT_TRUE(receive_is_poll_response.has_value());
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduCallsSingleRetransmitSetCallbackThenReceiveSeqNumCallbackOnSelectiveReject) {
  Engine rx_engine(NoOpTxCallback, NoOpFailureCallback);

  std::optional<bool> receive_is_poll_request;
  rx_engine.set_single_retransmit_set_callback(
      [&](bool is_poll_request) { receive_is_poll_request = is_poll_request; });

  std::optional<uint8_t> receive_seq_num;
  std::optional<bool> receive_is_poll_response;
  auto receive_seq_num_callback = [&](uint8_t seq_num, bool is_poll_response) {
    // SingleRetransmitCallback should be called before ReceiveSeqNumCallback.
    EXPECT_TRUE(receive_is_poll_request.has_value());
    receive_seq_num = seq_num;
    receive_is_poll_response = is_poll_response;
  };
  rx_engine.set_receive_seq_num_callback(receive_seq_num_callback);

  // Send an SREJ S-frame containing acknowledgment up to the 3rd frame that we transmitted.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, Table 3.2 for the first two bytes.
  auto srej_frame = StaticByteBuffer(0b1 | kExtendedControlSelectiveRejectBits, 3);
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, srej_frame, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_TRUE(receive_seq_num.has_value());
  EXPECT_TRUE(receive_is_poll_response.has_value());
}

TEST(EnhancedRetransmissionModeRxEngineTest,
     ProcessPduWithPollResponseAndPollRequestClosesChannel) {
  bool connection_failed = false;
  Engine rx_engine(NoOpTxCallback, [&] { connection_failed = true; });

  // Send an RR S-frame with both poll request and response bits set.
  // See Core Spec, v5, Vol 3, Part A, Section 3.3.2, Table 3.2 for field definitions.
  auto rr_frame = StaticByteBuffer(0b1 | kExtendedControlFBitMask | kExtendedControlPBitMask, 0);
  rx_engine.ProcessPdu(
      Fragmenter(kTestHandle)
          .BuildFrame(kTestChannelId, rr_frame, FrameCheckSequenceOption::kIncludeFcs));
  EXPECT_TRUE(connection_failed);
}

}  // namespace
}  // namespace bt::l2cap::internal
