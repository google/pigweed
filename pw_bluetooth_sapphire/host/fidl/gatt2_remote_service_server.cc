// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt2_remote_service_server.h"

#include <measure_tape/hlcpp/hlcpp_measure_tape_for_read_by_type_result.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/helpers.h"

namespace fbg = fuchsia::bluetooth::gatt2;
namespace measure_fbg = measure_tape::fuchsia::bluetooth::gatt2;

namespace bthost {
namespace {

fbg::Characteristic CharacteristicToFidl(
    const bt::gatt::CharacteristicData& characteristic,
    const std::map<bt::gatt::DescriptorHandle, bt::gatt::DescriptorData>& descriptors) {
  fbg::Characteristic fidl_char;
  fidl_char.set_handle(fbg::Handle{characteristic.value_handle});
  fidl_char.set_type(fuchsia::bluetooth::Uuid{characteristic.type.value()});

  // The FIDL property bitfield combines the properties and extended properties bits.
  // We mask away the kExtendedProperties property.
  constexpr uint8_t kRemoveExtendedPropertiesMask = 0x7F;
  uint16_t fidl_properties =
      static_cast<uint16_t>(characteristic.properties & kRemoveExtendedPropertiesMask);
  if (characteristic.extended_properties) {
    if (*characteristic.extended_properties & bt::gatt::ExtendedProperty::kReliableWrite) {
      fidl_properties |= static_cast<uint16_t>(fbg::CharacteristicPropertyBits::RELIABLE_WRITE);
    }
    if (*characteristic.extended_properties & bt::gatt::ExtendedProperty::kWritableAuxiliaries) {
      fidl_properties |=
          static_cast<uint16_t>(fbg::CharacteristicPropertyBits::WRITABLE_AUXILIARIES);
    }
  }
  fidl_char.set_properties(static_cast<uint32_t>(fidl_properties));

  if (!descriptors.empty()) {
    std::vector<fbg::Descriptor> fidl_descriptors;
    for (const auto& [handle, data] : descriptors) {
      fbg::Descriptor fidl_descriptor;
      fidl_descriptor.set_handle(fbg::Handle{handle.value});
      fidl_descriptor.set_type(fuchsia::bluetooth::Uuid{data.type.value()});
      fidl_descriptors.push_back(std::move(fidl_descriptor));
    }
    fidl_char.set_descriptors(std::move(fidl_descriptors));
  }

  return fidl_char;
}

bool IsHandleValid(fbg::Handle handle) {
  if (handle.value > std::numeric_limits<bt::att::Handle>::max()) {
    bt_log(ERROR, "fidl", "Invalid 64-bit FIDL GATT ID with `bits[16, 63] != 0` (0x%lX)",
           handle.value);
    return false;
  }
  return true;
}

// Returned result is supposed to match Read{Characteristic, Descriptor}Callback (result type is
// converted by FIDL move constructor).
[[nodiscard]] fit::result<::fuchsia::bluetooth::gatt2::ReadValue,
                          ::fuchsia::bluetooth::gatt2::Error>
ReadResultToFidl(bt::PeerId peer_id, fbg::Handle handle, bt::att::Status status,
                 const bt::ByteBuffer& value, bool maybe_truncated, const char* request) {
  if (bt_is_error(status, INFO, "fidl", "%s: error (peer: %s, handle: 0x%lX)", request,
                  bt_str(peer_id), handle.value)) {
    return fit::error(fidl_helpers::AttStatusToGattFidlError(status));
  }

  fbg::ReadValue fidl_value;
  fidl_value.set_handle(handle);
  fidl_value.set_value(value.ToVector());
  fidl_value.set_maybe_truncated(maybe_truncated);
  return fit::ok(std::move(fidl_value));
}

void FillInReadOptionsDefaults(fbg::ReadOptions& options) {
  if (options.is_short_read()) {
    return;
  }
  if (!options.long_read().has_offset()) {
    options.long_read().set_offset(0);
  }
  if (!options.long_read().has_max_bytes()) {
    options.long_read().set_max_bytes(fbg::MAX_VALUE_LENGTH);
  }
}

}  // namespace

Gatt2RemoteServiceServer::Gatt2RemoteServiceServer(
    fbl::RefPtr<bt::gatt::RemoteService> service, fxl::WeakPtr<bt::gatt::GATT> gatt,
    bt::PeerId peer_id, fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::RemoteService> request)
    : GattServerBase(gatt, this, std::move(request)),
      service_(std::move(service)),
      peer_id_(peer_id),
      weak_ptr_factory_(this) {}

void Gatt2RemoteServiceServer::DiscoverCharacteristics(DiscoverCharacteristicsCallback callback) {
  auto res_cb = [callback = std::move(callback)](
                    bt::att::Status status, const bt::gatt::CharacteristicMap& characteristics) {
    if (!status) {
      callback({});
      return;
    }

    std::vector<fbg::Characteristic> fidl_characteristics;
    for (const auto& [_, characteristic] : characteristics) {
      const auto& [data, descriptors] = characteristic;
      fidl_characteristics.push_back(CharacteristicToFidl(data, descriptors));
    }
    callback(std::move(fidl_characteristics));
  };

  service_->DiscoverCharacteristics(std::move(res_cb));
}

void Gatt2RemoteServiceServer::ReadByType(::fuchsia::bluetooth::Uuid uuid,
                                          ReadByTypeCallback callback) {
  service_->ReadByType(
      fidl_helpers::UuidFromFidl(uuid),
      [self = weak_ptr_factory_.GetWeakPtr(), cb = std::move(callback), func = __FUNCTION__](
          bt::att::Status status, std::vector<bt::gatt::RemoteService::ReadByTypeResult> results) {
        if (!self) {
          return;
        }

        switch (status.error()) {
          case bt::HostError::kNoError:
            break;
          case bt::HostError::kInvalidParameters:
            bt_log(WARN, "fidl", "%s: called with invalid parameters (peer: %s)", func,
                   bt_str(self->peer_id_));
            cb(fpromise::error(fbg::Error::INVALID_PARAMETERS));
            return;
          default:
            cb(fpromise::error(fbg::Error::FAILURE));
            return;
        }

        const size_t kVectorOverhead = sizeof(fidl_message_header_t) + sizeof(fidl_vector_t);
        const size_t kMaxBytes = ZX_CHANNEL_MAX_MSG_BYTES - kVectorOverhead;
        size_t bytes_used = 0;

        std::vector<fuchsia::bluetooth::gatt2::ReadByTypeResult> fidl_results;
        fidl_results.reserve(results.size());

        for (const bt::gatt::RemoteService::ReadByTypeResult& result : results) {
          fuchsia::bluetooth::gatt2::ReadByTypeResult fidl_result;
          fidl_result.set_handle(fbg::Handle{result.handle.value});
          if (result.result.is_ok()) {
            fbg::ReadValue read_value;
            read_value.set_handle(fbg::Handle{result.handle.value});
            read_value.set_value(result.result.value()->ToVector());
            read_value.set_maybe_truncated(result.maybe_truncated);
            fidl_result.set_value(std::move(read_value));
          } else {
            fidl_result.set_error(
                fidl_helpers::AttStatusToGattFidlError(bt::att::Status(result.result.error())));
          }

          measure_fbg::Size result_size = measure_fbg::Measure(fidl_result);
          ZX_ASSERT(result_size.num_handles == 0);
          bytes_used += result_size.num_bytes;

          if (bytes_used > kMaxBytes) {
            cb(fpromise::error(fuchsia::bluetooth::gatt2::Error::TOO_MANY_RESULTS));
            return;
          }

          fidl_results.push_back(std::move(fidl_result));
        }

        cb(fpromise::ok(std::move(fidl_results)));
      });
}

void Gatt2RemoteServiceServer::ReadCharacteristic(fbg::Handle fidl_handle, fbg::ReadOptions options,
                                                  ReadCharacteristicCallback callback) {
  if (!IsHandleValid(fidl_handle)) {
    callback(fit::error(fbg::Error::INVALID_HANDLE));
    return;
  }
  bt::gatt::CharacteristicHandle handle(static_cast<bt::att::Handle>(fidl_handle.value));

  FillInReadOptionsDefaults(options);

  const char* kRequestName = __FUNCTION__;
  bt::gatt::RemoteService::ReadValueCallback read_cb =
      [peer_id = peer_id_, fidl_handle, kRequestName, callback = std::move(callback)](
          bt::att::Status status, const bt::ByteBuffer& value, bool maybe_truncated) {
        callback(
            ReadResultToFidl(peer_id, fidl_handle, status, value, maybe_truncated, kRequestName));
      };

  if (options.is_short_read()) {
    service_->ReadCharacteristic(handle, std::move(read_cb));
    return;
  }

  service_->ReadLongCharacteristic(handle, options.long_read().offset(),
                                   options.long_read().max_bytes(), std::move(read_cb));
}

void Gatt2RemoteServiceServer::ReadDescriptor(::fuchsia::bluetooth::gatt2::Handle fidl_handle,
                                              ::fuchsia::bluetooth::gatt2::ReadOptions options,
                                              ReadDescriptorCallback callback) {
  if (!IsHandleValid(fidl_handle)) {
    callback(fit::error(fbg::Error::INVALID_HANDLE));
    return;
  }
  bt::gatt::DescriptorHandle handle(static_cast<bt::att::Handle>(fidl_handle.value));

  FillInReadOptionsDefaults(options);

  const char* kRequestName = __FUNCTION__;
  bt::gatt::RemoteService::ReadValueCallback read_cb =
      [peer_id = peer_id_, fidl_handle, kRequestName, callback = std::move(callback)](
          bt::att::Status status, const bt::ByteBuffer& value, bool maybe_truncated) {
        callback(
            ReadResultToFidl(peer_id, fidl_handle, status, value, maybe_truncated, kRequestName));
      };

  if (options.is_short_read()) {
    service_->ReadDescriptor(handle, std::move(read_cb));
    return;
  }

  service_->ReadLongDescriptor(handle, options.long_read().offset(),
                               options.long_read().max_bytes(), std::move(read_cb));
}

}  // namespace bthost
