// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <fuchsia/bluetooth/gatt/cpp/fidl.h>

#include <unordered_map>

#include "lib/fidl/cpp/binding.h"
#include "pw_bluetooth_sapphire/fuchsia/host/fidl/server_base.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/common/weak_self.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/gatt.h"

namespace bthost {

// Implements the gatt::RemoteService FIDL interface.
class GattRemoteServiceServer
    : public GattServerBase<fuchsia::bluetooth::gatt::RemoteService> {
 public:
  GattRemoteServiceServer(
      bt::gatt::RemoteService::WeakPtr service,
      bt::gatt::GATT::WeakPtr gatt,
      bt::PeerId peer_id,
      fidl::InterfaceRequest<fuchsia::bluetooth::gatt::RemoteService> request);
  ~GattRemoteServiceServer() override;

 private:
  // fuchsia::bluetooth::gatt::RemoteService overrides:
  void DiscoverCharacteristics(
      DiscoverCharacteristicsCallback callback) override;
  void ReadCharacteristic(uint64_t id,
                          ReadCharacteristicCallback callback) override;
  void ReadLongCharacteristic(uint64_t id,
                              uint16_t offset,
                              uint16_t max_bytes,
                              ReadLongCharacteristicCallback callback) override;
  void WriteCharacteristic(uint64_t id,
                           ::std::vector<uint8_t> value,
                           WriteCharacteristicCallback callback) override;
  void WriteLongCharacteristic(
      uint64_t id,
      uint16_t offset,
      ::std::vector<uint8_t> value,
      fuchsia::bluetooth::gatt::WriteOptions write_options,
      WriteCharacteristicCallback callback) override;
  void WriteCharacteristicWithoutResponse(
      uint64_t id, ::std::vector<uint8_t> value) override;
  void ReadDescriptor(uint64_t id, ReadDescriptorCallback callback) override;
  void ReadLongDescriptor(uint64_t id,
                          uint16_t offset,
                          uint16_t max_bytes,
                          ReadLongDescriptorCallback callback) override;
  void WriteDescriptor(uint64_t _id,
                       ::std::vector<uint8_t> value,
                       WriteDescriptorCallback callback) override;
  void WriteLongDescriptor(uint64_t _id,
                           uint16_t offset,
                           ::std::vector<uint8_t> value,
                           WriteDescriptorCallback callback) override;
  void ReadByType(fuchsia::bluetooth::Uuid uuid,
                  ReadByTypeCallback callback) override;
  void NotifyCharacteristic(uint64_t id,
                            bool enable,
                            NotifyCharacteristicCallback callback) override;

  // The remote GATT service that backs this service.
  bt::gatt::RemoteService::WeakPtr service_;

  const bt::PeerId peer_id_;

  using HandlerId = bt::gatt::IdType;
  std::unordered_map<bt::gatt::CharacteristicHandle, HandlerId>
      notify_handlers_;

  WeakSelf<GattRemoteServiceServer> weak_self_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattRemoteServiceServer);
};

}  // namespace bthost
