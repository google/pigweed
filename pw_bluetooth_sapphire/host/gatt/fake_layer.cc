// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_layer.h"

#include <lib/async/default.h>

namespace bt::gatt::testing {

FakeLayer::TestPeer::TestPeer() : fake_client(async_get_default_dispatcher()) {}

FakeLayer::TestPeer::~TestPeer() {
  for (auto& svc_pair : services) {
    svc_pair.second->ShutDown();
  }
}

std::pair<fbl::RefPtr<RemoteService>, fxl::WeakPtr<FakeClient>> FakeLayer::AddPeerService(
    PeerId peer_id, const ServiceData& info, bool notify) {
  auto [iter, _] = peers_.try_emplace(peer_id);
  auto& peer = iter->second;

  ZX_ASSERT(info.range_start <= info.range_end);
  auto service = fbl::AdoptRef(new RemoteService(info, peer.fake_client.AsWeakPtr()));

  std::vector<att::Handle> removed;
  ServiceList added;
  ServiceList modified;

  auto svc_iter = peer.services.find(info.range_start);
  if (svc_iter != peer.services.end()) {
    if (svc_iter->second->uuid() == info.type) {
      modified.push_back(service);
    } else {
      removed.push_back(svc_iter->second->handle());
      added.push_back(service);
    }

    svc_iter->second->ShutDown(/*service_changed=*/true);
    peer.services.erase(svc_iter);
  } else {
    added.push_back(service);
  }

  bt_log(DEBUG, "gatt", "services changed (removed: %zu, added: %zu, modified: %zu)",
         removed.size(), added.size(), modified.size());

  peer.services.emplace(info.range_start, service);

  if (notify && remote_service_watchers_.count(peer_id)) {
    remote_service_watchers_[peer_id](removed, added, modified);
  }

  return {service, peer.fake_client.AsFakeWeakPtr()};
}

void FakeLayer::RemovePeerService(PeerId peer_id, att::Handle handle) {
  auto peer_iter = peers_.find(peer_id);
  if (peer_iter == peers_.end()) {
    return;
  }
  auto svc_iter = peer_iter->second.services.find(handle);
  if (svc_iter == peer_iter->second.services.end()) {
    return;
  }
  svc_iter->second->ShutDown(/*service_changed=*/true);
  peer_iter->second.services.erase(svc_iter);

  if (remote_service_watchers_.count(peer_id)) {
    remote_service_watchers_[peer_id](/*removed=*/{handle}, /*added=*/{}, /*modified=*/{});
  }
}

void FakeLayer::AddConnection(PeerId peer_id, fbl::RefPtr<att::Bearer> att_bearer,
                              std::unique_ptr<Client> client) {
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
                                 ::std::vector<uint8_t> value, IndicationCallback indicate_cb) {
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

void FakeLayer::DiscoverServices(PeerId peer_id, std::vector<UUID> uuids) {
  if (discover_services_cb_) {
    discover_services_cb_(peer_id, uuids);
  }

  auto iter = peers_.find(peer_id);
  if (iter == peers_.end()) {
    return;
  }

  std::vector<fbl::RefPtr<RemoteService>> added;
  if (uuids.empty()) {
    for (auto& svc_pair : iter->second.services) {
      added.push_back(svc_pair.second);
    }
  } else {
    for (auto& svc_pair : iter->second.services) {
      auto uuid_iter = std::find_if(uuids.begin(), uuids.end(), [&svc_pair](auto uuid) {
        return svc_pair.second->uuid() == uuid;
      });
      if (uuid_iter != uuids.end()) {
        added.push_back(svc_pair.second);
      }
    }
  }

  if (remote_service_watchers_.count(peer_id)) {
    remote_service_watchers_[peer_id](/*removed=*/{}, /*added=*/added, /*modified=*/{});
  }
}

GATT::RemoteServiceWatcherId FakeLayer::RegisterRemoteServiceWatcherForPeer(
    PeerId peer_id, RemoteServiceWatcher watcher) {
  ZX_ASSERT(remote_service_watchers_.count(peer_id) == 0);
  remote_service_watchers_[peer_id] = std::move(watcher);
  // Use the PeerId as the watcher ID because FakeLayer only needs to support 1 watcher per peer.
  return peer_id.value();
}
bool FakeLayer::UnregisterRemoteServiceWatcher(RemoteServiceWatcherId watcher_id) {
  bool result = remote_service_watchers_.count(PeerId(watcher_id));
  remote_service_watchers_.erase(PeerId(watcher_id));
  return result;
}

void FakeLayer::ListServices(PeerId peer_id, std::vector<UUID> uuids,
                             ServiceListCallback callback) {
  if (pause_list_services_) {
    return;
  }

  ServiceList services;

  auto iter = peers_.find(peer_id);
  if (iter != peers_.end()) {
    for (auto& svc_pair : iter->second.services) {
      auto pred = [&](const UUID& uuid) { return svc_pair.second->uuid() == uuid; };
      if (uuids.empty() || std::find_if(uuids.begin(), uuids.end(), pred) != uuids.end()) {
        services.push_back(svc_pair.second);
      }
    }
  }

  callback(list_services_status_, std::move(services));
}

fbl::RefPtr<RemoteService> FakeLayer::FindService(PeerId peer_id, IdType service_id) {
  auto peer_iter = peers_.find(peer_id);
  if (peer_iter == peers_.end()) {
    return nullptr;
  }
  auto svc_iter = peer_iter->second.services.find(service_id);
  if (svc_iter == peer_iter->second.services.end()) {
    return nullptr;
  }
  return svc_iter->second;
}

void FakeLayer::SetDiscoverServicesCallback(DiscoverServicesCallback cb) {
  discover_services_cb_ = std::move(cb);
}

void FakeLayer::set_list_services_status(att::Result<> status) { list_services_status_ = status; }

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
