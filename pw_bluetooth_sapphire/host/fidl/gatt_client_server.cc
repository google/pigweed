// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_client_server.h"

#include <lib/fit/defer.h>

#include "gatt_remote_service_server.h"
#include "helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;

using fuchsia::bluetooth::gatt::Client;
using fuchsia::bluetooth::gatt::RemoteService;
using fuchsia::bluetooth::gatt::ServiceInfo;
using fuchsia::bluetooth::gatt::ServiceInfoPtr;

namespace bthost {

GattClientServer::GattClientServer(bt::gatt::PeerId peer_id, fxl::WeakPtr<bt::gatt::GATT> gatt,
                                   fidl::InterfaceRequest<Client> request)
    : GattServerBase(gatt, this, std::move(request)), peer_id_(peer_id), weak_ptr_factory_(this) {}

void GattClientServer::ListServices(::fidl::VectorPtr<::std::string> fidl_uuids,
                                    ListServicesCallback callback) {
  // Parse the UUID list.
  std::vector<bt::UUID> uuids;
  if (fidl_uuids.has_value()) {
    // Allocate all at once and convert in-place.
    uuids.resize(fidl_uuids->size());
    for (size_t i = 0; i < uuids.size(); ++i) {
      if (!StringToUuid(fidl_uuids.value()[i], &uuids[i])) {
        bt_log(WARN, "fidl", "%s: Invalid UUID: %s (peer: %s)", __FUNCTION__,
               fidl_uuids.value()[i].c_str(), bt_str(peer_id_));
        callback(fidl_helpers::NewFidlError(ErrorCode::INVALID_ARGUMENTS,
                                            "Invalid UUID: " + fidl_uuids.value()[i]),
                 std::vector<ServiceInfo>((size_t)0u));
        return;
      }
    }
  }

  auto cb = [callback = std::move(callback), peer_id = peer_id_, func = __FUNCTION__](
                bt::att::Result<> status, auto services) {
    std::vector<ServiceInfo> out;
    if (status.is_error()) {
      bt_log(WARN, "fidl", "%s: Failed to discover services (peer: %s)", func, bt_str(peer_id));
      auto fidl_status =
          fidl_helpers::ResultToFidlDeprecated(status, "Failed to discover services");
      callback(std::move(fidl_status), std::move(out));
      return;
    }

    out.resize(services.size());

    size_t i = 0;
    for (const auto& svc : services) {
      ServiceInfo service_info;
      service_info.id = svc->handle();
      service_info.primary = svc->info().kind == bt::gatt::ServiceKind::PRIMARY;
      service_info.type = svc->uuid().ToString();
      out[i++] = std::move(service_info);
    }
    callback(Status(), std::move(out));
  };

  gatt()->ListServices(peer_id_, std::move(uuids), std::move(cb));
}

void GattClientServer::ConnectToService(uint64_t id,
                                        ::fidl::InterfaceRequest<RemoteService> request) {
  if (connected_services_.count(id)) {
    bt_log(WARN, "fidl", "%s: service already requested (service: %lu, peer: %s)", __FUNCTION__, id,
           bt_str(peer_id_));
    return;
  }

  // Initialize an entry so that we remember when this request is in progress.
  connected_services_[id] = nullptr;

  fxl::WeakPtr<bt::gatt::RemoteService> service = gatt()->FindService(peer_id_, id);

  // Automatically called on failure.
  auto fail_cleanup = fit::defer([this, id] { connected_services_.erase(id); });

  if (!service) {
    bt_log(WARN, "fidl", "%s: failed (service: %lu, peer: %s)", __FUNCTION__, id, bt_str(peer_id_));
    return;
  }

  // Clean up the server if either the peer device or the FIDL client
  // disconnects.
  auto self = weak_ptr_factory_.GetWeakPtr();
  const char* func = __FUNCTION__;
  auto error_cb = [self, id, peer_id = peer_id_, func] {
    bt_log(DEBUG, "fidl", "%s: service disconnected (service: %lu, peer: %s)", func, id,
           bt_str(peer_id));
    if (self) {
      self->connected_services_.erase(id);
    }
  };

  if (!service->AddRemovedHandler(error_cb)) {
    bt_log(WARN, "fidl", "%s: failed to assign closed handler (service: %lu, peer: %s)", func, id,
           bt_str(self->peer_id_));
    return;
  }

  fail_cleanup.cancel();

  auto server = std::make_unique<GattRemoteServiceServer>(service->GetWeakPtr(), gatt(), peer_id_,
                                                          std::move(request));
  server->set_error_handler([cb = std::move(error_cb)](zx_status_t status) { cb(); });

  self->connected_services_[id] = std::move(server);
}

}  // namespace bthost
