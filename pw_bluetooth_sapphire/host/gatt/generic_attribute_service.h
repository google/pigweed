// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GENERIC_ATTRIBUTE_SERVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GENERIC_ATTRIBUTE_SERVICE_H_

#include <lib/fit/function.h>

#include <unordered_set>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/persisted_data.h"

namespace bt::gatt {
constexpr IdType kServiceChangedChrcId = 0u;

// Callback to send an indication. Used to inject the GATT object's update-sending ability without
// requiring this service to carry a reference to GATT or Server.
//   |chrc_id|: the service-defined ID of the characteristic to indicate
//   |svc_id|: the gatt::GATT-defined ID of the service containing |chrc_id|.
// For example, to indicate a new service to a peer via the Service Changed chrc, one would invoke
// this with svc_id = the GenericAttributeService's service_id_, chrc_id = kServiceChangedChrcId,
// peer_id of the peer, and value = the att::Handle range of the new service.
using SendIndicationCallback =
    fit::function<void(IdType svc_id, IdType chrc_id, PeerId peer_id, BufferView value)>;

// Implements the "Generic Attribute Profile Service" containing the "Service
// Changed" characteristic that is "...used to indicate to connected devices
// that services have changed (Vol 3, Part G, 7)."
class GenericAttributeService final {
 public:
  // Registers this service and makes this service the callee of the Service
  // Changed callback. GATT remote clients must still request that they be sent
  // indications for the Service Changed characteristic. Holds the
  // LocalServiceManager pointer for this object's lifetime. Do not register
  // multiple instances of this service in a single bt-host.
  GenericAttributeService(fxl::WeakPtr<LocalServiceManager> local_service_manager,
                          SendIndicationCallback send_indication_callback);
  ~GenericAttributeService();

  // This callback is called when a client changes the CCC for the service changed
  // characteristic to inform the upper layers of the stack to persist this value.
  void SetPersistServiceChangedCCCCallback(PersistServiceChangedCCCCallback callback) {
    persist_service_changed_ccc_callback_ = std::move(callback);
  }

  // Set the service changed indication subscription for a given peer.
  void SetServiceChangedIndicationSubscription(PeerId peer_id, bool indicate);

  inline IdType service_id() const { return service_id_; }

 private:
  void Register();

  // Send indications to subscribed clients when a service has changed.
  void OnServiceChanged(IdType service_id, att::Handle start, att::Handle end);

  // Data store against which to register and unregister this service. It must
  // outlive this instance.
  fxl::WeakPtr<LocalServiceManager> local_service_manager_;
  const SendIndicationCallback send_indication_callback_;

  // Peers that have subscribed to indications.
  std::unordered_set<PeerId> subscribed_peers_;

  // Handle for the Service Changed characteristic that is read when it is first
  // configured for indications.
  att::Handle svc_changed_handle_ = att::kInvalidHandle;

  // Local service ID; hidden because registration is tied to instance lifetime.
  IdType service_id_ = kInvalidId;

  // Callback to inform uper stack layers to persist service changed CCC.
  PersistServiceChangedCCCCallback persist_service_changed_ccc_callback_;
};

}  // namespace bt::gatt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_GENERIC_ATTRIBUTE_SERVICE_H_
