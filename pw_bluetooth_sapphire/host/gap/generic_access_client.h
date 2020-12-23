// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_CLIENT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_CLIENT_H_

#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"

namespace bt::gap::internal {

// This is a helper for reading characteristics of a remote Generic Access GATT service.
// Characteristics are not cached and read requests are not multiplexed because this is already
// taken care of in gatt::RemoteService.
// Destroying GenericAccessClient will cancel any read requests and callbacks will not be called.
class GenericAccessClient {
 public:
  // |peer_id| is the id of the peer serving the service.
  // The UUID of |generic_access_service| must be kGenericAccessService.
  GenericAccessClient(PeerId peer_id, fbl::RefPtr<gatt::RemoteService> generic_access_service);

  // Discover and read the peripheral preferred connections characteristic, if present.
  using ConnectionParametersCallback =
      fit::callback<void(fit::result<hci::LEPreferredConnectionParameters, att::Status>)>;
  void ReadPeripheralPreferredConnectionParameters(ConnectionParametersCallback callback);

 private:
  fbl::RefPtr<gatt::RemoteService> service_;
  PeerId peer_id_;
  fxl::WeakPtrFactory<GenericAccessClient> weak_ptr_factory_;
};

}  // namespace bt::gap::internal

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_GENERIC_ACCESS_CLIENT_H_
