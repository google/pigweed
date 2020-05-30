// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "low_energy_scanner.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/sequential_command_runner.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"

namespace bt {
namespace hci {

// Default implementations do nothing.

void LowEnergyScanner::Delegate::OnPeerFound(const LowEnergyScanResult& result,
                                             const ByteBuffer& data) {}

void LowEnergyScanner::Delegate::OnDirectedAdvertisement(const LowEnergyScanResult& result) {}

LowEnergyScanResult::LowEnergyScanResult()
    : resolved(false), connectable(false), rssi(hci::kRSSIInvalid) {}

LowEnergyScanResult::LowEnergyScanResult(const DeviceAddress& address, bool resolved,
                                         bool connectable, bool scan_response, int8_t rssi)
    : address(address),
      resolved(resolved),
      connectable(connectable),
      scan_response(scan_response),
      rssi(rssi) {
  ZX_DEBUG_ASSERT_MSG(!connectable || !scan_response,
                      "cannot be both connectable and a scan response");
}

LowEnergyScanner::LowEnergyScanner(fxl::WeakPtr<Transport> hci, async_dispatcher_t* dispatcher)
    : state_(State::kIdle),
      active_scan_requested_(false),
      delegate_(nullptr),
      dispatcher_(dispatcher),
      transport_(std::move(hci)) {
  ZX_DEBUG_ASSERT(transport_);
  ZX_DEBUG_ASSERT(dispatcher_);

  hci_cmd_runner_ = std::make_unique<SequentialCommandRunner>(dispatcher_, transport_);
}

}  // namespace hci
}  // namespace bt
