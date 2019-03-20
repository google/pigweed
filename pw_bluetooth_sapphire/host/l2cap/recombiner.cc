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

Recombiner::Recombiner() : ready_(false), frame_length_(0u), cur_length_(0u) {}

bool Recombiner::AddFragment(hci::ACLDataPacketPtr&& fragment) {
  ZX_DEBUG_ASSERT(fragment);

  if (ready())
    return false;

  if (empty()) {
    if (!ProcessFirstFragment(*fragment))
      return false;
    ZX_DEBUG_ASSERT(!empty());
  } else {
    if (fragment->packet_boundary_flag() !=
        hci::ACLPacketBoundaryFlag::kContinuingFragment) {
      bt_log(SPEW, "l2cap", "expected continuing fragment!");
      return false;
    }

    if (cur_length_ + fragment->view().payload_size() > frame_length_) {
      bt_log(SPEW, "l2cap", "continuing fragment too long!");
      return false;
    }
  }

  cur_length_ += fragment->view().payload_size();
  if (frame_length_ == cur_length_) {
    // The PDU is complete.
    ready_ = true;
  }

  pdu_->AppendFragment(std::move(fragment));
  return true;
}

bool Recombiner::Release(PDU* out_pdu) {
  if (empty() || !ready())
    return false;

  ZX_DEBUG_ASSERT(out_pdu);

  *out_pdu = std::move(*pdu_);
  Drop();

  return true;
}

void Recombiner::Drop() {
  ready_ = false;
  frame_length_ = 0u;
  cur_length_ = 0u;
  pdu_.reset();
}

bool Recombiner::ProcessFirstFragment(const hci::ACLDataPacket& fragment) {
  ZX_DEBUG_ASSERT(!ready());
  ZX_DEBUG_ASSERT(!frame_length_);
  ZX_DEBUG_ASSERT(!cur_length_);

  // The first fragment needs to at least contain the Basic L2CAP header and
  // should not be a continuation fragment.
  if (fragment.packet_boundary_flag() ==
          hci::ACLPacketBoundaryFlag::kContinuingFragment ||
      fragment.view().payload_size() < sizeof(BasicHeader)) {
    bt_log(SPEW, "l2cap", "bad first fragment");
    return false;
  }

  uint16_t frame_length =
      le16toh(GetBasicHeader(fragment).length) + sizeof(BasicHeader);

  if (fragment.view().payload_size() > frame_length) {
    bt_log(SPEW, "l2cap", "fragment too long!");
    return false;
  }

  pdu_ = PDU();
  frame_length_ = frame_length;

  return true;
}

}  // namespace l2cap
}  // namespace bt
