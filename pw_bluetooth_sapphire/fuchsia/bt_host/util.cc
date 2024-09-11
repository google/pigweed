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

#include "pw_bluetooth_sapphire/fuchsia/bt_host/util.h"

#include <lib/fdio/directory.h>
#include <lib/zx/channel.h>

#include "pw_bluetooth_sapphire/internal/host/common/log.h"

using namespace bt;

namespace bthost {

zx::result<fidl::ClientEnd<fuchsia_hardware_bluetooth::Vendor>>
CreateVendorHandle(const std::string& device_path) {
  zx::channel client, server;
  zx_status_t status = zx::channel::create(0, &client, &server);
  if (status != ZX_OK) {
    bt_log(WARN,
           "bt-host",
           "Failed to open HCI device: Could not create FIDL channel");
    return zx::error(status);
  }

  status = fdio_service_connect(device_path.c_str(), server.release());
  if (status != ZX_OK) {
    bt_log(WARN,
           "bt-host",
           "Failed to open HCI device: Could not connect to service directory");
    return zx::error(status);
  }

  return zx::ok(
      fidl::ClientEnd<fuchsia_hardware_bluetooth::Vendor>(std::move(client)));
}

}  // namespace bthost
