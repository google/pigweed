// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CONNECTION_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CONNECTION_SERVER_H_

#include <fuchsia/bluetooth/le/cpp/fidl.h>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/low_energy_connection_handle.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

class LowEnergyConnectionServer : public ServerBase<fuchsia::bluetooth::le::Connection> {
 public:
  LowEnergyConnectionServer(std::unique_ptr<bt::gap::LowEnergyConnectionHandle> connection,
                            zx::channel handle);

  // Assign a callback that signals the invalidation of this connection instance. This can be called
  // in response to the client closing its end of the FIDL channel or when the LL connection is
  // severed. |callback| will be called at most once in response to either of these events. The
  // owner of the LowEnergyConnectionServer instance is expected to destroy it.
  void set_closed_handler(fit::closure callback) { closed_handler_ = std::move(callback); }

  // Return a reference to the underlying connection ref. Expected to only be used for testing.
  const bt::gap::LowEnergyConnectionHandle* conn() const { return conn_.get(); }

 private:
  void OnClosed();

  std::unique_ptr<bt::gap::LowEnergyConnectionHandle> conn_;
  fit::closure closed_handler_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(LowEnergyConnectionServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_LOW_ENERGY_CONNECTION_SERVER_H_
