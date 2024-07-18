// Copyright 2024 The Pigweed Authors
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

#include <cstdint>

#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/iso/iso_common.h"
#include "pw_bluetooth_sapphire/internal/host/transport/command_channel.h"

namespace bt::iso {

class IsoStream final {
 public:
  IsoStream(uint8_t cig_id,
            uint8_t cis_id,
            hci_spec::ConnectionHandle cis_handle,
            CisEstablishedCallback cb,
            hci::CommandChannel::WeakPtr cmd_channel,
            pw::Callback<void()> on_closed_cb);

  // Handler for incoming HCI_LE_CIS_Established events. Returns a value
  // indicating whether the vent was handled.
  bool OnCisEstablished(const hci::EmbossEventPacket& event);

  // Terminate this stream.
  void Close();

  using WeakPtr = WeakSelf<IsoStream>::WeakPtr;
  WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  enum class IsoStreamState {
    kNotEstablished,
    kEstablished,
  } state_;

  uint8_t cig_id_ __attribute__((unused));
  uint8_t cis_id_ __attribute__((unused));

  // Connection parameters, only valid after CIS is established
  CisEstablishedParameters cis_params_;

  // Handle assigned by the controller
  hci_spec::ConnectionHandle cis_hci_handle_;

  // Called after HCI_LE_CIS_Established event is received and handled
  CisEstablishedCallback cis_established_cb_;

  // Called when stream is closed
  pw::Callback<void()> on_closed_cb_;

  hci::CommandChannel::WeakPtr cmd_;

  hci::CommandChannel::EventHandlerId cis_established_handler_;

  WeakSelf<IsoStream> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(IsoStream);
};

}  // namespace bt::iso
