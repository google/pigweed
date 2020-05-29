// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "recombiner.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace l2cap {
namespace {

const BasicHeader& GetBasicHeader(const hci::ACLDataPacket& fragment) {
  ZX_DEBUG_ASSERT(fragment.packet_boundary_flag() !=
                  hci::ACLPacketBoundaryFlag::kContinuingFragment);
  return fragment.view().payload<BasicHeader>();
}

}  // namespace

Recombiner::Recombiner(hci::ConnectionHandle handle) : handle_(handle) {}

Recombiner::Result Recombiner::ConsumeFragment(hci::ACLDataPacketPtr fragment) {
  ZX_DEBUG_ASSERT(fragment);
  ZX_DEBUG_ASSERT(fragment->connection_handle() == handle_);
  TRACE_DURATION("bluetooth", "Recombiner::AddFragment");

  if (!recombination_) {
    return ProcessFirstFragment(std::move(fragment));
  }

  // If we received a new initial packet without completing the recombination, then drop the
  // entire last sequence.
  if (fragment->packet_boundary_flag() != hci::ACLPacketBoundaryFlag::kContinuingFragment) {
    bt_log(WARN, "l2cap", "expected continuing fragment! (handle: %.4x)", handle_);
    ClearRecombination();

    // Try to initiate a new starting sequence with |fragment|.
    auto result = ProcessFirstFragment(std::move(fragment));

    // Report an error for the dropped frame, even if there was no error processing |fragment|
    // itself.
    result.frames_dropped = true;
    return result;
  }

  recombination_->accumulated_length += fragment->view().payload_size();
  recombination_->pdu.AppendFragment(std::move(fragment));
  BeginTrace();

  if (recombination_->accumulated_length > recombination_->expected_frame_length) {
    bt_log(WARN, "l2cap", "continuing fragment too long! (handle: %.4x)", handle_);
    ClearRecombination();

    // Drop |fragment| since a continuing fragment cannot begin a sequence.
    return {.frames_dropped = true};
  }

  if (recombination_->accumulated_length == recombination_->expected_frame_length) {
    // The frame is complete!
    auto pdu = std::move(recombination_->pdu);
    ClearRecombination();
    return {.pdu = {std::move(pdu)}, .frames_dropped = false};
  }

  // The frame is not complete yet.
  return {.frames_dropped = false};
}

Recombiner::Result Recombiner::ProcessFirstFragment(hci::ACLDataPacketPtr fragment) {
  ZX_DEBUG_ASSERT(fragment);
  ZX_DEBUG_ASSERT(!recombination_);

  // The first fragment needs to at least contain the Basic L2CAP header and
  // should not be a continuation fragment.
  size_t current_length = fragment->view().payload_size();
  if (fragment->packet_boundary_flag() == hci::ACLPacketBoundaryFlag::kContinuingFragment ||
      current_length < sizeof(BasicHeader)) {
    bt_log(DEBUG, "l2cap", "bad first fragment (size: %zu)", current_length);
    return {.frames_dropped = true};
  }

  // TODO(armansito): Also validate that the controller honors the HCI packet boundary flag contract
  // for the controller-to-host flow direction.

  size_t expected_frame_length = le16toh(GetBasicHeader(*fragment).length) + sizeof(BasicHeader);

  if (current_length > expected_frame_length) {
    bt_log(DEBUG, "l2cap",
           "fragment malformed: payload too long (expected length: %zu, fragment length: %zu)",
           expected_frame_length, current_length);
    return {.frames_dropped = true};
  }

  // We can start building a PDU.
  PDU pdu;
  pdu.AppendFragment(std::move(fragment));

  if (current_length == expected_frame_length) {
    // The PDU is complete.
    return {.pdu = {std::move(pdu)}, .frames_dropped = false};
  }

  // We need to recombine multiple fragments to obtain a complete PDU.
  BeginTrace();
  recombination_ = {
      .pdu = std::move(pdu),
      .expected_frame_length = expected_frame_length,
      .accumulated_length = current_length,
  };
  return {.frames_dropped = false};
}

void Recombiner::ClearRecombination() {
  ZX_DEBUG_ASSERT(recombination_);
  if (recombination_->pdu.is_valid()) {
    bt_log(DEBUG, "l2cap",
           "recombiner dropped packet (fragments: %zu, expected length: %zu, accumulated length: "
           "%zu, handle: %.4x)",
           recombination_->pdu.fragment_count(), recombination_->expected_frame_length,
           recombination_->accumulated_length, handle_);
  }
  recombination_.reset();
  EndTraces();
}

void Recombiner::BeginTrace() {
#ifndef NTRACE
  trace_flow_id_t flow_id = TRACE_NONCE();
  TRACE_FLOW_BEGIN("bluetooth", "Recombiner buffered ACL data fragment", flow_id);
  trace_ids_.push_back(flow_id);
#endif
}

void Recombiner::EndTraces() {
#ifndef NTRACE
  if (TRACE_ENABLED()) {
    for (auto flow_id : trace_ids_) {
      TRACE_FLOW_END("bluetooth", "Recombiner buffered ACL data fragment", flow_id);
    }
    trace_ids_.clear();
  }
#endif
}

}  // namespace l2cap
}  // namespace bt
