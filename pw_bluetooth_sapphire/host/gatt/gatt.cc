// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt.h"

#include <lib/async/default.h>
#include <zircon/assert.h>

#include <unordered_map>

#include "client.h"
#include "connection.h"
#include "remote_service.h"
#include "server.h"
#include "src/connectivity/bluetooth/core/bt-host/att/bearer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/generic_attribute_service.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"

namespace bt::gatt {

GATT::GATT() : weak_ptr_factory_(this) {}

namespace {

class Impl final : public GATT {
 public:
  explicit Impl() {
    local_services_ = std::make_unique<LocalServiceManager>();

    // Forwards Service Changed payloads to clients.
    auto send_indication_callback = [this](PeerId peer_id, att::Handle handle,
                                           const ByteBuffer& value) {
      auto iter = connections_.find(peer_id);
      if (iter == connections_.end()) {
        bt_log(WARN, "gatt", "peer not registered: %s", bt_str(peer_id));
        return;
      }
      iter->second.server()->SendNotification(handle, value.view(), true);
    };

    // Spin up Generic Attribute as the first service.
    gatt_service_ = std::make_unique<GenericAttributeService>(local_services_.get(),
                                                              std::move(send_indication_callback));

    bt_log(DEBUG, "gatt", "initialized");
  }

  ~Impl() override {
    bt_log(DEBUG, "gatt", "shutting down");

    connections_.clear();
    gatt_service_ = nullptr;
    local_services_ = nullptr;
  }

  // GATT overrides:

  void AddConnection(PeerId peer_id, fbl::RefPtr<att::Bearer> att_bearer,
                     std::unique_ptr<Client> client) override {
    bt_log(DEBUG, "gatt", "add connection %s", bt_str(peer_id));

    auto iter = connections_.find(peer_id);
    if (iter != connections_.end()) {
      bt_log(WARN, "gatt", "peer is already registered: %s", bt_str(peer_id));
      return;
    }

    RemoteServiceWatcher service_watcher = [this, peer_id](
                                               std::vector<att::Handle> removed,
                                               std::vector<fbl::RefPtr<RemoteService>> added,
                                               std::vector<fbl::RefPtr<RemoteService>> modified) {
      OnServicesChanged(peer_id, removed, added, modified);
    };

    connections_.try_emplace(peer_id, peer_id, std::move(att_bearer), std::move(client),
                             local_services_->database(), std::move(service_watcher),
                             async_get_default_dispatcher());

    if (retrieve_service_changed_ccc_callback_) {
      auto optional_service_changed_ccc_data = retrieve_service_changed_ccc_callback_(peer_id);
      if (optional_service_changed_ccc_data && gatt_service_) {
        gatt_service_->SetServiceChangedIndicationSubscription(
            peer_id, optional_service_changed_ccc_data->indicate);
      }
    } else {
      bt_log(WARN, "gatt", "Unable to retrieve service changed CCC: callback not set.");
    }
  }

  void RemoveConnection(PeerId peer_id) override {
    bt_log(DEBUG, "gatt", "remove connection: %s", bt_str(peer_id));
    local_services_->DisconnectClient(peer_id);
    connections_.erase(peer_id);
  }

  void RegisterService(ServicePtr service, ServiceIdCallback callback, ReadHandler read_handler,
                       WriteHandler write_handler, ClientConfigCallback ccc_callback) override {
    IdType id = local_services_->RegisterService(std::move(service), std::move(read_handler),
                                                 std::move(write_handler), std::move(ccc_callback));
    callback(id);
  }

  void UnregisterService(IdType service_id) override {
    local_services_->UnregisterService(service_id);
  }

  void SendNotification(IdType service_id, IdType chrc_id, PeerId peer_id,
                        ::std::vector<uint8_t> value, bool indicate) override {
    // There is nothing to do if the requested peer is not connected.
    auto iter = connections_.find(peer_id);
    if (iter == connections_.end()) {
      bt_log(TRACE, "gatt", "cannot notify disconnected peer: %s", bt_str(peer_id));
      return;
    }

    LocalServiceManager::ClientCharacteristicConfig config;
    if (!local_services_->GetCharacteristicConfig(service_id, chrc_id, peer_id, &config)) {
      bt_log(TRACE, "gatt", "peer has not configured characteristic: %s", bt_str(peer_id));
      return;
    }

    // Make sure that the client has subscribed to the requested protocol
    // method.
    if ((indicate & !config.indicate) || (!indicate && !config.notify)) {
      bt_log(TRACE, "gatt", "peer has no configuration (%s): %s", (indicate ? "ind" : "not"),
             bt_str(peer_id));
      return;
    }

    iter->second.server()->SendNotification(config.handle, BufferView(value.data(), value.size()),
                                            indicate);
  }

  void SetPersistServiceChangedCCCCallback(PersistServiceChangedCCCCallback callback) override {
    gatt_service_->SetPersistServiceChangedCCCCallback(std::move(callback));
  }

  void SetRetrieveServiceChangedCCCCallback(RetrieveServiceChangedCCCCallback callback) override {
    retrieve_service_changed_ccc_callback_ = std::move(callback);
  }

  void DiscoverServices(PeerId peer_id, std::vector<UUID> service_uuids) override {
    bt_log(TRACE, "gatt", "discover services: %s", bt_str(peer_id));

    auto iter = connections_.find(peer_id);
    if (iter == connections_.end()) {
      bt_log(WARN, "gatt", "unknown peer: %s", bt_str(peer_id));
      return;
    }

    iter->second.Initialize(std::move(service_uuids));
  }

  RemoteServiceWatcherId RegisterRemoteServiceWatcherForPeer(
      PeerId peer_id, RemoteServiceWatcher watcher) override {
    ZX_ASSERT(watcher);

    RemoteServiceWatcherId id = next_watcher_id_++;
    peer_remote_service_watchers_.emplace(peer_id, std::make_pair(id, std::move(watcher)));
    return id;
  }

  bool UnregisterRemoteServiceWatcher(RemoteServiceWatcherId watcher_id) override {
    for (auto it = peer_remote_service_watchers_.begin();
         it != peer_remote_service_watchers_.end();) {
      if (watcher_id == it->second.first) {
        it = peer_remote_service_watchers_.erase(it);
        return true;
      }
      it++;
    }
    return false;
  }

  void ListServices(PeerId peer_id, std::vector<UUID> uuids,
                    ServiceListCallback callback) override {
    ZX_ASSERT(callback);
    auto iter = connections_.find(peer_id);
    if (iter == connections_.end()) {
      // Connection not found.
      callback(att::Status(HostError::kNotFound), ServiceList());
      return;
    }
    iter->second.remote_service_manager()->ListServices(uuids, std::move(callback));
  }

  fbl::RefPtr<RemoteService> FindService(PeerId peer_id, IdType service_id) override {
    auto iter = connections_.find(peer_id);
    if (iter == connections_.end()) {
      // Connection not found.
      return nullptr;
    }
    return iter->second.remote_service_manager()->FindService(service_id);
  }

 private:
  void OnServicesChanged(PeerId peer_id, const std::vector<att::Handle>& removed,
                         const std::vector<fbl::RefPtr<RemoteService>>& added,
                         const std::vector<fbl::RefPtr<RemoteService>>& modified) {
    auto peer_watcher_range = peer_remote_service_watchers_.equal_range(peer_id);
    for (auto it = peer_watcher_range.first; it != peer_watcher_range.second; it++) {
      TRACE_DURATION("bluetooth", "GATT::OnServiceChanged notify watcher");
      it->second.second(removed, added, modified);
    }
  }

  // The registry containing all local GATT services. This represents a single
  // ATT database.
  std::unique_ptr<LocalServiceManager> local_services_;

  // Local GATT service (first in database) for clients to subscribe to service
  // registration and removal.
  std::unique_ptr<GenericAttributeService> gatt_service_;

  // Contains the state of all GATT profile connections and their services.
  std::unordered_map<PeerId, internal::Connection> connections_;

  // Callback to fetch CCC for Service Changed indications from upper layers.
  RetrieveServiceChangedCCCCallback retrieve_service_changed_ccc_callback_;

  RemoteServiceWatcherId next_watcher_id_ = 0u;
  std::unordered_multimap<PeerId, std::pair<RemoteServiceWatcherId, RemoteServiceWatcher>>
      peer_remote_service_watchers_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Impl);
};
}  // namespace

// static
std::unique_ptr<GATT> GATT::Create() { return std::make_unique<Impl>(); }

}  // namespace bt::gatt
