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
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

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

void RemoteServiceManager::Initialize(att::StatusCallback cb, std::optional<UUID> service_uuid) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  auto self = weak_ptr_factory_.GetWeakPtr();

  auto init_cb = [self, user_init_cb = std::move(cb)](att::Status status) {
    TRACE_DURATION("bluetooth", "gatt::RemoteServiceManager::Initialize::init_cb");
    if (!self)
      return;

    self->initialized_ = true;

    user_init_cb(status);

    // Notify pending ListService() requests.
    while (!self->pending_.empty()) {
      self->pending_.front().Complete(status, self->services_);
      self->pending_.pop();
    }
  };

  // Start out with the MTU exchange.
  client_->ExchangeMTU([self, init_cb = std::move(init_cb), service_uuid](att::Status status,
                                                                          uint16_t mtu) mutable {
    if (!self) {
      init_cb(att::Status(HostError::kFailed));
      return;
    }

    if (bt_is_error(status, TRACE, "gatt", "MTU exchange failed")) {
      init_cb(status);
      return;
    }

    Client::ServiceCallback svc_cb = [self](const ServiceData& service_data) {
      if (!self) {
        return;
      }

      att::Handle handle = service_data.range_start;
      auto iter = self->services_.find(handle);
      if (iter != self->services_.end()) {
        bt_log(ERROR, "gatt", "found duplicate service attribute handle! (%#.4x)", handle);
        return;
      }

      auto svc = fbl::AdoptRef(
          new RemoteService(service_data, self->client_->AsWeakPtr(), self->gatt_dispatcher_));
      if (!svc) {
        bt_log(DEBUG, "gatt", "failed to allocate RemoteService");
        return;
      }

      self->services_[handle] = svc;
    };

    auto status_cb = [self, init_cb = std::move(init_cb)](att::Status status) {
      TRACE_DURATION("bluetooth", "gatt::RemoteServiceManager::Initialize::MTUCallback::status_cb");
      if (!self) {
        init_cb(att::Status(HostError::kFailed));
        return;
      }

      // Service discovery support is mandatory for servers (v5.0, Vol 3,
      // Part G, 4.2).
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

      init_cb(status);
    };

    auto primary_discov_cb = [self, service_uuid, status_cb = std::move(status_cb),
                              svc_cb = svc_cb.share()](att::Status status) mutable {
      if (!self || !status) {
        status_cb(status);
        return;
      }

      auto secondary_discov_cb = [cb = std::move(status_cb)](att::Status status) mutable {
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
      self->DiscoverServices(ServiceKind::SECONDARY, service_uuid, std::move(svc_cb),
                             std::move(secondary_discov_cb));
    };

    self->DiscoverServices(ServiceKind::PRIMARY, service_uuid, std::move(svc_cb),
                           std::move(primary_discov_cb));
  });
}

void RemoteServiceManager::DiscoverServices(ServiceKind kind, std::optional<UUID> uuid,
                                            Client::ServiceCallback svc_cb,
                                            att::StatusCallback status_cb) {
  if (uuid) {
    client_->DiscoverServicesByUuid(kind, std::move(svc_cb), std::move(status_cb), uuid.value());
  } else {
    client_->DiscoverServices(kind, std::move(svc_cb), std::move(status_cb));
  }
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
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

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
