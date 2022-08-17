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

LowEnergyScanner::LowEnergyScanner(fxl::WeakPtr<Transport> hci, async_dispatcher_t* dispatcher)
    : state_(State::kIdle),
      active_scan_requested_(false),
      delegate_(nullptr),
      dispatcher_(dispatcher),
      transport_(std::move(hci)) {
  BT_DEBUG_ASSERT(transport_);
  BT_DEBUG_ASSERT(dispatcher_);

  hci_cmd_runner_ = std::make_unique<SequentialCommandRunner>(transport_);
}

}  // namespace bt::hci
