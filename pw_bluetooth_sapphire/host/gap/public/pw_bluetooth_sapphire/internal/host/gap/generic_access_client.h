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

#pragma once
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/remote_service.h"

namespace bt::gap::internal {

// This is a helper for reading characteristics of a remote Generic Access GATT
// service. Characteristics are not cached and read requests are not multiplexed
// because this is already taken care of in gatt::RemoteService. Destroying
// GenericAccessClient will cancel any read requests and callbacks will not be
// called.
class GenericAccessClient : private WeakSelf<GenericAccessClient> {
 public:
  // |peer_id| is the id of the peer serving the service.
  // The UUID of |generic_access_service| must be kGenericAccessService.
  GenericAccessClient(PeerId peer_id,
                      gatt::RemoteService::WeakPtr generic_access_service);

  // Discover and read the device name characteristic, if present.
  using DeviceNameCallback = fit::callback<void(att::Result<std::string>)>;
  void ReadDeviceName(DeviceNameCallback callback);

  // Discover and read the appearance characteristic, if present.
  using AppearanceCallback = fit::callback<void(att::Result<uint16_t>)>;
  void ReadAppearance(AppearanceCallback callback);

  // Discover and read the peripheral preferred connections characteristic, if
  // present.
  using ConnectionParametersCallback = fit::callback<void(
      att::Result<hci_spec::LEPreferredConnectionParameters>)>;
  void ReadPeripheralPreferredConnectionParameters(
      ConnectionParametersCallback callback);

 private:
  gatt::RemoteService::WeakPtr service_;
  PeerId peer_id_;
};

}  // namespace bt::gap::internal
