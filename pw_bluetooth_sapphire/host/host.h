// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_H_

#include <fuchsia/hardware/bt/hci/c/banjo.h>
#include <fuchsia/hardware/bt/vendor/c/banjo.h>
#include <lib/fit/function.h>
#include <lib/fit/thread_checker.h>
#include <zircon/types.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/gap/adapter.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/lib/fxl/memory/ref_counted.h"

namespace bthost {

class HostServer;

// Host is the top-level object of this driver and it is responsible for
// managing the host subsystem stack. It owns the core gap::Adapter object, and
// the FIDL server implementations. A Host's core responsibility is to relay
// messages from the devhost environment to the stack.
//
// The Host initializes 3 distinct serialization domains (with dedicated
// threads) in which the core Bluetooth tasks are processed:
//
// - GAP: The thread the host is created on. This thread handles:
//     * GAP-related FIDL control messaging,
//     * HCI command/event processing (gap::Adapter),
//     * L2CAP fixed channel protocols that are handled internally (e.g. SMP),
//       except for the signaling channels.
//
// - Data:
//     * L2CAP
//     * RFCOMM
//     * Sockets
//
// - GATT:
//     * All GATT FIDL messages
//     * All ATT protocol processing
//
// THREAD SAFETY: This class IS NOT thread-safe. All of its public methods
// should be called on the Host thread only.
class Host final : public fxl::RefCountedThreadSafe<Host> {
 public:
  // Initializes the system and reports the status in |success|.
  using InitCallback = fit::function<void(bool success)>;
  bool Initialize(inspect::Node& root_node, InitCallback callback);

  // Shuts down all systems.
  void ShutDown();

  // Binds the given |channel| to a Host FIDL interface server.
  void BindHostInterface(zx::channel channel);

  // Returns a pointer to GATT. Must not be called after ShutDown().
  bt::gatt::GATT* gatt() const { return gatt_.get(); }

 private:
  FRIEND_MAKE_REF_COUNTED(Host);
  FRIEND_REF_COUNTED_THREAD_SAFE(Host);

  explicit Host(const bt_hci_protocol_t& hci_proto,
                std::optional<bt_vendor_protocol_t> vendor_proto);
  ~Host();

  bt_hci_protocol_t hci_proto_;
  std::optional<bt_vendor_protocol_t> vendor_proto_;

  std::unique_ptr<bt::hci::Transport> hci_;

  std::unique_ptr<bt::gap::Adapter> gap_;

  // The GATT profile layer and bus.
  std::unique_ptr<bt::gatt::GATT> gatt_;

  // Currently connected Host interface handle. A Host allows only one of these
  // to be connected at a time.
  std::unique_ptr<HostServer> host_server_;

  fit::thread_checker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Host);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_HOST_H_
