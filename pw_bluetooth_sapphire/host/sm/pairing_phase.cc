// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pairing_phase.h"

#include <lib/async/default.h>
#include <zircon/assert.h>

#include <unordered_map>

#include "lib/fit/result.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt {
namespace sm {

PairingPhase::PairingPhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                           Role role)
    : chan_(std::move(chan)), listener_(std::move(listener)), role_(role) {
  ZX_ASSERT(listener_);
  ZX_ASSERT(chan_);
  ZX_ASSERT_MSG(async_get_default_dispatcher(), "default dispatcher required!");
  const PairingChannel& initialized_chan = *chan_;
  if (initialized_chan->link_type() == hci::Connection::LinkType::kLE) {
    ZX_ASSERT(initialized_chan->id() == l2cap::kLESMPChannelId);
  } else if (initialized_chan->link_type() == hci::Connection::LinkType::kACL) {
    ZX_ASSERT(initialized_chan->id() == l2cap::kSMPChannelId);
  } else {
    ZX_PANIC("unsupported link type!");
  }
}

void PairingPhase::SendPairingFailed(ErrorCode ecode) {
  auto pdu = util::NewPdu(sizeof(ErrorCode));
  PacketWriter writer(kPairingFailed, pdu.get());
  *writer.mutable_payload<PairingFailedParams>() = ecode;
  sm_chan()->Send(std::move(pdu));
}

}  // namespace sm
}  // namespace bt
