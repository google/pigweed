// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_REMOTE_SERVICE_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_REMOTE_SERVICE_SERVER_H_

#include <fuchsia/bluetooth/gatt2/cpp/fidl.h>

#include "lib/fidl/cpp/binding.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

namespace bthost {

class Gatt2RemoteServiceServer : public GattServerBase<fuchsia::bluetooth::gatt2::RemoteService> {
 public:
  // The maximum number of pending notification values per CharacteristicNotifier (for flow
  // control). If exceeded, the notifier protocol is closed.
  static const size_t kMaxPendingNotifierValues = 20;

  Gatt2RemoteServiceServer(
      bt::gatt::RemoteService::WeakPtr service, bt::gatt::GATT::WeakPtr gatt, bt::PeerId peer_id,
      fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::RemoteService> request);
  ~Gatt2RemoteServiceServer() override;

  void Close(zx_status_t status);

 private:
  using NotifierId = uint64_t;

  struct CharacteristicNotifier {
    bt::gatt::IdType handler_id;
    bt::gatt::CharacteristicHandle characteristic_handle;
    fidl::InterfacePtr<fuchsia::bluetooth::gatt2::CharacteristicNotifier> notifier;
    // For flow control, values are only sent when the client responds to the previous value with an
    // acknowledgement. This variable stores the queued values.
    std::queue<fuchsia::bluetooth::gatt2::ReadValue> queued_values;
    // `last_value_ack` defaults to true so that the first notification queued up is sent to the
    // FIDL client immediately.
    bool last_value_ack = true;
  };

  // fuchsia::bluetooth::gatt2::RemoteService overrides:
  void DiscoverCharacteristics(DiscoverCharacteristicsCallback callback) override;

  void ReadByType(::fuchsia::bluetooth::Uuid uuid, ReadByTypeCallback callback) override;

  void ReadCharacteristic(::fuchsia::bluetooth::gatt2::Handle handle,
                          ::fuchsia::bluetooth::gatt2::ReadOptions options,
                          ReadCharacteristicCallback callback) override;

  void WriteCharacteristic(::fuchsia::bluetooth::gatt2::Handle handle, ::std::vector<uint8_t> value,
                           ::fuchsia::bluetooth::gatt2::WriteOptions options,
                           WriteCharacteristicCallback callback) override;

  void ReadDescriptor(::fuchsia::bluetooth::gatt2::Handle handle,
                      ::fuchsia::bluetooth::gatt2::ReadOptions options,
                      ReadDescriptorCallback callback) override;

  void WriteDescriptor(::fuchsia::bluetooth::gatt2::Handle handle, ::std::vector<uint8_t> value,
                       ::fuchsia::bluetooth::gatt2::WriteOptions options,
                       WriteDescriptorCallback callback) override;

  void RegisterCharacteristicNotifier(
      ::fuchsia::bluetooth::gatt2::Handle handle,
      ::fidl::InterfaceHandle<::fuchsia::bluetooth::gatt2::CharacteristicNotifier> notifier,
      RegisterCharacteristicNotifierCallback callback) override;

  // Send the next notifier value in the queue if the client acknowledged the previous value.
  void MaybeNotifyNextValue(NotifierId notifier_id);

  void OnCharacteristicNotifierError(NotifierId notifier_id,
                                     bt::gatt::CharacteristicHandle char_handle,
                                     bt::gatt::IdType handler_id);

  // The remote GATT service that backs this service.
  bt::gatt::RemoteService::WeakPtr service_;

  NotifierId next_notifier_id_ = 0u;
  std::unordered_map<NotifierId, CharacteristicNotifier> characteristic_notifiers_;

  // The peer that is serving this service.
  bt::PeerId peer_id_;

  WeakSelf<Gatt2RemoteServiceServer> weak_self_;
};

}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_REMOTE_SERVICE_SERVER_H_
