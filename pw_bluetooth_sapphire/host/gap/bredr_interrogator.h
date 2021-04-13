// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_

#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>
#include <lib/fit/thread_checker.h>

#include <memory>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/interrogator.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/constants.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/command_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/control_packets.h"
#include "src/lib/fxl/functional/cancelable_callback.h"
#include "src/lib/fxl/memory/ref_ptr.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace hci {
class Transport;
}

namespace gap {

// A BrEdrInterrogator abstracts over the HCI commands and events involved
// immediately after connecting to a peer over BR/EDR.
//
// Only one interregator object is expected to exist per controller.
class BrEdrInterrogator final : public Interrogator {
 public:
  // |cache| must live longer than this object.
  BrEdrInterrogator(PeerCache* cache, fxl::WeakPtr<hci::Transport> hci);

 private:
  // Interrogator Overrides:
  void SendCommands(InterrogationRefPtr interrogation) override;

  // BR/EDR commands:

  // Requests the name of the remote peer.
  void MakeRemoteNameRequest(InterrogationRefPtr interrogation);

  // Requests features of |peer|, and asks for Extended Features if they exist.
  void ReadRemoteFeatures(InterrogationRefPtr interrogation);

  // Reads the extended feature page |page| of |peer|.
  void ReadRemoteExtendedFeatures(InterrogationRefPtr interrogation, uint8_t page);

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  fxl::WeakPtrFactory<BrEdrInterrogator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrInterrogator);
};

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_BREDR_INTERROGATOR_H_
