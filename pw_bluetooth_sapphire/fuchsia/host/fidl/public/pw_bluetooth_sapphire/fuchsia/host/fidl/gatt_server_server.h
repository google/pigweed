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

#include <fuchsia/bluetooth/gatt/cpp/fidl.h>

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/server_base.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/local_service_manager.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/types.h"

namespace bthost {

// Implements the gatt::Server FIDL interface.
class GattServerServer
    : public GattServerBase<fuchsia::bluetooth::gatt::Server> {
 public:
  // |adapter_manager| is used to lazily request a handle to the corresponding
  // adapter. It MUST out-live this GattServerServer instance.
  GattServerServer(
      bt::gatt::GATT::WeakPtr gatt,
      fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Server> request);

  ~GattServerServer() override;

  // Removes the service with the given |id| if it is known.
  // This can be called as a result of FIDL connection errors (such as handle
  // closure) or as a result of gatt.Service.RemoveService().
  void RemoveService(uint64_t id);

 private:
  class LocalServiceImpl;

  // ::fuchsia::bluetooth::gatt::Server overrides:
  void PublishService(
      fuchsia::bluetooth::gatt::ServiceInfo service_info,
      fidl::InterfaceHandle<fuchsia::bluetooth::gatt::LocalServiceDelegate>
          delegate,
      fidl::InterfaceRequest<fuchsia::bluetooth::gatt::LocalService>
          service_iface,
      PublishServiceCallback callback) override;

  // Called when a remote device issues a read request to one of our services.
  void OnReadRequest(bt::gatt::IdType service_id,
                     bt::gatt::IdType id,
                     uint16_t offset,
                     bt::gatt::ReadResponder responder);

  // Called when a remote device issues a write request to one of our services.
  void OnWriteRequest(bt::gatt::IdType service_id,
                      bt::gatt::IdType id,
                      uint16_t offset,
                      const bt::ByteBuffer& value,
                      bt::gatt::WriteResponder responder);

  // Called when a remote device has configured notifications or indications on
  // a local characteristic.
  void OnCharacteristicConfig(bt::gatt::IdType service_id,
                              bt::gatt::IdType chrc_id,
                              bt::gatt::PeerId peer_id,
                              bool notify,
                              bool indicate);

  // The mapping between service identifiers and FIDL Service implementations.
  std::unordered_map<uint64_t, std::unique_ptr<LocalServiceImpl>> services_;

  // Keep this as the last member to make sure that all weak pointers are
  // invalidated before other members get destroyed.
  WeakSelf<GattServerServer> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattServerServer);
};

}  // namespace bthost
