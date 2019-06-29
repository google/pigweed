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

  if (!remote_service_watcher_dispatcher_) {
    remote_service_watcher_(peer_id, std::move(service));
  } else {
    async::PostTask(remote_service_watcher_dispatcher_,
                    [watcher = remote_service_watcher_.share(), peer_id, svc = std::move(service)] {
                      watcher(peer_id, std::move(svc));
                    });
  }
}
void FakeLayer::Initialize() {
  // TODO: implement
}

void FakeLayer::ShutDown() {
  // TODO: implement
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

void FakeLayer::DiscoverServices(PeerId peer_id) {
  // TODO: implement
}

void FakeLayer::RegisterRemoteServiceWatcher(RemoteServiceWatcher callback,
                                             async_dispatcher_t* dispatcher) {
  remote_service_watcher_ = std::move(callback);
  remote_service_watcher_dispatcher_ = dispatcher;
}

void FakeLayer::ListServices(PeerId peer_id, std::vector<UUID> uuids,
                             ServiceListCallback callback) {
  // TODO: implement
}

void FakeLayer::FindService(PeerId peer_id, IdType service_id, RemoteServiceCallback callback) {
  // TODO: implement
}

}  // namespace testing
}  // namespace gatt
}  // namespace bt
