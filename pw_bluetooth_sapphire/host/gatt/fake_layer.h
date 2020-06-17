// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_H_

#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

namespace bt {
namespace gatt {
namespace testing {

// This is a fake version of the root GATT object that can be injected in unit
// tests.
class FakeLayer final : public GATT {
 public:
  inline static fbl::RefPtr<FakeLayer> Create() { return fbl::AdoptRef(new FakeLayer()); }

  // Notifies the remote service watcher if one is registered.
  void NotifyRemoteService(PeerId peer_id, fbl::RefPtr<RemoteService> service);

  // GATT overrides:
  void Initialize() override;
  void ShutDown() override;
  void AddConnection(PeerId peer_id, fbl::RefPtr<l2cap::Channel> att_chan) override;
  void RemoveConnection(PeerId peer_id) override;
  void RegisterService(ServicePtr service, ServiceIdCallback callback, ReadHandler read_handler,
                       WriteHandler write_handler, ClientConfigCallback ccc_callback) override;
  void UnregisterService(IdType service_id) override;
  void SendNotification(IdType service_id, IdType chrc_id, PeerId peer_id,
                        ::std::vector<uint8_t> value, bool indicate) override;
  void DiscoverServices(PeerId peer_id, std::optional<UUID> optional_service_uuid) override;
  void RegisterRemoteServiceWatcher(RemoteServiceWatcher callback,
                                    async_dispatcher_t* dispatcher) override;
  void ListServices(PeerId peer_id, std::vector<UUID> uuids, ServiceListCallback callback) override;
  void FindService(PeerId peer_id, IdType service_id, RemoteServiceCallback callback) override;

  // Unit test callbacks
  using DiscoverServicesCallback = fit::function<void(PeerId, std::optional<UUID>)>;
  void SetDiscoverServicesCallback(DiscoverServicesCallback cb);

 private:
  friend class fbl::RefPtr<FakeLayer>;
  FakeLayer() = default;
  ~FakeLayer() override = default;

  RemoteServiceWatcher remote_service_watcher_;
  async_dispatcher_t* remote_service_watcher_dispatcher_;

  DiscoverServicesCallback discover_services_cb_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeLayer);
};

}  // namespace testing
}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_LAYER_H_
