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

void GenericAccessClient::ReadDeviceName(DeviceNameCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  service_->DiscoverCharacteristics(
      [self, cb = std::move(callback)](att::Result<> result,
                                       const gatt::CharacteristicMap& chars) mutable {
        if (!self) {
          return;
        }

        if (result.is_error()) {
          cb(result.take_error());
          return;
        }

        std::optional<gatt::CharacteristicHandle> device_name_value_handle;
        for (auto& [handle, chr] : chars) {
          auto& data = chr.first;
          if (data.type == kDeviceNameCharacteristic) {
            device_name_value_handle.emplace(data.value_handle);
            break;
          }
        }

        if (!device_name_value_handle) {
          bt_log(DEBUG, "gap-le",
                 "GAP service does not have device name characteristic "
                 "(peer: %s)",
                 bt_str(self->peer_id_));
          cb(ToResult(HostError::kNotFound).take_error());
          return;
        }

        // according to Core Spec v5.3, Vol 3, Part C, 12.1: "0 to 248 octets in length"
        self->service_->ReadLongCharacteristic(
            *device_name_value_handle, /*offset=*/0, att::kMaxAttributeValueLength,
            [self, cb = std::move(cb)](att::Result<> result, const ByteBuffer& buffer,
                                       bool /*maybe_truncated*/) mutable {
              if (!self) {
                return;
              }

              if (bt_is_error(result, DEBUG, "gap-le",
                              "error reading device name characteristic (peer: %s)",
                              bt_str(self->peer_id_))) {
                cb(result.take_error());
                return;
              }

              const auto device_name_end = std::find(buffer.begin(), buffer.end(), '\0');
              cb(fitx::ok(std::string(buffer.begin(), device_name_end)));
            });
      });
}

void GenericAccessClient::ReadAppearance(AppearanceCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  service_->DiscoverCharacteristics([self, cb = std::move(callback)](
                                        att::Result<> result,
                                        const gatt::CharacteristicMap& chars) mutable {
    if (!self) {
      return;
    }

    if (result.is_error()) {
      cb(result.take_error());
      return;
    }

    std::optional<gatt::CharacteristicHandle> appearance_value_handle;
    for (auto& [handle, chr] : chars) {
      auto& data = chr.first;
      if (data.type == kAppearanceCharacteristic) {
        appearance_value_handle.emplace(data.value_handle);
        break;
      }
    }

    if (!appearance_value_handle) {
      bt_log(DEBUG, "gap-le",
             "GAP service does not have appearance characteristic "
             "(peer: %s)",
             bt_str(self->peer_id_));
      cb(ToResult(HostError::kNotFound).take_error());
      return;
    }

    // according to Core Spec v5.3, Vol 3, Part C, 12.2: "2 octets in length"
    self->service_->ReadCharacteristic(
        *appearance_value_handle,
        [self, cb = std::move(cb)](att::Result<> result, const ByteBuffer& buffer,
                                   bool /*maybe_truncated*/) mutable {
          if (!self) {
            return;
          }

          if (bt_is_error(result, DEBUG, "gap-le",
                          "error reading appearance characteristic (peer: %s)",
                          bt_str(self->peer_id_))) {
            cb(result.take_error());
            return;
          }

          if (buffer.size() != sizeof(uint16_t)) {
            bt_log(DEBUG, "gap-le", "appearance characteristic has invalid value size (peer: %s)",
                   bt_str(self->peer_id_));
            cb(ToResult(HostError::kPacketMalformed).take_error());
            return;
          }

          uint16_t char_value = letoh16(buffer.template To<uint16_t>());
          cb(fitx::ok(char_value));
        });
  });
}

void GenericAccessClient::ReadPeripheralPreferredConnectionParameters(
    ConnectionParametersCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  service_->DiscoverCharacteristics([self, cb = std::move(callback)](
                                        att::Result<> result,
                                        const gatt::CharacteristicMap& chars) mutable {
    if (!self) {
      return;
    }

    if (result.is_error()) {
      cb(result.take_error());
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
      cb(ToResult(HostError::kNotFound).take_error());
      return;
    }

    self->service_->ReadCharacteristic(
        *conn_params_value_handle,
        [self, cb = std::move(cb)](att::Result<> result, const ByteBuffer& buffer,
                                   bool /*maybe_truncated*/) mutable {
          if (!self) {
            return;
          }

          if (bt_is_error(result, DEBUG, "gap-le",
                          "error reading peripheral preferred connection parameters characteristic "
                          "(peer: %s)",
                          bt_str(self->peer_id_))) {
            cb(result.take_error());
            return;
          }

          if (buffer.size() != sizeof(PeripheralPreferredConnectionParametersCharacteristicValue)) {
            bt_log(
                DEBUG, "gap-le",
                "peripheral preferred connection parameters characteristic has invalid value size "
                "(peer: %s)",
                bt_str(self->peer_id_));
            cb(ToResult(HostError::kPacketMalformed).take_error());
            return;
          }

          auto char_value =
              buffer.template To<PeripheralPreferredConnectionParametersCharacteristicValue>();
          hci_spec::LEPreferredConnectionParameters params(
              letoh16(char_value.min_interval), letoh16(char_value.max_interval),
              letoh16(char_value.max_latency), letoh16(char_value.supervision_timeout));

          cb(fitx::ok(params));
        });
  });
}

}  // namespace bt::gap::internal
