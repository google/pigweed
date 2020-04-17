// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_client.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/client.h"

namespace bt {
namespace gatt {
namespace testing {

using att::StatusCallback;

FakeClient::FakeClient(async_dispatcher_t* dispatcher)
    : dispatcher_(dispatcher), weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(dispatcher_);
}

fxl::WeakPtr<Client> FakeClient::AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

uint16_t FakeClient::mtu() const {
  // TODO(armansito): Return a configurable value.
  return att::kLEMinMTU;
}

void FakeClient::ExchangeMTU(MTUCallback callback) {
  auto task = [status = exchange_mtu_status_, mtu = server_mtu_, callback = std::move(callback)] {
    callback(status, mtu);
  };
  async::PostTask(dispatcher_, std::move(task));
}

void FakeClient::DiscoverPrimaryServices(ServiceCallback svc_callback,
                                         StatusCallback status_callback) {
  async::PostTask(dispatcher_, [this, svc_callback = std::move(svc_callback),
                                status_callback = std::move(status_callback)] {
    for (const auto& svc : services_) {
      svc_callback(svc);
    }
    status_callback(service_discovery_status_);
  });
}

void FakeClient::DiscoverCharacteristics(att::Handle range_start, att::Handle range_end,
                                         CharacteristicCallback chrc_callback,
                                         StatusCallback status_callback) {
  last_chrc_discovery_start_handle_ = range_start;
  last_chrc_discovery_end_handle_ = range_end;
  chrc_discovery_count_++;

  async::PostTask(dispatcher_,
                  [this, range_start, range_end, chrc_callback = std::move(chrc_callback),
                   status_callback = std::move(status_callback)] {
                    for (const auto& chrc : chrcs_) {
                      if (chrc.handle >= range_start && chrc.handle <= range_end) {
                        chrc_callback(chrc);
                      }
                    }
                    status_callback(chrc_discovery_status_);
                  });
}

void FakeClient::DiscoverDescriptors(att::Handle range_start, att::Handle range_end,
                                     DescriptorCallback desc_callback,
                                     StatusCallback status_callback) {
  last_desc_discovery_start_handle_ = range_start;
  last_desc_discovery_end_handle_ = range_end;
  desc_discovery_count_++;

  att::Status status;
  if (!desc_discovery_status_target_ || desc_discovery_count_ == desc_discovery_status_target_) {
    status = desc_discovery_status_;
  }

  async::PostTask(dispatcher_,
                  [this, status, range_start, range_end, desc_callback = std::move(desc_callback),
                   status_callback = std::move(status_callback)] {
                    for (const auto& desc : descs_) {
                      if (desc.handle >= range_start && desc.handle <= range_end) {
                        desc_callback(desc);
                      }
                    }
                    status_callback(status);
                  });
}

void FakeClient::ReadRequest(att::Handle handle, ReadCallback callback) {
  if (read_request_callback_) {
    read_request_callback_(handle, std::move(callback));
  }
}

void FakeClient::ReadByTypeRequest(const UUID& type, att::Handle start_handle,
                                   att::Handle end_handle, ReadByTypeCallback callback) {
  if (read_by_type_request_callback_) {
    read_by_type_request_callback_(type, start_handle, end_handle, std::move(callback));
  }
}

void FakeClient::ReadBlobRequest(att::Handle handle, uint16_t offset, ReadCallback callback) {
  if (read_blob_request_callback_) {
    read_blob_request_callback_(handle, offset, std::move(callback));
  }
}

void FakeClient::WriteRequest(att::Handle handle, const ByteBuffer& value,
                              StatusCallback callback) {
  if (write_request_callback_) {
    write_request_callback_(handle, value, std::move(callback));
  }
}

void FakeClient::ExecutePrepareWrites(att::PrepareWriteQueue write_queue,
                                      ReliableMode reliable_mode, att::StatusCallback callback) {
  if (execute_prepare_writes_callback_) {
    execute_prepare_writes_callback_(std::move(write_queue), std::move(callback));
  }
}

void FakeClient::PrepareWriteRequest(att::Handle handle, uint16_t offset,
                                     const ByteBuffer& part_value, PrepareCallback callback) {
  if (prepare_write_request_callback_) {
    prepare_write_request_callback_(handle, offset, part_value, std::move(callback));
  }
}
void FakeClient::ExecuteWriteRequest(att::ExecuteWriteFlag flag, att::StatusCallback callback) {
  if (execute_write_request_callback_) {
    execute_write_request_callback_(flag, std::move(callback));
  }
}

void FakeClient::WriteWithoutResponse(att::Handle handle, const ByteBuffer& value) {
  if (write_without_rsp_callback_) {
    write_without_rsp_callback_(handle, value);
  }
}

void FakeClient::SendNotification(bool indicate, att::Handle handle, const ByteBuffer& value) {
  if (notification_callback_) {
    notification_callback_(indicate, handle, value);
  }
}

void FakeClient::SetNotificationHandler(NotificationCallback callback) {
  notification_callback_ = std::move(callback);
}

}  // namespace testing
}  // namespace gatt
}  // namespace bt
