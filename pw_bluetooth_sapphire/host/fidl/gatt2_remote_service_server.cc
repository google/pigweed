// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt2_remote_service_server.h"

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"

namespace fbg = fuchsia::bluetooth::gatt2;

namespace bthost {
namespace {

fbg::Characteristic CharacteristicToFidl(
    const bt::gatt::CharacteristicData& characteristic,
    const std::map<bt::gatt::DescriptorHandle, bt::gatt::DescriptorData>& descriptors) {
  fbg::Characteristic fidl_char;
  fidl_char.set_handle(fbg::Handle{characteristic.handle});
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

}  // namespace

Gatt2RemoteServiceServer::Gatt2RemoteServiceServer(
    fbl::RefPtr<bt::gatt::RemoteService> service, fxl::WeakPtr<bt::gatt::GATT> gatt,
    bt::PeerId peer_id, fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::RemoteService> request)
    : GattServerBase(gatt, this, std::move(request)), service_(std::move(service)) {}

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

}  // namespace bthost
