// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_CLIENT_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_CLIENT_H_

#include "src/connectivity/bluetooth/core/bt-host/gatt/client.h"

namespace bt {
namespace gatt {
namespace testing {

class FakeClient final : public Client {
 public:
  explicit FakeClient(async_dispatcher_t* dispatcher);
  ~FakeClient() override = default;

  void set_server_mtu(uint16_t mtu) { server_mtu_ = mtu; }
  void set_exchage_mtu_status(att::Status status) {
    exchange_mtu_status_ = status;
  }

  void set_primary_services(std::vector<ServiceData> services) {
    services_ = std::move(services);
  }

  void set_service_discovery_status(att::Status status) {
    service_discovery_status_ = status;
  }

  void set_characteristics(std::vector<CharacteristicData> chrcs) {
    chrcs_ = std::move(chrcs);
  }

  void set_descriptors(std::vector<DescriptorData> descs) {
    descs_ = std::move(descs);
  }

  void set_characteristic_discovery_status(att::Status status) {
    chrc_discovery_status_ = status;
  }

  // If |count| is set to a non-zero value, |status| only applies to |count|th
  // request and all other requests will succeed. Otherwise, |status| applies to
  // all requests.
  void set_descriptor_discovery_status(att::Status status, size_t count = 0) {
    desc_discovery_status_target_ = count;
    desc_discovery_status_ = status;
  }

  att::Handle last_chrc_discovery_start_handle() const {
    return last_chrc_discovery_start_handle_;
  }

  att::Handle last_chrc_discovery_end_handle() const {
    return last_chrc_discovery_end_handle_;
  }

  att::Handle last_desc_discovery_start_handle() const {
    return last_desc_discovery_start_handle_;
  }

  att::Handle last_desc_discovery_end_handle() const {
    return last_desc_discovery_end_handle_;
  }

  size_t chrc_discovery_count() const { return chrc_discovery_count_; }
  size_t desc_discovery_count() const { return desc_discovery_count_; }

  // Sets a callback which will run when ReadRequest gets called.
  using ReadRequestCallback = fit::function<void(att::Handle, ReadCallback)>;
  void set_read_request_callback(ReadRequestCallback callback) {
    read_request_callback_ = std::move(callback);
  }

  // Sets a callback which will run when ReadBlobRequest gets called.
  using ReadBlobRequestCallback =
      fit::function<void(att::Handle, uint16_t offset, ReadCallback)>;
  void set_read_blob_request_callback(ReadBlobRequestCallback callback) {
    read_blob_request_callback_ = std::move(callback);
  }

  // Sets a callback which will run when WriteRequest gets called.
  using WriteRequestCallback = fit::function<void(
      att::Handle, const common::ByteBuffer&, att::StatusCallback)>;
  void set_write_request_callback(WriteRequestCallback callback) {
    write_request_callback_ = std::move(callback);
  }

  // Sets a callback which will run when WriteWithoutResponse gets called.
  using WriteWithoutResponseCallback =
      fit::function<void(att::Handle, const common::ByteBuffer&)>;
  void set_write_without_rsp_callback(WriteWithoutResponseCallback callback) {
    write_without_rsp_callback_ = std::move(callback);
  }

  // Emulates the receipt of a notification or indication PDU.
  void SendNotification(bool indicate, att::Handle handle,
                        const common::ByteBuffer& value);

 private:
  // Client overrides:
  fxl::WeakPtr<Client> AsWeakPtr() override;
  uint16_t mtu() const override;
  void ExchangeMTU(MTUCallback callback) override;
  void DiscoverPrimaryServices(ServiceCallback svc_callback,
                               att::StatusCallback status_callback) override;
  void DiscoverCharacteristics(att::Handle range_start, att::Handle range_end,
                               CharacteristicCallback chrc_callback,
                               att::StatusCallback status_callback) override;
  void DiscoverDescriptors(att::Handle range_start, att::Handle range_end,
                           DescriptorCallback desc_callback,
                           att::StatusCallback status_callback) override;
  void ReadRequest(att::Handle handle, ReadCallback callback) override;
  void ReadBlobRequest(att::Handle handle, uint16_t offset,
                       ReadCallback callback) override;
  void WriteRequest(att::Handle handle, const common::ByteBuffer& value,
                    att::StatusCallback callback) override;
  void WriteWithoutResponse(att::Handle handle,
                            const common::ByteBuffer& value) override;
  void SetNotificationHandler(NotificationCallback callback) override;

  // All callbacks will be posted on this dispatcher to emulate asynchronous
  // behavior.
  async_dispatcher_t* dispatcher_;

  // Value to return for MTU exchange.
  uint16_t server_mtu_ = att::kLEMinMTU;

  // Data used for DiscoveryPrimaryServices().
  std::vector<ServiceData> services_;

  // Fake status values to return for GATT procedures.
  att::Status exchange_mtu_status_;
  att::Status service_discovery_status_;
  att::Status chrc_discovery_status_;

  size_t desc_discovery_status_target_ = 0;
  att::Status desc_discovery_status_;

  // Data used for DiscoverCharacteristics().
  std::vector<CharacteristicData> chrcs_;
  att::Handle last_chrc_discovery_start_handle_ = 0;
  att::Handle last_chrc_discovery_end_handle_ = 0;
  size_t chrc_discovery_count_ = 0;

  // Data used for DiscoverDescriptors().
  std::vector<DescriptorData> descs_;
  att::Handle last_desc_discovery_start_handle_ = 0;
  att::Handle last_desc_discovery_end_handle_ = 0;
  size_t desc_discovery_count_ = 0;

  ReadRequestCallback read_request_callback_;
  ReadBlobRequestCallback read_blob_request_callback_;
  WriteRequestCallback write_request_callback_;
  WriteWithoutResponseCallback write_without_rsp_callback_;
  NotificationCallback notification_callback_;

  fxl::WeakPtrFactory<FakeClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeClient);
};

}  // namespace testing
}  // namespace gatt
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_FAKE_CLIENT_H_
