// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/enhanced_retransmission_mode_rx_engine.h"

namespace bt {
namespace l2cap {
namespace internal {

namespace {

template <typename T>
std::optional<T> TryCopyFromPdu(const PDU& pdu) {
  if (pdu.length() < sizeof(T))
    return std::nullopt;

  StaticByteBuffer<sizeof(T)> buf;
  pdu.Copy(&buf, 0, sizeof(T));
  return buf.template As<T>();
}

std::variant<std::monostate, const SimpleInformationFrameHeader,
             const SimpleStartOfSduFrameHeader, const SimpleSupervisoryFrame>
GetFrameHeaderFromPdu(const PDU& pdu) {
  const auto control_field_opt = TryCopyFromPdu<EnhancedControlField>(pdu);
  if (!control_field_opt) {
    // TODO(BT-713): Add metric counting runt frames.
    return std::monostate();
  }

  const auto& control_field = control_field_opt.value();
  if (control_field.designates_supervisory_frame()) {
    const auto frame_opt = TryCopyFromPdu<SimpleSupervisoryFrame>(pdu);
    if (!frame_opt) {
      // TODO(BT-713): Add metric counting runt S-frames.
      return std::monostate();
    }
    return frame_opt.value();
  }

  if (control_field.designates_start_of_segmented_sdu()) {
    const auto frame_opt = TryCopyFromPdu<SimpleStartOfSduFrameHeader>(pdu);
    if (!frame_opt) {
      // TODO(BT-713): Add metric counting runt Start-of-SDU frames.
      return std::monostate();
    }
    return frame_opt.value();
  }

  const auto frame_opt = TryCopyFromPdu<SimpleInformationFrameHeader>(pdu);
  if (!frame_opt) {
    // TODO(BT-713): Add metric counting runt I-frames.
    return std::monostate();
  }
  return frame_opt.value();
}

bool IsMpsValid(const PDU& pdu) {
  // TODO(quiche): Check PDU's length against the MPS.
  return true;
}

}  // namespace

using Engine = EnhancedRetransmissionModeRxEngine;

Engine::EnhancedRetransmissionModeRxEngine(
    SendBasicFrameCallback send_basic_frame_callback)
    : next_seqnum_(0),
      send_basic_frame_callback_(std::move(send_basic_frame_callback)) {}

ByteBufferPtr Engine::ProcessPdu(PDU pdu) {
  // A note on validation (see Vol 3, Part A, 3.3.7):
  //
  // We skip step 1 (validation of the Channel ID), as a frame with an
  // unrecognized Channel ID will not be delivered to us. (Various
  // L2CAP_ChannelManagerTest test cases verify that LogicalLink directs frames
  // to their proper channels.)
  //
  // We skip step 2 (validation of FCS), as we don't support FCS.
  //
  // Step 3 (size checking) is implemented in IsMpsValid(), and
  // GetFrameHeaderFromPdu().
  //
  // TODO(quiche): Implement step 4/5 (Check SAR bits, close connection on
  // error).

  if (!IsMpsValid(pdu)) {
    // TODO(quiche): Close connection.
    // TODO(BT-713): Add metric counting oversized frames.
    return nullptr;
  }

  auto header = GetFrameHeaderFromPdu(pdu);
  return std::visit(
      [this, pdu = std::move(pdu)](auto&& header) mutable {
        return ProcessFrame(header, std::move(pdu));
      },
      header);
}

ByteBufferPtr Engine::ProcessFrame(const SimpleInformationFrameHeader header,
                                   PDU pdu) {
  if (header.tx_seq() != next_seqnum_) {
    // TODO(quiche): Send REJ frame.
    // TODO(quiche): Add histogram for |header.tx_seq() - next_seqnum_|. This
    // will give us an upper bound on the potential benefit of sending SREJ
    // frames.
    return nullptr;
  }

  // TODO(quiche): check if the frame is within the permitted window.

  if (header.designates_part_of_segmented_sdu()) {
    // TODO(quiche): Implement validation and handling of segmented frames.
    return nullptr;
  }

  AdvanceSeqNum();

  SimpleReceiverReadyFrame ack_frame;
  ack_frame.set_request_seq_num(next_seqnum_);
  send_basic_frame_callback_(std::make_unique<DynamicByteBuffer>(
      BufferView(&ack_frame, sizeof(ack_frame))));

  const auto header_len = sizeof(header);
  const auto payload_len = pdu.length() - header_len;
  auto sdu = std::make_unique<DynamicByteBuffer>(payload_len);
  pdu.Copy(sdu.get(), header_len, payload_len);
  return sdu;
}

ByteBufferPtr Engine::ProcessFrame(const SimpleStartOfSduFrameHeader, PDU pdu) {
  // TODO(quiche): Implement validation and handling of Start-of-SDU frames.
  return nullptr;
}

ByteBufferPtr Engine::ProcessFrame(const SimpleSupervisoryFrame sframe,
                                   PDU pdu) {
  if (sframe.function() == SupervisoryFunction::ReceiverReady &&
      sframe.is_poll_request()) {
    // TODO(quiche): Propagate ReqSeq to the transmit engine.
    // See Core Spec, v5, Vol 3, Part A, Table 8.6, "Recv RR(P=1)".
    //
    // Note, however, that there may be additional work to do if we're in the
    // REJ_SENT state. See Core Spec, v5, Vol 3, Part A, Table 8.7, "Recv
    // RR(P=1)".
    SimpleReceiverReadyFrame poll_response;
    poll_response.set_is_poll_response();
    poll_response.set_request_seq_num(next_seqnum_);
    send_basic_frame_callback_(std::make_unique<DynamicByteBuffer>(
        BufferView(&poll_response, sizeof(poll_response))));
    return nullptr;
  }

  // TODO(quiche): Implement handling of other S-frames.
  return nullptr;
}

ByteBufferPtr Engine::ProcessFrame(std::monostate, PDU pdu) {
  // TODO(quiche): Close connection.
  return nullptr;
}

void Engine::AdvanceSeqNum() {
  ++next_seqnum_;
  if (next_seqnum_ > EnhancedControlField::kMaxSeqNum) {
    next_seqnum_ = 0;
  }
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bt
