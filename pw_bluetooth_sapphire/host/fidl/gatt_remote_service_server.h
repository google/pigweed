// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT_REMOTE_SERVICE_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT_REMOTE_SERVICE_SERVER_H_

#include <fuchsia/bluetooth/gatt/cpp/fidl.h>

#include <unordered_map>

#include <fbl/macros.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

// Implements the gatt::RemoteService FIDL interface.
class GattRemoteServiceServer : public GattServerBase<fuchsia::bluetooth::gatt::RemoteService> {
 public:
  GattRemoteServiceServer(fbl::RefPtr<bt::gatt::RemoteService> service,
                          fxl::WeakPtr<bt::gatt::GATT> gatt, bt::PeerId peer_id,
                          fidl::InterfaceRequest<fuchsia::bluetooth::gatt::RemoteService> request);
  ~GattRemoteServiceServer() override;

 private:
  // fuchsia::bluetooth::gatt::RemoteService overrides:
  void DiscoverCharacteristics(DiscoverCharacteristicsCallback callback) override;
  void ReadCharacteristic(uint64_t id, ReadCharacteristicCallback callback) override;
  void ReadLongCharacteristic(uint64_t id, uint16_t offset, uint16_t max_bytes,
                              ReadLongCharacteristicCallback callback) override;
  void WriteCharacteristic(uint64_t id, ::std::vector<uint8_t> value,
                           WriteCharacteristicCallback callback) override;
  void WriteLongCharacteristic(uint64_t id, uint16_t offset, ::std::vector<uint8_t> value,
                               fuchsia::bluetooth::gatt::WriteOptions write_options,
                               WriteCharacteristicCallback callback) override;
  void WriteCharacteristicWithoutResponse(uint64_t id, ::std::vector<uint8_t> value) override;
  void ReadDescriptor(uint64_t id, ReadDescriptorCallback callback) override;
  void ReadLongDescriptor(uint64_t id, uint16_t offset, uint16_t max_bytes,
                          ReadLongDescriptorCallback callback) override;
  void WriteDescriptor(uint64_t _id, ::std::vector<uint8_t> value,
                       WriteDescriptorCallback callback) override;
  void WriteLongDescriptor(uint64_t _id, uint16_t offset, ::std::vector<uint8_t> value,
                           WriteDescriptorCallback callback) override;
  void ReadByType(fuchsia::bluetooth::Uuid uuid, ReadByTypeCallback callback) override;
  void NotifyCharacteristic(uint64_t id, bool enable,
                            NotifyCharacteristicCallback callback) override;

  // The remote GATT service that backs this service.
  fbl::RefPtr<bt::gatt::RemoteService> service_;

  const bt::PeerId peer_id_;

  using HandlerId = bt::gatt::IdType;
  std::unordered_map<bt::gatt::CharacteristicHandle, HandlerId> notify_handlers_;

  fxl::WeakPtrFactory<GattRemoteServiceServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattRemoteServiceServer);
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT_REMOTE_SERVICE_SERVER_H_
