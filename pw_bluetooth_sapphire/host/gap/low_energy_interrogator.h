// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_INTERROGATOR_H_

#include "src/connectivity/bluetooth/core/bt-host/gap/interrogator.h"

namespace bt {
namespace hci {
class Transport;
}  // namespace hci

namespace gap {

// LowEnergyInterrogator sends HCI commands that request the controller version and features of a
// peer and handles responses by updating the Peer specified in Interrogator::Start.
// LowEnergyInterrogator must only be used with an LE or dual mode controller.
class LowEnergyInterrogator final : public Interrogator {
 public:
  // |cache| must live longer than this object.
  LowEnergyInterrogator(PeerCache* cache, fxl::WeakPtr<hci::Transport> hci);

 private:
  // Interrogator Overrides:
  void SendCommands(InterrogationRefPtr interrogation) final;

  // LE commands:

  void ReadLERemoteFeatures(InterrogationRefPtr interrogation);

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<LowEnergyInterrogator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(LowEnergyInterrogator);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_LOW_ENERGY_INTERROGATOR_H_
