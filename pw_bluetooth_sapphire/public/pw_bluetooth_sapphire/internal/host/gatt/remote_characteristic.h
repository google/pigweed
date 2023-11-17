// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_CHARACTERISTIC_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_CHARACTERISTIC_H_

#include <lib/fit/function.h>

#include <map>
#include <queue>
#include <unordered_map>

#include "pw_bluetooth_sapphire/internal/host/att/error.h"
#include "pw_bluetooth_sapphire/internal/host/common/macros.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/client.h"
#include "pw_bluetooth_sapphire/internal/host/gatt/gatt_defs.h"

namespace bt::gatt {

class Client;

// Used by a RemoteService to represent one of its characteristics. This object
// maintains information about a characteristic (such as its descriptors, known
// permissions, etc) and is responsible for routing notifications to subscribed
// clients.
//
// Instances are created and owned by a RemoteService.
//
// ID SCHEME:
//
// The ID that gets assigned to a RemoteCharacteristic is its value_handle
// The ID that gets assigned to a Descriptor is its handle. Looking up a
// descriptor by id from the service is logarithmic in the number of
// descriptors.
class RemoteCharacteristic final {
 public:
  using ValueCallback =
      fit::function<void(const ByteBuffer&, bool maybe_truncated)>;
  using NotifyStatusCallback =
      fit::function<void(att::Result<>, IdType handler_id)>;

  // We use an ordered map so that the Descriptors are exposed to the world in
  // order
  using DescriptorMap = std::map<DescriptorHandle, DescriptorData>;

  RemoteCharacteristic(Client::WeakPtr client, const CharacteristicData& info);
  ~RemoteCharacteristic();

  // The properties for this characteristic.
  Properties properties() const { return info_.properties; }

  // The extended properties for this characteristic.
  std::optional<ExtendedProperties> extended_properties() const {
    return info_.extended_properties;
  }

  // ATT declaration data for this characteristic.
  const CharacteristicData& info() const { return info_; }

  // Descriptors of this characteristic.
  const DescriptorMap& descriptors() const { return descriptors_; }

 private:
  friend class RemoteService;

  // The following private methods can only be called by a RemoteService.

  // `service_changed` indicates whether destruction will occur due to a Service
  // Changed notification, in which case this characteristic may no longer exist
  // or may have been changed.
  void set_service_changed(bool service_changed) {
    service_changed_ = service_changed;
  }

  // Updates the CharacteristicData |info_| with the Extended Properties that
  // are read from the descriptors discovered in |DiscoverDescriptors|.
  void UpdateDataWithExtendedProperties(ExtendedProperties ext_props);

  // Discovers the descriptors of this characteristic and reports the status in
  // |callback|.
  //
  // NOTE: The owning RemoteService is responsible for ensuring that this object
  // outlives the discovery procedure.
  void DiscoverDescriptors(att::Handle range_end,
                           att::ResultFunction<> callback);

  // (See RemoteService::EnableNotifications in remote_service.h).
  void EnableNotifications(ValueCallback value_callback,
                           NotifyStatusCallback status_callback);
  bool DisableNotifications(IdType handler_id);

  // Sends a request to disable notifications and indications. Called by
  // DisableNotifications and destructor.
  void DisableNotificationsInternal();

  // Resolves all pending notification subscription requests. Called by
  // EnableNotifications().
  void ResolvePendingNotifyRequests(att::Result<> status);

  // Called when a notification is received for this characteristic.
  void HandleNotification(const ByteBuffer& value, bool maybe_truncated);

  CharacteristicData info_;
  DescriptorMap descriptors_;
  bool discovery_error_;

  // If true, this characteristic was in a service that has been changed. Values
  // should not be read/written after a service is changed.
  bool service_changed_ = false;

  // Handle of the Client Characteristic Configuration descriptor, or 0 if none.
  att::Handle ccc_handle_;

  // Handle of the Characteristic Extended Properties descriptor, or 0 if none.
  att::Handle ext_prop_handle_;

  // Represents a pending request to subscribe to notifications or indications.
  struct PendingNotifyRequest {
    PendingNotifyRequest(ValueCallback value_callback,
                         NotifyStatusCallback status_callback);

    PendingNotifyRequest() = default;
    PendingNotifyRequest(PendingNotifyRequest&&) = default;

    ValueCallback value_callback;
    NotifyStatusCallback status_callback;
  };
  std::queue<PendingNotifyRequest> pending_notify_reqs_;

  // Active notification handlers.
  std::unordered_map<IdType, ValueCallback> notify_handlers_;
  // Set to true while handlers in notify_handlers_ are being notified.
  bool notifying_handlers_ = false;
  std::vector<IdType> handlers_pending_disable_;

  // The next available notification handler ID.
  size_t next_notify_handler_id_;

  // The GATT client bearer used for ATT requests.
  Client::WeakPtr client_;

  WeakSelf<RemoteCharacteristic> weak_self_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(RemoteCharacteristic);
};

}  // namespace bt::gatt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_CHARACTERISTIC_H_
