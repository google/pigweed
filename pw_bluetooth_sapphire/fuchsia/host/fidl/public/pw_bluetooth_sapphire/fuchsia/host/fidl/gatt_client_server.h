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

#include "lib/fidl/cpp/binding.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/gatt_remote_service_server.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/server_base.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"

namespace bthost {

// Implements the gatt::Client FIDL interface.
class GattClientServer
    : public GattServerBase<fuchsia::bluetooth::gatt::Client> {
 public:
  GattClientServer(
      bt::gatt::PeerId peer_id,
      bt::gatt::GATT::WeakPtr gatt,
      fidl::InterfaceRequest<fuchsia::bluetooth::gatt::Client> request);
  ~GattClientServer() override = default;

 private:
  // bluetooth::gatt::Client overrides:
  void ListServices(::fidl::VectorPtr<::std::string> uuids,
                    ListServicesCallback callback) override;
  void ConnectToService(
      uint64_t id,
      ::fidl::InterfaceRequest<fuchsia::bluetooth::gatt::RemoteService> request)
      override;

  // The ID of the peer that this client is attached to.
  bt::gatt::PeerId peer_id_;

  // Remote GATT services that were connected through this client. The value can
  // be null while a ConnectToService request is in progress.
  std::unordered_map<uint64_t, std::unique_ptr<GattRemoteServiceServer>>
      connected_services_;

  WeakSelf<GattClientServer> weak_self_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattClientServer);
};

}  // namespace bthost
