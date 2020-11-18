// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_layer.h"

#include <lib/async/default.h>

namespace bt::gatt::testing {

FakeLayer::TestPeer::TestPeer() : fake_client(async_get_default_dispatcher()) {}

FakeLayer::TestPeer::~TestPeer() {
  for (auto& s : services) {
    s->ShutDown();
  }
}

std::pair<fbl::RefPtr<RemoteService>, fxl::WeakPtr<FakeClient>> FakeLayer::AddPeerService(
    PeerId peer_id, const ServiceData& info, bool notify) {
  auto [iter, _] = peers_.try_emplace(peer_id);
  auto& peer = iter->second;

  auto service = fbl::AdoptRef(
      new RemoteService(info, peer.fake_client.AsWeakPtr(), async_get_default_dispatcher()));
  peer.services.push_back(service);

  if (notify && remote_service_watcher_) {
    remote_service_watcher_(peer_id, service);
  }

  return {service, peer.fake_client.AsFakeWeakPtr()};
}

void FakeLayer::AddConnection(PeerId peer_id, fbl::RefPtr<l2cap::Channel> att_chan) {
  peers_.try_emplace(peer_id);
}

void FakeLayer::RemoveConnection(PeerId peer_id) { peers_.erase(peer_id); }

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

void FakeLayer::SetPersistServiceChangedCCCCallback(PersistServiceChangedCCCCallback callback) {
  if (set_persist_service_changed_ccc_cb_cb_) {
    set_persist_service_changed_ccc_cb_cb_();
  }
  persist_service_changed_ccc_cb_ = std::move(callback);
}

void FakeLayer::SetRetrieveServiceChangedCCCCallback(RetrieveServiceChangedCCCCallback callback) {
  if (set_retrieve_service_changed_ccc_cb_cb_) {
    set_retrieve_service_changed_ccc_cb_cb_();
  }
  retrieve_service_changed_ccc_cb_ = std::move(callback);
}

void FakeLayer::DiscoverServices(PeerId peer_id, std::optional<UUID> optional_service_uuid) {
  if (discover_services_cb_) {
    discover_services_cb_(peer_id, optional_service_uuid);
  }

  auto iter = peers_.find(peer_id);
  if (!remote_service_watcher_ || iter == peers_.end()) {
    return;
  }

  for (auto& s : iter->second.services) {
    if (!optional_service_uuid || s->uuid() == *optional_service_uuid) {
      remote_service_watcher_(peer_id, s);
    }
  }
}

void FakeLayer::RegisterRemoteServiceWatcher(RemoteServiceWatcher callback) {
  remote_service_watcher_ = std::move(callback);
}

void FakeLayer::ListServices(PeerId peer_id, std::vector<UUID> uuids,
                             ServiceListCallback callback) {
  ServiceList services;

  auto iter = peers_.find(peer_id);
  if (iter != peers_.end()) {
    for (auto& s : iter->second.services) {
      auto pred = [&](const UUID& uuid) { return s->uuid() == uuid; };
      if (uuids.empty() || std::find_if(uuids.begin(), uuids.end(), pred) != uuids.end()) {
        services.push_back(s);
      }
    }
  }

  callback(att::Status(), std::move(services));
}

void FakeLayer::FindService(PeerId peer_id, IdType service_id, RemoteServiceCallback callback) {
  // TODO: implement
}

void FakeLayer::SetDiscoverServicesCallback(DiscoverServicesCallback cb) {
  discover_services_cb_ = std::move(cb);
}

void FakeLayer::SetSetPersistServiceChangedCCCCallbackCallback(
    SetPersistServiceChangedCCCCallbackCallback cb) {
  set_persist_service_changed_ccc_cb_cb_ = std::move(cb);
}

void FakeLayer::SetSetRetrieveServiceChangedCCCCallbackCallback(
    SetRetrieveServiceChangedCCCCallbackCallback cb) {
  set_retrieve_service_changed_ccc_cb_cb_ = std::move(cb);
}

void FakeLayer::CallPersistServiceChangedCCCCallback(PeerId peer_id, bool notify, bool indicate) {
  persist_service_changed_ccc_cb_(peer_id, {.notify = notify, .indicate = indicate});
}

std::optional<ServiceChangedCCCPersistedData> FakeLayer::CallRetrieveServiceChangedCCCCallback(
    PeerId peer_id) {
  return retrieve_service_changed_ccc_cb_(peer_id);
}

}  // namespace bt::gatt::testing
