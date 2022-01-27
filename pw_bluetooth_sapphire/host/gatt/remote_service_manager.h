// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_MANAGER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_MANAGER_H_

#include <lib/async/dispatcher.h>
#include <zircon/assert.h>

#include <map>
#include <memory>
#include <vector>

#include <fbl/ref_ptr.h>

#include "src/connectivity/bluetooth/core/bt-host/att/error.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::gatt {

class Client;

namespace internal {

// Maintains a collection of services that are present on a particular peer
// device. The RemoteServiceManager owns the GATT client data bearer, interacts
// with the profile-defined GATT service, and allows observers to be notified
// as services get discovered.
//
// THREAD SAFETY:
//
// This class is NOT thread safe. It must be created, used, and destroyed on a
// specific thread.
class RemoteServiceManager final {
 public:
  RemoteServiceManager(std::unique_ptr<Client> client, async_dispatcher_t* gatt_dispatcher);
  ~RemoteServiceManager();

  // Adds a handler to be notified when services are removed, added, or modified.
  // NOTE: `removed` services should be handled first because they may share handles with `added`
  // services.
  void set_service_watcher(RemoteServiceWatcher watcher) {
    ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());
    svc_watcher_ = std::move(watcher);
  }

  // Initiates the Exchange MTU procedure followed by service discovery.
  // |callback| is called to notify the result of the procedure.
  // If |services| is empty, discover all services.
  // If |services| is not empty, only discover services that match the UUIDs in |services|.
  // TODO(fxbug.dev/65592): Support initiating multiple service discoveries for different service
  // UUIDs.
  void Initialize(att::ResultFunction<> callback, std::vector<UUID> services = {});

  // Returns a vector containing discovered services that match any of the given
  // |uuids| via |callback|. All services will be returned if |uuids| is empty.
  //
  // If called while uninitialized, |callback| will be run after initialization.
  void ListServices(const std::vector<UUID>& uuids, ServiceListCallback callback);

  // Returns the RemoteService with the requested range start |handle| or
  // nullptr if it is not recognized. This method may fail if called before or
  // during initialization.
  fbl::RefPtr<RemoteService> FindService(att::Handle handle);

 private:
  using ServiceMap = std::map<att::Handle, fbl::RefPtr<RemoteService>>;

  // Used to represent a queued ListServices() request.
  class ServiceListRequest {
   public:
    ServiceListRequest(ServiceListCallback callback, const std::vector<UUID>& uuids);

    ServiceListRequest() = default;
    ServiceListRequest(ServiceListRequest&&) = default;

    // Completes this request by using entries from |services| that match any of
    // the requested UUIDs.
    void Complete(att::Result<> status, const ServiceMap& services);

   private:
    ServiceListCallback callback_;
    std::vector<UUID> uuids_;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ServiceListRequest);
  };

  // State of the current Service Changed notification procedure.
  struct ServiceChangedState {
    ServiceChangedCharacteristicValue value;
    // The cache of services discovered in the changed range.
    std::map<att::Handle, ServiceData> services;
  };

  // If the GATT Profile service has been discovered, returns the service. Returns
  // nullptr otherwise.
  fbl::RefPtr<RemoteService> GattProfileService();

  // Attempt to discover the GATT Profile service. This method must complete before discovery of
  // other services. Notifies |callback| with a status of HostError::kNotFound if the GATT Profile
  // service is not found.
  void DiscoverGattProfileService(att::ResultFunction<> callback);

  // Discovers characteristics of |gatt_profile_service| and enables notifications of the Service
  // Changed characteristic. Notifies |callback| with a status of HostError::kNotFound if the GATT
  // Profile service's Service Changed characteristic is not found.
  void ConfigureServiceChangedNotifications(fbl::RefPtr<RemoteService> gatt_profile_service,
                                            att::ResultFunction<> callback);

  // Discover the GATT Profile service and configure the Service Changed characteristic therein.
  void InitializeGattProfileService(att::ResultFunction<> callback);

  // Create a RemoteService and insert it into the services map, discarding duplicates.
  void AddService(const ServiceData& service_data);

  // Discover services of the indicated |kind|.
  // If |service_uuids| is non-empty, only discovers services with the given UUIDs.
  // |status_cb| will be called when the operation completes.
  using ServiceCallback = fit::function<void(const ServiceData&)>;
  void DiscoverServicesOfKind(ServiceKind kind, std::vector<UUID> service_uuids,
                              att::ResultFunction<> status_cb);

  // Discover primary and secondary services.
  // If |service_uuids| is non-empty, only discovers services with the given UUIDs.
  // |status_cb| will be called when the operation completes.
  void DiscoverServices(std::vector<UUID> service_uuids, att::ResultFunction<> status_cb);
  void DiscoverPrimaryAndSecondaryServicesInRange(std::vector<UUID> service_uuids,
                                                  att::Handle start, att::Handle end,
                                                  ServiceCallback service_cb,
                                                  att::ResultFunction<> status_cb);

  // Shuts down and cleans up all services.
  void ClearServices();

  // Called by |client_| when a notification or indication is received.
  void OnNotification(bool ind, att::Handle value_handle, const ByteBuffer& value,
                      bool maybe_truncated);

  // Handler of notifications from the Service Changed Characteristic
  void OnServiceChangedNotification(const ByteBuffer& buffer);

  // If a service changed notification is queued and none are currently being handled, start
  // processing the next notification.
  // |on_complete| is called when all pending service changed notifications have been processed
  // (immediately if there are none).
  void MaybeHandleNextServiceChangedNotification(fit::callback<void()> on_complete = nullptr);

  // Apply service changed discovery results to |services_|. If initialization is complete, notify
  // |svc_watcher_|.
  void ProcessServiceChangedDiscoveryResults(const ServiceChangedState& service_changed);

  // Calculate the difference between the current services and the services discovered in the
  // Service Changed notification range. Results are appended to the parameters vectors.
  void CalculateServiceChanges(
      const ServiceChangedState& service_changed,
      std::vector<ServiceMap::iterator>& removed_services, std::vector<ServiceData>& added_services,
      std::vector<std::pair<ServiceMap::iterator, ServiceData>>& modified_services);

  async_dispatcher_t* gatt_dispatcher_;
  std::unique_ptr<Client> client_;

  bool initialized_;
  RemoteServiceWatcher svc_watcher_;

  // Requests queued during calls ListServices() before initialization.
  std::queue<ServiceListRequest> pending_list_services_requests_;

  // Services are sorted by handle.
  ServiceMap services_;

  // Queue of unprocessed Service Changed notification changes.
  std::queue<ServiceChangedCharacteristicValue> queued_service_changes_;

  // Service Changed notification that is currently being processed by performing service
  // discovery.
  std::optional<ServiceChangedState> current_service_change_;

  // Callbacks passed to MaybeHandleNextServiceChangedNotification. Will be called when all queued
  // service changes have been processed.
  std::vector<fit::callback<void()>> service_changes_complete_callbacks_;

  fit::thread_checker thread_checker_;
  fxl::WeakPtrFactory<RemoteServiceManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(RemoteServiceManager);
};

}  // namespace internal
}  // namespace bt::gatt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_MANAGER_H_
