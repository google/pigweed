// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_scanner.h"

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {

// Default implementations do nothing.

void LowEnergyScanner::Delegate::OnPeerFound(const LowEnergyScanResult& result,
                                             const ByteBuffer& data) {}

void LowEnergyScanner::Delegate::OnDirectedAdvertisement(const LowEnergyScanResult& result) {}

LowEnergyScanner::LowEnergyScanner(hci::Transport::WeakPtr hci,
                                   pw::async::Dispatcher& pw_dispatcher)
    : state_(State::kIdle),
      active_scan_requested_(false),
      delegate_(nullptr),
      pw_dispatcher_(pw_dispatcher),
      transport_(std::move(hci)) {
  BT_DEBUG_ASSERT(transport_.is_alive());

  hci_cmd_runner_ =
      std::make_unique<SequentialCommandRunner>(transport_->command_channel()->AsWeakPtr());
}

}  // namespace bt::hci
