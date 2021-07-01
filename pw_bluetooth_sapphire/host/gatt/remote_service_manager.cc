// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_service_manager.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"

namespace bt::gatt::internal {

RemoteServiceManager::ServiceListRequest::ServiceListRequest(ServiceListCallback callback,
                                                             const std::vector<UUID>& uuids)
    : callback_(std::move(callback)), uuids_(uuids) {
  ZX_DEBUG_ASSERT(callback_);
}

void RemoteServiceManager::ServiceListRequest::Complete(att::Status status,
                                                        const ServiceMap& services) {
  TRACE_DURATION("bluetooth", "gatt::RemoteServiceManager::ServiceListRequest::Complete");

  ServiceList result;

  if (!status || services.empty()) {
    callback_(status, std::move(result));
    return;
  }

  for (const auto& iter : services) {
    auto& svc = iter.second;
    auto pred = [&svc](const UUID& uuid) { return svc->uuid() == uuid; };
    if (uuids_.empty() || std::find_if(uuids_.begin(), uuids_.end(), pred) != uuids_.end()) {
      result.push_back(iter.second);
    }
  }

  callback_(status, std::move(result));
}

RemoteServiceManager::RemoteServiceManager(std::unique_ptr<Client> client,
                                           async_dispatcher_t* gatt_dispatcher)
    : gatt_dispatcher_(gatt_dispatcher),
      client_(std::move(client)),
      initialized_(false),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(gatt_dispatcher_);
  ZX_DEBUG_ASSERT(client_);

  client_->SetNotificationHandler(fit::bind_member(this, &RemoteServiceManager::OnNotification));
}

RemoteServiceManager::~RemoteServiceManager() {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  client_->SetNotificationHandler({});
  ClearServices();

  // Resolve all pending requests with an error.
  att::Status status(HostError::kFailed);

  auto pending = std::move(pending_);
  while (!pending.empty()) {
    // This copies |services|.
    pending.front().Complete(status, services_);
    pending.pop();
  }
}

void RemoteServiceManager::Initialize(att::StatusCallback cb, std::vector<UUID> services) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  auto self = weak_ptr_factory_.GetWeakPtr();

  auto init_cb = [self, user_init_cb = std::move(cb)](att::Status status) {
    TRACE_DURATION("bluetooth", "gatt::RemoteServiceManager::Initialize::init_cb");

    // The Client's Bearer may outlive this object.
    if (!self) {
      return;
    }

    self->initialized_ = true;

    user_init_cb(status);

    // Notify pending ListService() requests.
    while (!self->pending_.empty()) {
      self->pending_.front().Complete(status, self->services_);
      self->pending_.pop();
    }
  };

  client_->ExchangeMTU([self, init_cb = std::move(init_cb), services = std::move(services)](
                           att::Status status, uint16_t mtu) mutable {
    // The Client's Bearer may outlive this object.
    if (!self) {
      return;
    }

    if (bt_is_error(status, INFO, "gatt", "MTU exchange failed")) {
      init_cb(status);
      return;
    }

    self->InitializeGattProfileService(
        [self, init_cb = std::move(init_cb),
         services = std::move(services)](att::Status status) mutable {
          if (status.error() == HostError::kNotFound) {
            // The GATT Profile service's Service Changed characteristic is optional. Its absence
            // implies that the set of GATT services on the server is fixed, so the kNotFound error
            // can be safely ignored.
            bt_log(DEBUG, "gatt", "GATT Profile service not found. Assuming services are fixed.");
          } else if (!status.is_success()) {
            init_cb(status);
            return;
          }

          self->DiscoverServices(std::move(services), std::move(init_cb));
        });
  });
}

fbl::RefPtr<RemoteService> RemoteServiceManager::GattProfileService() {
  auto service_iter = std::find_if(services_.begin(), services_.end(), [](auto& s) {
    return s.second->uuid() == types::kGenericAttributeService;
  });
  return service_iter == services_.end() ? nullptr : service_iter->second;
}

void RemoteServiceManager::OnServiceChangedNotification(const ByteBuffer& value) {
  // TODO(fxbug.dev/71986): Handle service changed notification.
  bt_log(WARN, "gatt", "Ignoring service changed notification");
}

void RemoteServiceManager::ConfigureServiceChangedNotifications(
    fbl::RefPtr<RemoteService> gatt_profile_service, att::StatusCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  gatt_profile_service->DiscoverCharacteristics(
      [self, callback = std::move(callback)](att::Status status,
                                             const CharacteristicMap& characteristics) mutable {
        // The Client's Bearer may outlive this object.
        if (!self) {
          return;
        }

        if (bt_is_error(status, WARN, "gatt",
                        "Error discovering GATT Profile service characteristics")) {
          callback(status);
          return;
        }

        auto gatt_profile_service = self->GattProfileService();
        ZX_ASSERT(gatt_profile_service);

        auto svc_changed_char_iter =
            std::find_if(characteristics.begin(), characteristics.end(),
                         [](CharacteristicMap::const_reference c) {
                           const CharacteristicData& data = c.second.first;
                           return data.type == types::kServiceChangedCharacteristic;
                         });

        // The Service Changed characteristic is optional, and its absence implies that the set
        // of GATT services on the server is fixed.
        if (svc_changed_char_iter == characteristics.end()) {
          callback(att::Status(HostError::kNotFound));
          return;
        }

        const bt::gatt::CharacteristicHandle svc_changed_char_handle = svc_changed_char_iter->first;

        auto notification_cb = [self](const ByteBuffer& value) {
          // The Client's Bearer may outlive this object.
          if (self) {
            self->OnServiceChangedNotification(value);
          }
        };

        // Don't save handler_id as notifications never need to be disabled.
        auto status_cb = [self, callback = std::move(callback)](att::Status status,
                                                                IdType /*handler_id*/) {
          // The Client's Bearer may outlive this object.
          if (!self) {
            return;
          }

          // If the Service Changed characteristic exists, notification support is mandatory (Core
          // Spec v5.2, Vol 3, Part G, Sec 7.1).
          if (bt_is_error(status, WARN, "gatt",
                          "Enabling notifications of Service Changed characteristic failed")) {
            callback(status);
            return;
          }

          callback(att::Status());
        };

        gatt_profile_service->EnableNotifications(svc_changed_char_handle,
                                                  std::move(notification_cb), std::move(status_cb));
      });
}

void RemoteServiceManager::InitializeGattProfileService(att::StatusCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  DiscoverGattProfileService([self, callback = std::move(callback)](att::Status status) mutable {
    // The Client's Bearer may outlive this object.
    if (!self) {
      return;
    }

    if (!status.is_success()) {
      callback(status);
      return;
    }

    fbl::RefPtr<RemoteService> gatt_svc = self->GattProfileService();
    ZX_ASSERT(gatt_svc);
    self->ConfigureServiceChangedNotifications(
        std::move(gatt_svc), [self, callback = std::move(callback)](att::Status status) {
          // The Client's Bearer may outlive this object.
          if (!self) {
            return;
          }

          callback(status);
        });
  });
}

void RemoteServiceManager::DiscoverGattProfileService(att::StatusCallback callback) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb = [self, callback = std::move(callback)](att::Status status) {
    if (!self) {
      return;
    }

    if (bt_is_error(status, WARN, "gatt", "Error discovering GATT Profile service")) {
      callback(status);
      return;
    }

    // The GATT Profile service is optional, and its absence implies that the set of GATT services
    // on the server is fixed.
    if (self->services_.empty()) {
      callback(att::Status(HostError::kNotFound));
      return;
    }

    // At most one instance of the GATT Profile service may exist (Core Spec v5.2, Vol 3, Part G,
    // Sec 7).
    if (self->services_.size() > 1) {
      bt_log(WARN, "gatt", "Discovered (%zu) GATT Profile services, expected 1",
             self->services_.size());
      callback(att::Status(HostError::kFailed));
      return;
    }

    UUID uuid = self->services_.begin()->second->uuid();
    // The service UUID is filled in by Client based on the service discovery request, so it should
    // be the same as the requested UUID.
    ZX_ASSERT(uuid == types::kGenericAttributeService);

    callback(att::Status());
  };
  DiscoverServicesOfKind(ServiceKind::PRIMARY, {types::kGenericAttributeService},
                         std::move(status_cb));
}

void RemoteServiceManager::AddService(const ServiceData& service_data) {
  att::Handle handle = service_data.range_start;
  auto iter = services_.find(handle);
  if (iter != services_.end()) {
    // The GATT Profile service is discovered before general service discovery, so it may be
    // discovered twice.
    if (iter->second->uuid() != types::kGenericAttributeService) {
      bt_log(WARN, "gatt", "found duplicate service attribute handle! (%#.4x)", handle);
    }
    return;
  }

  auto svc = fbl::AdoptRef(new RemoteService(service_data, client_->AsWeakPtr(), gatt_dispatcher_));
  if (!svc) {
    bt_log(DEBUG, "gatt", "failed to allocate RemoteService");
    return;
  }

  services_[handle] = std::move(svc);
}

void RemoteServiceManager::DiscoverServicesOfKind(ServiceKind kind, std::vector<UUID> service_uuids,
                                                  att::StatusCallback status_cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  ServiceCallback svc_cb = [self](const ServiceData& service_data) {
    // The Client's Bearer may outlive this object.
    if (self) {
      self->AddService(service_data);
    }
  };

  if (!service_uuids.empty()) {
    client_->DiscoverServicesWithUuids(kind, std::move(svc_cb), std::move(status_cb),
                                       std::move(service_uuids));
  } else {
    client_->DiscoverServices(kind, std::move(svc_cb), std::move(status_cb));
  }
}

void RemoteServiceManager::DiscoverServices(std::vector<UUID> service_uuids,
                                            att::StatusCallback status_cb) {
  auto self = weak_ptr_factory_.GetWeakPtr();
  auto status_cb_wrapper = [self, status_cb = std::move(status_cb)](att::Status status) {
    TRACE_DURATION("bluetooth", "gatt::RemoteServiceManager::DiscoverServices::status_cb_wrapper");

    // The Client's Bearer may outlive this object.
    if (!self) {
      status_cb(att::Status(HostError::kFailed));
      return;
    }

    // Service discovery support is mandatory for servers (v5.0, Vol 3, Part G, 4.2).
    if (bt_is_error(status, TRACE, "gatt", "failed to discover services")) {
      // Clear services that were buffered so far.
      self->ClearServices();
    } else if (self->svc_watcher_) {
      // Notify all discovered services here.
      for (auto& iter : self->services_) {
        TRACE_DURATION("bluetooth", "gatt::RemoteServiceManager::svc_watcher_");
        self->svc_watcher_(iter.second);
      }
    }

    status_cb(status);
  };

  auto primary_discov_cb = [self, service_uuids, status_cb_wrapper = std::move(status_cb_wrapper)](
                               att::Status status) mutable {
    if (!self || !status) {
      status_cb_wrapper(status);
      return;
    }

    auto secondary_discov_cb = [cb = std::move(status_cb_wrapper)](att::Status status) mutable {
      // Not all GATT servers support the "secondary service" group type. We suppress the
      // "Unsupported Group Type" error code and simply report no services instead of treating it
      // as a fatal condition (errors propagated up the stack from here will cause the connection
      // to be terminated).
      if (status.is_protocol_error() &&
          status.protocol_error() == att::ErrorCode::kUnsupportedGroupType) {
        bt_log(DEBUG, "gatt", "peer does not support secondary services; ignoring ATT error");
        status = att::Status();
      }

      cb(status);
    };

    self->DiscoverServicesOfKind(ServiceKind::SECONDARY, std::move(service_uuids),
                                 std::move(secondary_discov_cb));
  };

  DiscoverServicesOfKind(ServiceKind::PRIMARY, std::move(service_uuids),
                         std::move(primary_discov_cb));
}

void RemoteServiceManager::ListServices(const std::vector<UUID>& uuids,
                                        ServiceListCallback callback) {
  ServiceListRequest request(std::move(callback), uuids);
  if (initialized_) {
    request.Complete(att::Status(), services_);
  } else {
    pending_.push(std::move(request));
  }
}

fbl::RefPtr<RemoteService> RemoteServiceManager::FindService(att::Handle handle) {
  auto iter = services_.find(handle);
  return iter == services_.end() ? nullptr : iter->second;
}

void RemoteServiceManager::ClearServices() {
  auto services = std::move(services_);
  for (auto& iter : services) {
    iter.second->ShutDown();
  }
}

void RemoteServiceManager::OnNotification(bool, att::Handle value_handle, const ByteBuffer& value) {
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  if (services_.empty()) {
    bt_log(DEBUG, "gatt", "ignoring notification from unknown service");
    return;
  }

  // Find the service that |value_handle| belongs to.
  auto iter = services_.upper_bound(value_handle);
  if (iter != services_.begin())
    --iter;

  // If |value_handle| is within the previous service then we found it.
  auto& svc = iter->second;
  ZX_DEBUG_ASSERT(value_handle >= svc->handle());

  if (svc->info().range_end >= value_handle) {
    svc->HandleNotification(value_handle, value);
  }
}

}  // namespace bt::gatt::internal
