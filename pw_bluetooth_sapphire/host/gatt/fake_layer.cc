// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_layer.h"

namespace bt {
namespace gatt {
namespace testing {

void FakeLayer::NotifyRemoteService(PeerId peer_id, fbl::RefPtr<RemoteService> service) {
  if (!remote_service_watcher_) {
    return;
  }

  remote_service_watcher_(peer_id, std::move(service));
}

void FakeLayer::AddConnection(PeerId peer_id, fbl::RefPtr<l2cap::Channel> att_chan) {
  // TODO: implement
}

void FakeLayer::RemoveConnection(PeerId peer_id) {
  // TODO: implement
}

void FakeLayer::RegisterService(ServicePtr service, ServiceIdCallback callback,
                                ReadHandler read_handler, WriteHandler write_handler,
                                ClientConfigCallback ccc_callback) {
  // TODO: implement
}

void FakeLayer::UnregisterService(IdType service_id) {
  // TODO: implement
}

void FakeLayer::SendNotification(IdType service_id, IdType chrc_id, PeerId peer_id,
                                 ::std::vector<uint8_t> value, bool indicate) {
  // TODO: implement
}

void FakeLayer::DiscoverServices(PeerId peer_id, std::optional<UUID> optional_service_uuid) {
  if (discover_services_cb_) { discover_services_cb_(peer_id, optional_service_uuid); }
  // TODO: implement the rest
}

void FakeLayer::RegisterRemoteServiceWatcher(RemoteServiceWatcher callback) {
  remote_service_watcher_ = std::move(callback);
}

void FakeLayer::ListServices(PeerId peer_id, std::vector<UUID> uuids,
                             ServiceListCallback callback) {
  // TODO: implement
}

void FakeLayer::FindService(PeerId peer_id, IdType service_id, RemoteServiceCallback callback) {
  // TODO: implement
}

void FakeLayer::SetDiscoverServicesCallback(DiscoverServicesCallback cb) {
 discover_services_cb_ = std::move(cb);
}

}  // namespace testing
}  // namespace gatt
}  // namespace bt
