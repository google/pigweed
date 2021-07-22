// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_REMOTE_SERVICE_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_REMOTE_SERVICE_SERVER_H_

#include <fuchsia/bluetooth/gatt2/cpp/fidl.h>

#include <fbl/macros.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bthost {

class Gatt2RemoteServiceServer : public GattServerBase<fuchsia::bluetooth::gatt2::RemoteService> {
 public:
  Gatt2RemoteServiceServer(
      fbl::RefPtr<bt::gatt::RemoteService> service, fxl::WeakPtr<bt::gatt::GATT> gatt,
      bt::PeerId peer_id, fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::RemoteService> request);
  ~Gatt2RemoteServiceServer() override = default;

 private:
  // fuchsia::bluetooth::gatt2::RemoteService overrides:
  void DiscoverCharacteristics(DiscoverCharacteristicsCallback callback) override;

  void ReadByType(::fuchsia::bluetooth::Uuid uuid, ReadByTypeCallback callback) override;

  void ReadCharacteristic(::fuchsia::bluetooth::gatt2::Handle handle,
                          ::fuchsia::bluetooth::gatt2::ReadOptions options,
                          ReadCharacteristicCallback callback) override;

  void WriteCharacteristic(::fuchsia::bluetooth::gatt2::Handle handle, ::std::vector<uint8_t> value,
                           ::fuchsia::bluetooth::gatt2::WriteOptions options,
                           WriteCharacteristicCallback callback) override {}

  void ReadDescriptor(::fuchsia::bluetooth::gatt2::Handle handle,
                      ::fuchsia::bluetooth::gatt2::ReadOptions options,
                      ReadDescriptorCallback callback) override {}

  void WriteDescriptor(::fuchsia::bluetooth::gatt2::Handle handle, ::std::vector<uint8_t> value,
                       ::fuchsia::bluetooth::gatt2::WriteOptions options,
                       WriteDescriptorCallback callback) override {}

  void RegisterCharacteristicNotifier(
      ::fuchsia::bluetooth::gatt2::Handle handle,
      ::fidl::InterfaceHandle<::fuchsia::bluetooth::gatt2::CharacteristicNotifier> notifier,
      RegisterCharacteristicNotifierCallback callback) override {}

  // The remote GATT service that backs this service.
  fbl::RefPtr<bt::gatt::RemoteService> service_;

  // The peer that is serving this service.
  bt::PeerId peer_id_;

  fxl::WeakPtrFactory<Gatt2RemoteServiceServer> weak_ptr_factory_;
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_REMOTE_SERVICE_SERVER_H_
