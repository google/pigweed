// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_H_

#include <lib/fit/function.h>
#include <lib/fit/result.h>

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/client.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_characteristic.h"

namespace bt::gatt {

class RemoteService;

using ServiceList = std::vector<WeakSelf<RemoteService>::WeakPtr>;

// Callback type invoked when GATT services are removed, added, or modified.
// `added` and `modified` are not combined into `updated` for flexibility and debuggability.
// Modified service handles are not included in `removed`.
// NOTE: `removed` services should be handled first because they may share handles with `added`
// services.
using RemoteServiceWatcher =
    fit::function<void(std::vector<att::Handle> removed, ServiceList added, ServiceList modified)>;

using ServiceListCallback = fit::function<void(att::Result<>, ServiceList)>;

using DescriptorMap = std::map<DescriptorHandle, DescriptorData>;
using CharacteristicMap =
    std::map<CharacteristicHandle, std::pair<CharacteristicData, DescriptorMap>>;

namespace internal {
class RemoteServiceManager;
}  // namespace internal

// Represents the state of a GATT service that was discovered on a remote
// device. Clients can interact with a remote GATT service by obtaining a
// RemoteService object from the GATT system.
class RemoteService final {
 public:
  // In production, a RemoteService should only be constructed by a RemoteServiceManager.
  // The constructor and destructor are made available for testing.
  RemoteService(const ServiceData& service_data, Client::WeakPtr client);
  ~RemoteService();

  // If true, a Service Changed notification for this service was received. This service may no
  // longer exist or may have been modified (and so no writes should be performed upon destruction).
  void set_service_changed(bool service_changed) { service_changed_ = service_changed; }

  const ServiceData& info() const { return service_data_; }

  // Returns the service range start handle. This is used to uniquely identify
  // this service.
  att::Handle handle() const { return service_data_.range_start; }

  // Returns the service UUID.
  const UUID& uuid() const { return service_data_.type; }

  // The current ATT_MTU.
  uint16_t att_mtu() const { return client_->mtu(); }

  // Adds a handler which will be called when this service gets removed.
  // Returns false if the service was already shut down.
  bool AddRemovedHandler(fit::closure handler);

  // Returns true if all contents of this service have been discovered. This is primarily intended
  // for unit tests. Clients should not rely on this and use DiscoverCharacteristics() to guarantee
  // discovery.
  bool IsDiscovered() const;

  // Performs characteristic discovery and reports the result asynchronously in
  // |callback|. Returns the cached results if characteristics were already
  // discovered.
  using CharacteristicCallback = fit::function<void(att::Result<>, const CharacteristicMap&)>;
  void DiscoverCharacteristics(CharacteristicCallback callback);

  // Sends a read request to the characteristic with the given identifier. Fails
  // if characteristics have not been discovered.
  // |maybe_truncated| indicates whether the full value might be longer than the reported value.
  using ReadValueCallback =
      fit::function<void(att::Result<>, const ByteBuffer&, bool maybe_truncated)>;
  void ReadCharacteristic(CharacteristicHandle id, ReadValueCallback callback);

  // Performs the "Read Long Characteristic Values" procedure which allows
  // characteristic values larger than the ATT_MTU to be read over multiple
  // requests.
  //
  // The read will start at |offset| and will return at most |max_bytes| octets.
  // The resulting value will be returned via |callback|.
  // The value of |maybe_truncated| reported to the callback indicates whether the full value may be
  // larger than the reported value. This is only possible if |max_bytes| are read, and |max_bytes|
  // is less than att::kMaxAttributeValueLength.
  void ReadLongCharacteristic(CharacteristicHandle id, uint16_t offset, size_t max_bytes,
                              ReadValueCallback callback);

  // Sends a read by type request for attribute values in this service with the given |type|
  // and returns read values via |callback|. If no matching attributes are found, the callback
  // status will indicate success and the vector of values will be empty.
  //
  // If a permission error occurs for an attribute, the error and handle of the attribute that
  // caused the error will be included in the results and the overall status will indicate success.
  // If a general error occurs, the status will indicate the error and no results will be returned.
  //
  // |type| must be the UUID of a characteristic or descriptor value, NOT an internal GATT UUID
  // such as a service or characteristic declaration (the callback will be invoked with an error in
  // this case).
  //
  // NOTE: The values returned may be truncated, as indicated by ReadByTypeResult.maybe_truncated.
  // ReadCharacteristic(), ReadLongCharacteristic(), ReadDescriptor(), and ReadLongDescriptor()
  // should be used to read complete values.
  struct ReadByTypeResult {
    CharacteristicHandle handle;
    fit::result<att::ErrorCode, ByteBufferPtr> result;
    bool maybe_truncated;
  };
  using ReadByTypeCallback = fit::function<void(att::Result<>, std::vector<ReadByTypeResult>)>;
  void ReadByType(const UUID& type, ReadByTypeCallback callback);

  // Sends a write request to the characteristic with the given identifier.
  //
  // TODO(armansito): Add a ByteBuffer version.
  void WriteCharacteristic(CharacteristicHandle id, std::vector<uint8_t> value,
                           att::ResultFunction<> callback);

  // Sends a write request to the characteristic with the given identifier at
  // the given offset, will write over multiple requests if needed. Fails if
  // characteristics have not been discovered.
  //
  // TODO(armansito): Add a ByteBuffer version.
  void WriteLongCharacteristic(CharacteristicHandle id, uint16_t offset, std::vector<uint8_t> value,
                               ReliableMode reliable_mode, att::ResultFunction<> callback);

  // Sends a "Write Without Response" to the characteristic with the given
  // identifier. Fails if characteristics have not been discovered.
  void WriteCharacteristicWithoutResponse(CharacteristicHandle id, std::vector<uint8_t> value,
                                          att::ResultFunction<> cb);

  // Performs the "Read Characteristic Descriptors" procedure (v5.0, Vol 3, Part
  // G, 4.12.1).
  // The callback parameter |maybe_truncated| indicates whether the full value might be longer than
  // the reported value.
  void ReadDescriptor(DescriptorHandle id, ReadValueCallback callback);

  // Performs the "Read Long Characteristic Descriptors" procedure (v5.0, Vol 3,
  // Part G, 4.12.2).
  // The callback parameter |maybe_truncated| indicates whether the full value may be
  // larger than the reported value. This is only possible if |max_bytes| are read, and |max_bytes|
  // is less than att::kMaxAttributeValueLength.
  void ReadLongDescriptor(DescriptorHandle id, uint16_t offset, size_t max_bytes,
                          ReadValueCallback callback);

  // Performs the "Write Characteristic Descriptors" procedure (v5.0, Vol 3,
  // Part G, 4.12.3).
  //
  // TODO(armansito): Add a ByteBuffer version.
  void WriteDescriptor(DescriptorHandle id, std::vector<uint8_t> value,
                       att::ResultFunction<> callback);

  // Performs the "Write Long Characteristic Descriptors" procedure (v5.0, Vol 3,
  // Part G, 4.12.4).
  //
  // TODO(armansito): Add a ByteBuffer version.
  void WriteLongDescriptor(DescriptorHandle id, uint16_t offset, std::vector<uint8_t> value,
                           att::ResultFunction<> callback);

  // Subscribe to characteristic handle/value notifications or indications
  // from the characteristic with the given identifier. Either notifications or
  // indications will be enabled depending on the characteristic properties.
  //
  // This method can be called more than once to register multiple subscribers.
  // The remote Client Characteristic Configuration descriptor will be written
  // only if this is called for the first subscriber.
  //
  // |status_callback| will be called with the status of the operation. On
  // success, a |handler_id| will be returned that can be used to unregister the
  // handler.
  //
  // On success, notifications will be delivered to |callback|.
  using ValueCallback = RemoteCharacteristic::ValueCallback;
  using NotifyStatusCallback = RemoteCharacteristic::NotifyStatusCallback;
  void EnableNotifications(CharacteristicHandle id, ValueCallback callback,
                           NotifyStatusCallback status_callback);

  // Disables characteristic notifications for the given |handler_id| previously
  // obtained via EnableNotifications. The value of the Client Characteristic
  // Configuration descriptor will be cleared if no subscribers remain.
  void DisableNotifications(CharacteristicHandle characteristic_id, IdType handler_id,
                            att::ResultFunction<> status_callback);

  // Simulate receiving a notification. HandleNotification is usually called by
  // RemoteServiceManager, but tests without a RemoteServiceManager may use this method.
  void HandleNotificationForTesting(att::Handle value_handle, const ByteBuffer& value,
                                    bool maybe_truncated) {
    HandleNotification(value_handle, value, maybe_truncated);
  }

  using WeakPtr = WeakSelf<RemoteService>::WeakPtr;
  RemoteService::WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

 private:
  friend class internal::RemoteServiceManager;

  static constexpr size_t kSentinel = std::numeric_limits<size_t>::max();

  // Returns a pointer to the characteristic with |id|. Returns nullptr if not
  // found.
  fit::result<Error<>> GetCharacteristic(CharacteristicHandle id, RemoteCharacteristic** out_char);

  // Returns a pointer to the characteristic descriptor with |id|. Returns
  // nullptr if not found.
  fit::result<Error<>> GetDescriptor(DescriptorHandle id, const DescriptorData** out_desc);

  // Called immediately after characteristic discovery to initiate descriptor
  // discovery.
  void StartDescriptorDiscovery();

  // Completes all pending characteristic discovery requests.
  void CompleteCharacteristicDiscovery(att::Result<> status);

  // Breaks Long Write requests down into a PrepareWriteQueue, then enqueues
  // for the client to process. Drives the "Write Long Characteristic/
  // Descriptor Values" procedure. Called by WriteCharacteristic() and
  // WriteDescriptor().
  void SendLongWriteRequest(att::Handle value_handle, uint16_t offset, BufferView value,
                            ReliableMode reliable_mode, att::ResultFunction<> callback);

  // Helper function that drives the recursive "Read Long Characteristic Values"
  // procedure. Called by ReadLongCharacteristic().
  void ReadLongHelper(att::Handle value_handle, uint16_t offset, MutableByteBufferPtr buffer,
                      size_t bytes_read, ReadValueCallback callback);

  // Helper function that drives the recursive "Read by Type" procedure.
  // Accumulates attribute values in |values| until either |start| > |end| or an error occurs. On
  // completion, accumulated |values| and the status are passed to |callback|.
  // Called by ReadByType().
  void ReadByTypeHelper(const UUID& type, att::Handle start, att::Handle end,
                        std::vector<ReadByTypeResult> values, ReadByTypeCallback callback);

  // Returns true if characteristic discovery has completed.
  inline bool HasCharacteristics() const { return remaining_descriptor_requests_ == 0u; }

  // Called by RemoteServiceManager when a notification is received for one of
  // this service's characteristics.
  void HandleNotification(att::Handle value_handle, const ByteBuffer& value, bool maybe_truncated);

  ServiceData service_data_;

  // The GATT Client bearer for performing remote procedures.
  Client::WeakPtr client_;

  // Queued discovery requests.
  using PendingDiscoveryList = std::vector<CharacteristicCallback>;
  PendingDiscoveryList pending_discov_reqs_;

  // The known characteristics of this service. If not |characteristics_ready_|,
  // this may contain a partial list of characteristics stored during the
  // discovery process.
  //
  // The id of each characteristic corresponds to its index in this vector.
  std::map<CharacteristicHandle, RemoteCharacteristic> characteristics_;

  // The number of pending characteristic descriptor discoveries.
  // Characteristics get marked as ready when this number reaches 0.
  size_t remaining_descriptor_requests_;

  // Indicates whether the service was changed, as indicated by a service changed notification.
  bool service_changed_ = false;

  // Called by ShutDown().
  std::vector<fit::callback<void()>> rm_handlers_;

  WeakSelf<RemoteService> weak_self_{this};

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(RemoteService);
};

}  // namespace bt::gatt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_H_
