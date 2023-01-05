// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_CLIENT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_CLIENT_H_

#include "src/connectivity/bluetooth/core/bt-host/common/weak_self.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"

namespace bt::gap::internal {

// This is a helper for reading characteristics of a remote Generic Access GATT service.
// Characteristics are not cached and read requests are not multiplexed because this is already
// taken care of in gatt::RemoteService.
// Destroying GenericAccessClient will cancel any read requests and callbacks will not be called.
class GenericAccessClient : private WeakSelf<GenericAccessClient> {
 public:
  // |peer_id| is the id of the peer serving the service.
  // The UUID of |generic_access_service| must be kGenericAccessService.
  GenericAccessClient(PeerId peer_id, gatt::RemoteService::WeakPtr generic_access_service);

  // Discover and read the device name characteristic, if present.
  using DeviceNameCallback = fit::callback<void(att::Result<std::string>)>;
  void ReadDeviceName(DeviceNameCallback callback);

  // Discover and read the appearance characteristic, if present.
  using AppearanceCallback = fit::callback<void(att::Result<uint16_t>)>;
  void ReadAppearance(AppearanceCallback callback);

  // Discover and read the peripheral preferred connections characteristic, if present.
  using ConnectionParametersCallback =
      fit::callback<void(att::Result<hci_spec::LEPreferredConnectionParameters>)>;
  void ReadPeripheralPreferredConnectionParameters(ConnectionParametersCallback callback);

 private:
  gatt::RemoteService::WeakPtr service_;
  PeerId peer_id_;
};

}  // namespace bt::gap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_CLIENT_H_
