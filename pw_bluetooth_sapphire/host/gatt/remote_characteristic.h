// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_CHARACTERISTIC_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_CHARACTERISTIC_H_

#include <lib/async/dispatcher.h>
#include <lib/fit/function.h>

#include <map>
#include <queue>
#include <unordered_map>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/att/status.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/lib/fxl/memory/weak_ptr.h"
#include "src/lib/fxl/synchronization/thread_checker.h"

namespace bt {
namespace gatt {

class Client;

// Used by a RemoteService to represent one of its characteristics. This object
// maintains information about a characteristic (such as its descriptors, known
// permissions, etc) and is responsible for routing notifications to subscribed
// clients.
//
// The public accessors of this class are thread-safe and can be called on any
// thread as long as this object is alive and is only managed by a
// RemoteService.
//
// Instances are created and owned by a RemoteService. Creation and all
// modifications will happen on the GATT thread.
//
// ID SCHEME:
//
// The ID that gets assigned to a RemoteCharacteristic is its value_handle
// The ID that gets assigned to a Descriptor is its handle. Looking up a descriptor by id from the
// service is linear in the number of characteristics.
//
// THREAD-SAFETY:
//
// Since each RemoteService is shared (potentially across threads),
// RemoteCharacteristic objects can get destroyed on any thread. RemoteService
// MUST call ShutDown() on the GATT thread to ensure safe clean up.
class RemoteCharacteristic final {
 public:
  using ValueCallback = fit::function<void(const ByteBuffer&)>;
  using NotifyStatusCallback = fit::function<void(att::Status, IdType handler_id)>;

  // We use an ordered map so that the Descriptors are exposed to the world in order
  using DescriptorMap = std::map<DescriptorHandle, DescriptorData>;

  RemoteCharacteristic(fxl::WeakPtr<Client> client, const CharacteristicData& info);
  ~RemoteCharacteristic() = default;

  // The move constructor allows this move-only type to be stored in a vector
  // (or array) by value (this allows RemoteService to store
  // RemoteCharacteristics in contiguous memory).
  //
  // Moving transfers all data from the source object to the destination except
  // for the weak pointer factory. All weak pointers to the source object are
  // invalidated.
  //
  // Care should be taken when used together with std::vector as it moves its
  // contents while resizing its storage.
  RemoteCharacteristic(RemoteCharacteristic&&);

  // ATT declaration data for this characteristic.
  const CharacteristicData& info() const { return info_; }

  // Descriptors of this characteristic.
  const DescriptorMap& descriptors() const { return descriptors_; }

 private:
  friend class RemoteService;

  // The following private methods can only be called by a RemoteService. All
  // except the destructor will be called on the GATT thread.

  // Cleans up all state associated with this characteristic.
  void ShutDown();

  // Discovers the descriptors of this characteristic and reports the status in
  // |callback|.
  //
  // NOTE: The owning RemoteService is responsible for ensuring that this object
  // outlives the discovery procedure.
  void DiscoverDescriptors(att::Handle range_end, att::StatusCallback callback);

  // (See RemoteService::EnableNotifications in remote_service.h).
  void EnableNotifications(ValueCallback value_callback, NotifyStatusCallback status_callback,
                           async_dispatcher_t* dispatcher = nullptr);
  bool DisableNotifications(IdType handler_id);

  // Sends a request to disable notifications and indications. Called by
  // DisableNotifications and ShutDown.
  void DisableNotificationsInternal();

  // Resolves all pending notification subscription requests. Called by
  // EnableNotifications().
  void ResolvePendingNotifyRequests(att::Status status);

  // Called when a notification is received for this characteristic.
  void HandleNotification(const ByteBuffer& value);

  fxl::ThreadChecker thread_checker_;
  CharacteristicData info_;
  DescriptorMap descriptors_;
  bool discovery_error_;

  std::atomic_bool shut_down_;

  // Handle of the Client Characteristic Configuration descriptor, or 0 if none.
  att::Handle ccc_handle_;

  // Represents a pending request to subscribe to notifications or indications.
  struct PendingNotifyRequest {
    PendingNotifyRequest(async_dispatcher_t* dispatcher, ValueCallback value_callback,
                         NotifyStatusCallback status_callback);

    PendingNotifyRequest() = default;
    PendingNotifyRequest(PendingNotifyRequest&&) = default;

    async_dispatcher_t* dispatcher;
    ValueCallback value_callback;
    NotifyStatusCallback status_callback;
  };
  std::queue<PendingNotifyRequest> pending_notify_reqs_;

  // Active notification handlers.
  struct NotifyHandler {
    NotifyHandler(async_dispatcher_t* dispatcher, ValueCallback callback);

    NotifyHandler() = default;
    NotifyHandler(NotifyHandler&&) = default;
    NotifyHandler& operator=(NotifyHandler&&) = default;

    async_dispatcher_t* dispatcher;
    ValueCallback callback;
  };
  std::unordered_map<IdType, NotifyHandler> notify_handlers_;

  // The next available notification handler ID.
  size_t next_notify_handler_id_;

  // The GATT client bearer used for ATT requests.
  fxl::WeakPtr<Client> client_;

  fxl::WeakPtrFactory<RemoteCharacteristic> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(RemoteCharacteristic);
};

}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_CHARACTERISTIC_H_
