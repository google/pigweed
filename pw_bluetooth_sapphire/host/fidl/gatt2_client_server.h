// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_CLIENT_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_CLIENT_SERVER_H_

#include <fuchsia/bluetooth/gatt2/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "src/connectivity/bluetooth/core/bt-host/fidl/gatt2_remote_service_server.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/server_base.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

namespace bthost {
class Gatt2ClientServer : public GattServerBase<fuchsia::bluetooth::gatt2::Client> {
 public:
  // |error_cb| will be called if the FIDL client closed the protocol or an error occurs and
  // this server should be destroyed.
  Gatt2ClientServer(bt::gatt::PeerId peer_id, fxl::WeakPtr<bt::gatt::GATT> weak_gatt,
                    fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::Client> request,
                    fit::callback<void()> error_cb);
  ~Gatt2ClientServer() override;

 private:
  using WatchServicesCallbackOnce =
      fit::callback<void(std::vector<::fuchsia::bluetooth::gatt2::ServiceInfo>,
                         std::vector<::fuchsia::bluetooth::gatt2::Handle>)>;
  using WatchServicesRequest = WatchServicesCallbackOnce;

  using ServiceMap = std::unordered_map<bt::att::Handle, fbl::RefPtr<bt::gatt::RemoteService>>;

  struct WatchServicesResult {
    std::unordered_set<bt::att::Handle> removed;
    ServiceMap updated;
  };

  void OnWatchServicesResult(const std::vector<bt::att::Handle>& removed,
                             const std::vector<fbl::RefPtr<bt::gatt::RemoteService>>& added,
                             const std::vector<fbl::RefPtr<bt::gatt::RemoteService>>& modified);

  void TrySendNextWatchServicesResult();

  // fuchsia::bluetooth::gatt2::Client overrides:
  void WatchServices(std::vector<::fuchsia::bluetooth::Uuid> fidl_uuids,
                     WatchServicesCallback callback) override;
  void ConnectToService(
      fuchsia::bluetooth::gatt2::Handle handle,
      fidl::InterfaceRequest<fuchsia::bluetooth::gatt2::RemoteService> request) override;

  // The ID of the peer that this client is attached to.
  bt::gatt::PeerId peer_id_;

  // Callback provided by this server's owner that handles fatal errors (by closing this server).
  fit::callback<void()> server_error_cb_;

  // If a service's handle maps to a null value, a connection request to that service is in
  // progress.
  // TODO(fxbug.dev/84788): Once FindService() returns the service directly, don't use null values.
  std::unordered_map<bt::att::Handle, std::unique_ptr<Gatt2RemoteServiceServer>> services_;

  // False initially, and set to true after GATT::ListServices() completes.
  // Set to false again if WatchServices() is called with a new UUID list.
  bool list_services_complete_ = false;

  // UUIDs of the previous WatchServices() call, if any.
  std::unordered_set<bt::UUID> prev_watch_services_uuids_;
  std::optional<WatchServicesRequest> watch_services_request_;

  // Between clients calls to WatchServices, service watcher results are accumulated here.
  std::optional<WatchServicesResult> next_watch_services_result_;

  bt::gatt::GATT::RemoteServiceWatcherId service_watcher_id_;

  // Must be the last member of this class.
  fxl::WeakPtrFactory<Gatt2ClientServer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(Gatt2ClientServer);
};
}  // namespace bthost

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_FIDL_GATT2_CLIENT_SERVER_H_
