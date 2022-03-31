// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt.h"

#include <lib/async/default.h>
#include <lib/fit/defer.h>
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
    auto send_indication_callback = [this](IdType service_id, IdType chrc_id, PeerId peer_id,
                                           BufferView value) {
      auto iter = connections_.find(peer_id);
      if (iter == connections_.end()) {
        bt_log(WARN, "gatt", "peer not registered: %s", bt_str(peer_id));
        return;
      }
      auto indication_cb = [](att::Result<> result) {
        bt_log(TRACE, "gatt", "service changed indication complete: %s", bt_str(result));
      };
      iter->second.server()->SendUpdate(service_id, chrc_id, value.view(),
                                        std::move(indication_cb));
    };

    // Spin up Generic Attribute as the first service.
    gatt_service_ = std::make_unique<GenericAttributeService>(local_services_->GetWeakPtr(),
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

  void AddConnection(PeerId peer_id, std::unique_ptr<Client> client,
                     Server::FactoryFunction server_factory) override {
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
    std::unique_ptr<Server> server = server_factory(peer_id, local_services_->GetWeakPtr());
    connections_.try_emplace(peer_id, std::move(client), std::move(server),
                             std::move(service_watcher), async_get_default_dispatcher());

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

  void SendUpdate(IdType service_id, IdType chrc_id, PeerId peer_id, ::std::vector<uint8_t> value,
                  IndicationCallback indicate_cb) override {
    // There is nothing to do if the requested peer is not connected.
    auto iter = connections_.find(peer_id);
    if (iter == connections_.end()) {
      bt_log(TRACE, "gatt", "cannot notify disconnected peer: %s", bt_str(peer_id));
      if (indicate_cb) {
        indicate_cb(ToResult(HostError::kNotFound));
      }
      return;
    }
    iter->second.server()->SendUpdate(service_id, chrc_id, BufferView(value.data(), value.size()),
                                      std::move(indicate_cb));
  }

  void UpdateConnectedPeers(IdType service_id, IdType chrc_id, ::std::vector<uint8_t> value,
                            IndicationCallback indicate_cb) override {
    att::ResultFunction<> shared_peer_results_cb(nullptr);
    if (indicate_cb) {
      // This notifies indicate_cb with success when destroyed (if indicate_cb has not been invoked)
      auto deferred_success = fit::defer([outer_cb = indicate_cb.share()]() mutable {
        if (outer_cb) {
          outer_cb(fitx::ok());
        }
      });
      // This captures, but doesn't use, deferred_success. Because this is later |share|d for each
      // peer's SendUpdate callback, deferred_success is stored in this refcounted memory. If any of
      // the SendUpdate callbacks fail, the outer callback is notified of failure. But if all of the
      // callbacks succeed, shared_peer_results_cb's captures will be destroyed, and
      // deferred_success will then notify indicate_cb of success.
      shared_peer_results_cb = [deferred = std::move(deferred_success),
                                outer_cb = std::move(indicate_cb)](att::Result<> res) mutable {
        if (outer_cb && res.is_error()) {
          outer_cb(res);
        }
      };
    }
    for (auto& iter : connections_) {
      // The `shared_peer_results_cb.share()` *does* propagate indication vs. notification-ness
      // correctly - `fit::function(nullptr).share` just creates another null fit::function.
      iter.second.server()->SendUpdate(service_id, chrc_id, BufferView(value.data(), value.size()),
                                       shared_peer_results_cb.share());
    }
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
      callback(ToResult(HostError::kNotFound), ServiceList());
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
