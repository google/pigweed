// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/hci/low_energy_scanner.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/hci/sequential_command_runner.h"
#include "pw_bluetooth_sapphire/internal/host/transport/transport.h"

namespace bt::hci {

// Default implementations do nothing.

void LowEnergyScanner::Delegate::OnPeerFound(const LowEnergyScanResult& result,
                                             const ByteBuffer& data) {}

void LowEnergyScanner::Delegate::OnDirectedAdvertisement(
    const LowEnergyScanResult& result) {}

LowEnergyScanner::LowEnergyScanner(hci::Transport::WeakPtr hci,
                                   pw::async::Dispatcher& pw_dispatcher)
    : state_(State::kIdle),
      active_scan_requested_(false),
      delegate_(nullptr),
      pw_dispatcher_(pw_dispatcher),
      transport_(std::move(hci)) {
  BT_DEBUG_ASSERT(transport_.is_alive());

  hci_cmd_runner_ = std::make_unique<SequentialCommandRunner>(
      transport_->command_channel()->AsWeakPtr());
}

}  // namespace bt::hci
