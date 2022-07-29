// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_SERVER_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_SERVER_SERVER_H_

#include <fuchsia/bluetooth/gatt2/cpp/fidl.h>

#include <fbl/macros.h>

#include "lib/zx/eventpair.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/gatt2_server_ids.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Implements the gatt2::Server FIDL interface.
// TODO(fxbug.dev/103855): Support sending gatt2::LocalService::PeerUpdate.
// TODO(fxbug.dev/685): Support GATT service includes.
// TODO(fxbug.dev/98598): Support OnSuppressDiscovery
class Gatt2ServerServer : public GattServerBase<fuchsia::bluetooth::gatt2::Server> {
 public:
  // Arbitrary; we only refresh credits when the peer starts to get low.
  // The current implementation does not support a value of 0.
  static const uint8_t REFRESH_CREDITS_AT = 3;

  // |gatt| - The GATT instance underlying this Server.
  // |request| - The FIDL request.
  Gatt2ServerServer(fxl::WeakPtr<bt::gatt::GATT> gatt,
                    fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::Server> request);

  ~Gatt2ServerServer() override;

 private:
  struct Service {
    // The LocalService FIDL proxy
    fidl::InterfacePtr<fuchsia::bluetooth::gatt2::LocalService> local_svc_ptr;

    // The credits available for this LocalService
    int16_t credits = fuchsia::bluetooth::gatt2::INITIAL_VALUE_CHANGED_CREDITS;
  };

  // ::fuchsia::bluetooth::gatt2::Server overrides:
  void PublishService(fuchsia::bluetooth::gatt2::ServiceInfo info,
                      fidl::InterfaceHandle<fuchsia::bluetooth::gatt2::LocalService> service,
                      PublishServiceCallback callback) override;

  // Removes the service with the given |id| if it is known, usually as a result of FIDL connection
  // errors (such as handle closure).
  void RemoveService(InternalServiceId id);

  // Handles the ::fuchsia::bluetooth:gatt2::Server OnSuppressDiscovery event.
  void OnSuppressDiscovery(InternalServiceId service_id);

  // If the update has the required fields and there are credits available, subtracts a credit from
  // the service and returns true. Otherwise, returns false.
  bool ValidateValueChangedEvent(InternalServiceId service_id,
                                 const fuchsia::bluetooth::gatt2::ValueChangedParameters& update,
                                 const char* update_type);

  // Handles the ::fuchsia::bluetooth:gatt2::Server OnNotifyValue event.
  void OnNotifyValue(InternalServiceId service_id,
                     fuchsia::bluetooth::gatt2::ValueChangedParameters update);

  // Handles the ::fuchsia::bluetooth:gatt2::Server OnSuppressDiscovery event.
  void OnIndicateValue(InternalServiceId service_id,
                       fuchsia::bluetooth::gatt2::ValueChangedParameters update,
                       zx::eventpair confirmation);

  // Called when a remote device issues a read request to one of our services.
  void OnReadRequest(bt::PeerId peer_id, bt::gatt::IdType service_id, bt::gatt::IdType id,
                     uint16_t offset, bt::gatt::ReadResponder responder);

  // Called when a remote device issues a write request to one of our services.
  void OnWriteRequest(bt::PeerId peer_id, bt::gatt::IdType service_id, bt::gatt::IdType id,
                      uint16_t offset, const bt::ByteBuffer& value,
                      bt::gatt::WriteResponder responder);

  // Called when a remote device has configured notifications or indications on a local
  // characteristic.
  void OnClientCharacteristicConfiguration(bt::gatt::IdType service_id, bt::gatt::IdType chrc_id,
                                           bt::PeerId peer_id, bool notify, bool indicate);

  // Subtract one credit from the client, potentially refreshing the credits to the client.
  static void SubtractCredit(Service& svc);

  // The mapping between internal service identifiers and FIDL Service implementations.
  std::unordered_map<InternalServiceId, Service> services_;

  // Mapping between client-provided Service IDs and internally-generated IDs.
  // TODO(fxbug.dev/685): This will be necessary for supporting service includes.
  std::unordered_map<ClientServiceId, InternalServiceId> service_id_mapping_;

  // Keep this as the last member to make sure that all weak pointers are invalidated before other
  // members get destroyed.
  fxl::WeakPtrFactory<Gatt2ServerServer> weak_ptr_factory_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Gatt2ServerServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_SERVER_SERVER_H_
