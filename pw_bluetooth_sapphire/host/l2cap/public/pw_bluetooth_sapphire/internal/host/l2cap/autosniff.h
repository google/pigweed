// Copyright 2025 The Pigweed Authors
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

#pragma once

#include "pw_bluetooth_sapphire/internal/host/common/smart_task.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

namespace bt::l2cap::internal {

struct SniffModeParams {
  uint16_t min_interval;
  uint16_t max_interval;
  uint16_t sniff_attempt;
  uint16_t sniff_timeout;
};

// Implements autosniff functionality on top of the logical link
class Autosniff {
 public:
  Autosniff(SniffModeParams params,
            hci::CommandChannel* channel,
            hci_spec::ConnectionHandle handle,
            pw::async::Dispatcher* dispatcher,
            pw::chrono::SystemClock::duration idle_timeout);

  void MarkPacketRx();
  void MarkPacketTx();

 private:
  SniffModeParams params_;
  RecurringTask autosniff_timeout_;
  hci::CommandChannel* cmd_channel_;
  hci_spec::ConnectionHandle handle_;
  hci::CommandChannel::OwnedEventHandle mode_change_event_;
  pw::bluetooth::emboss::AclConnectionMode connection_mode_ =
      pw::bluetooth::emboss::AclConnectionMode::ACTIVE;

  // Used to avoid a mode change request while in the process of transitioning
  bool mode_transition_ = false;

  RecurringDisposition OnTimeout();
  void ResetTimeout();
  hci::CommandChannel::EventCallbackResult OnModeChange(
      const hci::EventPacket& event);

 public:
  inline pw::bluetooth::emboss::AclConnectionMode CurrentMode() const {
    return connection_mode_;
  }
};

}  // namespace bt::l2cap::internal
