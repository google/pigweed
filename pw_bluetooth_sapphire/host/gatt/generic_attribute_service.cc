// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "generic_attribute_service.h"

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/lib/fxl/strings/string_number_conversions.h"

namespace bt::gatt {

GenericAttributeService::GenericAttributeService(LocalServiceManager::WeakPtr local_service_manager,
                                                 SendIndicationCallback send_indication_callback)
    : local_service_manager_(std::move(local_service_manager)),
      send_indication_callback_(std::move(send_indication_callback)) {
  BT_ASSERT(local_service_manager_.is_alive());
  BT_DEBUG_ASSERT(send_indication_callback_);

  Register();
}

GenericAttributeService::~GenericAttributeService() {
  if (local_service_manager_.is_alive() && service_id_ != kInvalidId) {
    local_service_manager_->UnregisterService(service_id_);
  }
}

void GenericAttributeService::Register() {
  const att::AccessRequirements kDisallowed;
  const att::AccessRequirements kAllowedNoSecurity(/*encryption=*/false, /*authentication=*/false,
                                                   /*authorization=*/false);
  CharacteristicPtr service_changed_chr =
      std::make_unique<Characteristic>(kServiceChangedChrcId,                 // id
                                       types::kServiceChangedCharacteristic,  // type
                                       Property::kIndicate,                   // properties
                                       0u,                                    // extended_properties
                                       kDisallowed,                           // read
                                       kDisallowed,                           // write
                                       kAllowedNoSecurity);                   // update
  auto service = std::make_unique<Service>(true, types::kGenericAttributeService);
  service->AddCharacteristic(std::move(service_changed_chr));

  ClientConfigCallback ccc_callback = [this](IdType service_id, IdType chrc_id, PeerId peer_id,
                                             bool notify, bool indicate) {
    BT_DEBUG_ASSERT(chrc_id == 0u);

    // Discover the handle assigned to this characteristic if necessary.
    if (svc_changed_handle_ == att::kInvalidHandle) {
      LocalServiceManager::ClientCharacteristicConfig config;
      if (!local_service_manager_->GetCharacteristicConfig(service_id, chrc_id, peer_id, &config)) {
        bt_log(DEBUG, "gatt", "service: Peer has not configured characteristic: %s",
               bt_str(peer_id));
        return;
      }
      svc_changed_handle_ = config.handle;
    }
    SetServiceChangedIndicationSubscription(peer_id, indicate);
    if (persist_service_changed_ccc_callback_) {
      ServiceChangedCCCPersistedData persisted = {.notify = notify, .indicate = indicate};
      persist_service_changed_ccc_callback_(peer_id, persisted);
    } else {
      bt_log(WARN, "gatt", "Attempted to persist service changed ccc but no callback found.");
    }
  };

  service_id_ = local_service_manager_->RegisterService(std::move(service), NopReadHandler,
                                                        NopWriteHandler, std::move(ccc_callback));
  BT_DEBUG_ASSERT(service_id_ != kInvalidId);

  local_service_manager_->set_service_changed_callback(
      fit::bind_member<&GenericAttributeService::OnServiceChanged>(this));
}

void GenericAttributeService::SetServiceChangedIndicationSubscription(PeerId peer_id,
                                                                      bool indicate) {
  if (indicate) {
    subscribed_peers_.insert(peer_id);
    bt_log(DEBUG, "gatt", "service: Service Changed enabled for peer %s", bt_str(peer_id));
  } else {
    subscribed_peers_.erase(peer_id);
    bt_log(DEBUG, "gatt", "service: Service Changed disabled for peer %s", bt_str(peer_id));
  }
}

void GenericAttributeService::OnServiceChanged(IdType service_id, att::Handle start,
                                               att::Handle end) {
  // Service Changed not yet configured for indication.
  if (svc_changed_handle_ == att::kInvalidHandle) {
    return;
  }

  // Don't send indications for this service's removal.
  if (service_id_ == service_id) {
    return;
  }

  StaticByteBuffer<2 * sizeof(uint16_t)> value;

  value[0] = static_cast<uint8_t>(start);
  value[1] = static_cast<uint8_t>(start >> 8);
  value[2] = static_cast<uint8_t>(end);
  value[3] = static_cast<uint8_t>(end >> 8);

  for (auto peer_id : subscribed_peers_) {
    bt_log(TRACE, "gatt",
           "service: indicating peer %s of service(s) changed "
           "(start: %#.4x, end: %#.4x)",
           bt_str(peer_id), start, end);
    send_indication_callback_(service_id_, kServiceChangedChrcId, peer_id, value.view());
  }
}

}  // namespace bt::gatt
