// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "generic_access_client.h"

#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"

namespace bt::gap::internal {

GenericAccessClient::GenericAccessClient(PeerId peer_id, fbl::RefPtr<gatt::RemoteService> service)
    : service_(std::move(service)), peer_id_(peer_id), weak_ptr_factory_(this) {
  ZX_ASSERT(service_);
  ZX_ASSERT(service_->uuid() == kGenericAccessService);
}

void GenericAccessClient::ReadPeripheralPreferredConnectionParameters(
    ConnectionParametersCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  service_->DiscoverCharacteristics([self, cb = std::move(callback)](auto status,
                                                                     auto chars) mutable {
    if (!self) {
      return;
    }

    if (!status) {
      cb(fpromise::error(status));
      return;
    }

    std::optional<gatt::CharacteristicHandle> conn_params_value_handle;
    for (auto& [handle, chr] : chars) {
      auto& data = chr.first;
      if (data.type == kPeripheralPreferredConnectionParametersCharacteristic) {
        conn_params_value_handle.emplace(data.value_handle);
        break;
      }
    }

    if (!conn_params_value_handle) {
      bt_log(DEBUG, "gap-le",
             "GAP service does not have peripheral preferred connection parameters characteristic "
             "(peer: %s)",
             bt_str(self->peer_id_));
      cb(fpromise::error(att::Status(HostError::kNotFound)));
      return;
    }

    self->service_->ReadCharacteristic(*conn_params_value_handle, [self, cb = std::move(cb)](
                                                                      auto status, auto& buffer,
                                                                      auto) mutable {
      if (!self) {
        return;
      }

      if (bt_is_error(
              status, DEBUG, "gap-le",
              "error reading peripheral preferred connection parameters characteristic (peer: %s)",
              bt_str(self->peer_id_))) {
        cb(fpromise::error(status));
        return;
      }

      if (buffer.size() != sizeof(PeripheralPreferredConnectionParametersCharacteristicValue)) {
        bt_log(DEBUG, "gap-le",
               "peripheral preferred connection parameters characteristic has invalid value size "
               "(peer: %s)",
               bt_str(self->peer_id_));
        cb(fpromise::error(att::Status(HostError::kPacketMalformed)));
        return;
      }

      auto& char_value =
          buffer.template As<PeripheralPreferredConnectionParametersCharacteristicValue>();
      hci_spec::LEPreferredConnectionParameters params(
          letoh16(char_value.min_interval), letoh16(char_value.max_interval),
          letoh16(char_value.max_latency), letoh16(char_value.supervision_timeout));

      cb(fpromise::ok(params));
    });
  });
}

}  // namespace bt::gap::internal
