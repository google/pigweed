// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_service.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/run_or_post.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

namespace bt {
namespace gatt {

using att::Status;
using att::StatusCallback;

namespace {

void ReportStatus(Status status, StatusCallback callback, async_dispatcher_t* dispatcher) {
  RunOrPost([status, cb = std::move(callback)] { cb(status); }, dispatcher);
}

void ReportValue(att::Status status, const ByteBuffer& value,
                 RemoteService::ReadValueCallback callback, async_dispatcher_t* dispatcher) {
  if (!dispatcher) {
    callback(status, value);
    return;
  }

  // TODO(armansito): Consider making att::Bearer return the ATT PDU buffer
  // directly which would remove the need for a copy.

  auto buffer = NewSlabBuffer(value.size());
  value.Copy(buffer.get());

  async::PostTask(dispatcher, [status, callback = std::move(callback), val = std::move(buffer)] {
    callback(status, *val);
  });
}

}  // namespace

// static
constexpr size_t RemoteService::kSentinel;

RemoteService::RemoteService(const ServiceData& service_data, fxl::WeakPtr<Client> client,
                             async_dispatcher_t* gatt_dispatcher)
    : service_data_(service_data),
      gatt_dispatcher_(gatt_dispatcher),
      client_(client),
      remaining_descriptor_requests_(kSentinel),
      shut_down_(false) {
  ZX_DEBUG_ASSERT(client_);
  ZX_DEBUG_ASSERT(gatt_dispatcher_);
}

RemoteService::~RemoteService() {
  std::lock_guard<std::mutex> lock(mtx_);
  ZX_DEBUG_ASSERT(!alive());
}

void RemoteService::ShutDown() {
  ZX_DEBUG_ASSERT(IsOnGattThread());

  std::vector<PendingClosure> rm_handlers;

  {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!alive()) {
      return;
    }

    for (auto& chr : characteristics_) {
      chr.ShutDown();
    }

    shut_down_ = true;
    rm_handlers = std::move(rm_handlers_);
  }

  for (auto& handler : rm_handlers) {
    RunOrPost(std::move(handler.callback), handler.dispatcher);
  }
}

bool RemoteService::AddRemovedHandler(fit::closure handler, async_dispatcher_t* dispatcher) {
  std::lock_guard<std::mutex> lock(mtx_);

  if (!alive())
    return false;

  rm_handlers_.emplace_back(std::move(handler), dispatcher);
  return true;
}

void RemoteService::DiscoverCharacteristics(CharacteristicCallback callback,
                                            async_dispatcher_t* dispatcher) {
  RunGattTask([this, cb = std::move(callback), dispatcher]() mutable {
    if (shut_down_) {
      ReportCharacteristics(Status(HostError::kFailed), std::move(cb), dispatcher);
      return;
    }

    // Characteristics already discovered. Return success.
    if (HasCharacteristics()) {
      ReportCharacteristics(Status(), std::move(cb), dispatcher);
      return;
    }

    // Queue this request.
    pending_discov_reqs_.emplace_back(std::move(cb), dispatcher);

    // Nothing to do if a write request is already pending.
    if (pending_discov_reqs_.size() > 1u)
      return;

    auto self = fbl::WrapRefPtr(this);
    auto chrc_cb = [self](const CharacteristicData& chrc) {
      if (!self->shut_down_) {
        IdType id = self->characteristics_.size();
        self->characteristics_.emplace_back(self->client_, id, chrc);
      }
    };

    auto res_cb = [self](Status status) mutable {
      if (self->shut_down_) {
        status = Status(HostError::kFailed);
      }

      if (bt_is_error(status, TRACE, "gatt", "characteristic discovery failed")) {
        self->characteristics_.clear();
      }

      if (self->characteristics_.empty()) {
        if (status) {
          // This marks that characteristic discovery has completed
          // successfully.
          self->remaining_descriptor_requests_ = 0u;
        }

        // Skip descriptor discovery and end the procedure as no characteristics
        // were found (or the operation failed).
        self->CompleteCharacteristicDiscovery(status);
        return;
      }

      self->StartDescriptorDiscovery();
    };

    client_->DiscoverCharacteristics(service_data_.range_start, service_data_.range_end,
                                     std::move(chrc_cb), std::move(res_cb));
  });
}

bool RemoteService::IsDiscovered() const {
  // TODO(armansito): Return true only if included services have also been
  // discovered.
  return HasCharacteristics();
}

void RemoteService::ReadCharacteristic(IdType id, ReadValueCallback cb,
                                       async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, cb = std::move(cb), dispatcher]() mutable {
    RemoteCharacteristic* chrc;
    att::Status status = att::Status(GetCharacteristic(id, &chrc));
    ZX_DEBUG_ASSERT(chrc || !status);
    if (!status) {
      ReportValue(status, BufferView(), std::move(cb), dispatcher);
      return;
    }

    if (!(chrc->info().properties & Property::kRead)) {
      bt_log(TRACE, "gatt", "characteristic does not support \"read\"");
      ReportValue(att::Status(HostError::kNotSupported), BufferView(), std::move(cb), dispatcher);
      return;
    }

    SendReadRequest(chrc->info().value_handle, std::move(cb), dispatcher);
  });
}

void RemoteService::ReadLongCharacteristic(IdType id, uint16_t offset, size_t max_bytes,
                                           ReadValueCallback cb, async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, offset, max_bytes, cb = std::move(cb), dispatcher]() mutable {
    RemoteCharacteristic* chrc;
    att::Status status = att::Status(GetCharacteristic(id, &chrc));
    ZX_DEBUG_ASSERT(chrc || !status);
    if (!status) {
      ReportValue(status, BufferView(), std::move(cb), dispatcher);
      return;
    }

    if (!(chrc->info().properties & Property::kRead)) {
      bt_log(TRACE, "gatt", "characteristic does not support \"read\"");
      ReportValue(att::Status(HostError::kNotSupported), BufferView(), std::move(cb), dispatcher);
      return;
    }

    if (max_bytes == 0) {
      bt_log(SPEW, "gatt", "invalid value for |max_bytes|: 0");
      ReportValue(att::Status(HostError::kInvalidParameters), BufferView(), std::move(cb),
                  dispatcher);
      return;
    }

    // Set up the buffer in which we'll accumulate the blobs.
    auto buffer = NewSlabBuffer(std::min(max_bytes, att::kMaxAttributeValueLength));
    if (!buffer) {
      ReportValue(att::Status(HostError::kOutOfMemory), BufferView(), std::move(cb), dispatcher);
      return;
    }

    ReadLongHelper(chrc->info().value_handle, offset, std::move(buffer), 0u /* bytes_read */,
                   std::move(cb), dispatcher);
  });
}

void RemoteService::WriteCharacteristic(IdType id, std::vector<uint8_t> value, StatusCallback cb,
                                        async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, value = std::move(value), cb = std::move(cb), dispatcher]() mutable {
    RemoteCharacteristic* chrc;
    Status status = Status(GetCharacteristic(id, &chrc));
    ZX_DEBUG_ASSERT(chrc || !status);
    if (!status) {
      ReportStatus(status, std::move(cb), dispatcher);
      return;
    }

    if (!(chrc->info().properties & Property::kWrite)) {
      bt_log(TRACE, "gatt", "characteristic does not support \"write\"");
      ReportStatus(Status(HostError::kNotSupported), std::move(cb), dispatcher);
      return;
    }

    SendWriteRequest(chrc->info().value_handle, BufferView(value.data(), value.size()),
                     std::move(cb), dispatcher);
  });
}

void RemoteService::WriteLongCharacteristic(IdType id, uint16_t offset, std::vector<uint8_t> value,
                                            StatusCallback cb, async_dispatcher_t* dispatcher) {
  RunGattTask(
      [this, id, offset, value = std::move(value), cb = std::move(cb), dispatcher]() mutable {
        RemoteCharacteristic* chrc;
        Status status = Status(GetCharacteristic(id, &chrc));
        ZX_DEBUG_ASSERT(chrc || !status);
        if (!status) {
          ReportStatus(status, std::move(cb), dispatcher);
          return;
        }

        if (!(chrc->info().properties & Property::kWrite)) {
          bt_log(TRACE, "gatt", "characteristic does not support \"write\"");
          ReportStatus(Status(HostError::kNotSupported), std::move(cb), dispatcher);
          return;
        }

        SendLongWriteRequest(chrc->info().value_handle, offset,
                             BufferView(value.data(), value.size()), std::move(cb), dispatcher);
      });
}

void RemoteService::WriteCharacteristicWithoutResponse(IdType id, std::vector<uint8_t> value) {
  RunGattTask([this, id, value = std::move(value)]() mutable {
    RemoteCharacteristic* chrc;
    Status status = Status(GetCharacteristic(id, &chrc));
    ZX_DEBUG_ASSERT(chrc || !status);
    if (!status) {
      return;
    }

    if (!(chrc->info().properties & Property::kWriteWithoutResponse)) {
      bt_log(TRACE, "gatt", "characteristic does not support \"write without response\"");
      return;
    }

    client_->WriteWithoutResponse(chrc->info().value_handle,
                                  BufferView(value.data(), value.size()));
  });
}

void RemoteService::ReadDescriptor(IdType id, ReadValueCallback cb,
                                   async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, cb = std::move(cb), dispatcher]() mutable {
    const RemoteCharacteristic::Descriptor* desc;
    att::Status status = att::Status(GetDescriptor(id, &desc));
    ZX_DEBUG_ASSERT(desc || !status);
    if (!status) {
      ReportValue(status, BufferView(), std::move(cb), dispatcher);
      return;
    }

    SendReadRequest(desc->info().handle, std::move(cb), dispatcher);
  });
}

void RemoteService::ReadLongDescriptor(IdType id, uint16_t offset, size_t max_bytes,
                                       ReadValueCallback cb, async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, offset, max_bytes, cb = std::move(cb), dispatcher]() mutable {
    const RemoteCharacteristic::Descriptor* desc;
    att::Status status = att::Status(GetDescriptor(id, &desc));
    ZX_DEBUG_ASSERT(desc || !status);
    if (!status) {
      ReportValue(status, BufferView(), std::move(cb), dispatcher);
      return;
    }

    if (max_bytes == 0) {
      bt_log(SPEW, "gatt", "invalid value for |max_bytes|: 0");
      ReportValue(att::Status(HostError::kInvalidParameters), BufferView(), std::move(cb),
                  dispatcher);
      return;
    }

    // Set up the buffer in which we'll accumulate the blobs.
    auto buffer = NewSlabBuffer(std::min(max_bytes, att::kMaxAttributeValueLength));
    if (!buffer) {
      ReportValue(att::Status(HostError::kOutOfMemory), BufferView(), std::move(cb), dispatcher);
      return;
    }

    ReadLongHelper(desc->info().handle, offset, std::move(buffer), 0u /* bytes_read */,
                   std::move(cb), dispatcher);
  });
}

void RemoteService::WriteDescriptor(IdType id, std::vector<uint8_t> value, att::StatusCallback cb,
                                    async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, value = std::move(value), cb = std::move(cb), dispatcher]() mutable {
    const RemoteCharacteristic::Descriptor* desc;
    Status status = Status(GetDescriptor(id, &desc));
    ZX_DEBUG_ASSERT(desc || !status);
    if (!status) {
      ReportStatus(status, std::move(cb), dispatcher);
      return;
    }

    // Do not allow writing to internally reserved descriptors.
    if (desc->info().type == types::kClientCharacteristicConfig) {
      bt_log(TRACE, "gatt", "writing to CCC descriptor not allowed");
      ReportStatus(Status(HostError::kNotSupported), std::move(cb), dispatcher);
      return;
    }

    SendWriteRequest(desc->info().handle, BufferView(value.data(), value.size()), std::move(cb),
                     dispatcher);
  });
}

void RemoteService::WriteLongDescriptor(IdType id, uint16_t offset, std::vector<uint8_t> value,
                                        att::StatusCallback cb, async_dispatcher_t* dispatcher) {
  RunGattTask(
      [this, id, offset, value = std::move(value), cb = std::move(cb), dispatcher]() mutable {
        const RemoteCharacteristic::Descriptor* desc;
        Status status = Status(GetDescriptor(id, &desc));
        ZX_DEBUG_ASSERT(desc || !status);
        if (!status) {
          ReportStatus(status, std::move(cb), dispatcher);
          return;
        }

        // Do not allow writing to internally reserved descriptors.
        if (desc->info().type == types::kClientCharacteristicConfig) {
          bt_log(TRACE, "gatt", "writing to CCC descriptor not allowed");
          ReportStatus(Status(HostError::kNotSupported), std::move(cb), dispatcher);
          return;
        }

        SendLongWriteRequest(desc->info().handle, offset, BufferView(value.data(), value.size()),
                             std::move(cb), dispatcher);
      });
}

void RemoteService::EnableNotifications(IdType id, ValueCallback callback,
                                        NotifyStatusCallback status_callback,
                                        async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, cb = std::move(callback), status_cb = std::move(status_callback),
               dispatcher]() mutable {
    RemoteCharacteristic* chrc;
    att::Status status = att::Status(GetCharacteristic(id, &chrc));
    ZX_DEBUG_ASSERT(chrc || !status);
    if (!status) {
      RunOrPost([status, cb = std::move(status_cb)] { cb(status, kInvalidId); }, dispatcher);
      return;
    }

    chrc->EnableNotifications(std::move(cb), std::move(status_cb), dispatcher);
  });
}

void RemoteService::DisableNotifications(IdType id, IdType handler_id,
                                         StatusCallback status_callback,
                                         async_dispatcher_t* dispatcher) {
  RunGattTask([this, id, handler_id, cb = std::move(status_callback), dispatcher]() mutable {
    RemoteCharacteristic* chrc;
    att::Status status = att::Status(GetCharacteristic(id, &chrc));
    ZX_DEBUG_ASSERT(chrc || !status);
    if (status && !chrc->DisableNotifications(handler_id)) {
      status = att::Status(HostError::kNotFound);
    }
    ReportStatus(status, std::move(cb), dispatcher);
  });
}

void RemoteService::StartDescriptorDiscovery() {
  ZX_DEBUG_ASSERT(IsOnGattThread());
  ZX_DEBUG_ASSERT(!pending_discov_reqs_.empty());

  ZX_DEBUG_ASSERT(characteristics_.size());
  remaining_descriptor_requests_ = characteristics_.size();

  auto self = fbl::WrapRefPtr(this);

  // Callback called for each characteristic. This may be called in any
  // order since we request the descriptors of all characteristics all at
  // once.
  auto desc_done_callback = [self](att::Status status) {
    // Do nothing if discovery was concluded earlier (which would have cleared
    // the pending discovery requests).
    if (self->pending_discov_reqs_.empty())
      return;

    // Report an error if the service was removed.
    if (self->shut_down_) {
      status = att::Status(HostError::kFailed);
    }

    if (status) {
      self->remaining_descriptor_requests_ -= 1;

      // Defer handling
      if (self->remaining_descriptor_requests_ > 0)
        return;

      // HasCharacteristics() should return true now.
      ZX_DEBUG_ASSERT(self->HasCharacteristics());

      // Fall through and notify clients below.
    } else {
      ZX_DEBUG_ASSERT(!self->HasCharacteristics());
      bt_log(TRACE, "gatt", "descriptor discovery failed %s", status.ToString().c_str());
      self->characteristics_.clear();

      // Fall through and notify the clients below.
    }

    self->CompleteCharacteristicDiscovery(status);
  };

  for (size_t i = 0; i < characteristics_.size(); ++i) {
    // We determine the range end handle based on the start handle of the next
    // characteristic. The characteristic ends with the service range if this is
    // the last characteristic.
    att::Handle end_handle;

    if (i == characteristics_.size() - 1) {
      end_handle = service_data_.range_end;
    } else {
      end_handle = characteristics_[i + 1].info().handle - 1;
    }

    ZX_DEBUG_ASSERT(client_);
    characteristics_[i].DiscoverDescriptors(end_handle, desc_done_callback);
  }
}

bool RemoteService::IsOnGattThread() const {
  return async_get_default_dispatcher() == gatt_dispatcher_;
}

HostError RemoteService::GetCharacteristic(IdType id, RemoteCharacteristic** out_char) {
  ZX_DEBUG_ASSERT(IsOnGattThread());
  ZX_DEBUG_ASSERT(out_char);

  if (shut_down_)
    return HostError::kFailed;

  if (!HasCharacteristics())
    return HostError::kNotReady;

  if (id >= characteristics_.size())
    return HostError::kNotFound;

  *out_char = &characteristics_[id];
  return HostError::kNoError;
}

HostError RemoteService::GetDescriptor(IdType id,
                                       const RemoteCharacteristic::Descriptor** out_desc) {
  ZX_DEBUG_ASSERT(IsOnGattThread());
  ZX_DEBUG_ASSERT(out_desc);

  if (shut_down_)
    return HostError::kFailed;

  if (!HasCharacteristics())
    return HostError::kNotReady;

  // The second set of 16-bits of |id| represent the characteristic ID and the
  // lower bits are the descriptor index. (See the section titled "ID SCHEME" in
  // remote_characteristic.h)
  IdType desc_idx = id & 0xFFFF;
  IdType chrc_idx = (id >> 16) & 0xFFFF;

  if (chrc_idx >= characteristics_.size())
    return HostError::kNotFound;

  auto* chrc = &characteristics_[chrc_idx];
  if (desc_idx >= chrc->descriptors().size())
    return HostError::kNotFound;

  *out_desc = &chrc->descriptors()[desc_idx];
  ZX_DEBUG_ASSERT((*out_desc)->id() == id);

  return HostError::kNoError;
}

void RemoteService::RunGattTask(fit::closure task) {
  // Capture a reference to this object to guarantee its lifetime.
  RunOrPost([objref = fbl::WrapRefPtr(this), task = std::move(task)] { task(); }, gatt_dispatcher_);
}

void RemoteService::ReportCharacteristics(Status status, CharacteristicCallback callback,
                                          async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(IsOnGattThread());
  RunOrPost(
      [self = fbl::WrapRefPtr(this), status, cb = std::move(callback)] {
        // We return a const reference to our |characteristics_| field to avoid
        // copying its contents into this lambda.
        //
        // |characteristics_| is not annotated with __TA_GUARDED() since locking
        // |mtx_| can cause a deadlock if |dispatcher| == nullptr. We
        // guarantee the validity of this data by keeping the public
        // interface of Characteristic small and by never modifying
        // |characteristics_| following discovery.
        cb(status, self->characteristics_);
      },
      dispatcher);
}

void RemoteService::CompleteCharacteristicDiscovery(att::Status status) {
  ZX_DEBUG_ASSERT(!pending_discov_reqs_.empty());
  ZX_DEBUG_ASSERT(!status || remaining_descriptor_requests_ == 0u);

  auto pending = std::move(pending_discov_reqs_);
  for (auto& req : pending) {
    ReportCharacteristics(status, std::move(req.callback), req.dispatcher);
  }
}

void RemoteService::SendReadRequest(att::Handle handle, ReadValueCallback cb,
                                    async_dispatcher_t* dispatcher) {
  client_->ReadRequest(
      handle, [cb = std::move(cb), dispatcher](att::Status status, const auto& value) mutable {
        ReportValue(status, value, std::move(cb), dispatcher);
      });
}

void RemoteService::SendWriteRequest(att::Handle handle, const ByteBuffer& value, StatusCallback cb,
                                     async_dispatcher_t* dispatcher) {
  client_->WriteRequest(handle, value, [cb = std::move(cb), dispatcher](Status status) mutable {
    ReportStatus(status, std::move(cb), dispatcher);
  });
}

void RemoteService::SendLongWriteRequest(att::Handle handle, uint16_t offset, BufferView value,
                                         att::StatusCallback final_cb,
                                         async_dispatcher_t* dispatcher) {
  att::PrepareWriteQueue long_write_queue;
  auto header_ln = sizeof(att::PrepareWriteRequestParams) + sizeof(att::OpCode);
  uint16_t bytes_written = 0;

  // Divide up the long write into it's constituent PreparedWrites and add them
  // to the queue.
  while (bytes_written < value.size()) {
    uint16_t part_value_size = std::min(client_->mtu() - header_ln, value.size() - bytes_written);
    auto part_buffer = value.view(bytes_written, part_value_size);

    long_write_queue.push(att::QueuedWrite(handle, offset, part_buffer));

    bytes_written += part_value_size;
    offset += part_value_size;
  }

  client_->ExecutePrepareWrites(std::move(long_write_queue),
                                [cb = std::move(final_cb), dispatcher](Status status) mutable {
                                  ReportStatus(status, std::move(cb), dispatcher);
                                });
}

void RemoteService::ReadLongHelper(att::Handle value_handle, uint16_t offset,
                                   MutableByteBufferPtr buffer, size_t bytes_read,
                                   ReadValueCallback callback, async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(IsOnGattThread());
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(buffer);
  ZX_DEBUG_ASSERT(!shut_down_);

  // Capture a reference so that this object is alive when the callback runs.
  auto self = fbl::WrapRefPtr(this);
  auto read_blob_cb = [self, value_handle, offset, buffer = std::move(buffer), bytes_read,
                       cb = std::move(callback),
                       dispatcher](att::Status status, const ByteBuffer& blob) mutable {
    if (self->shut_down_) {
      // The service was removed. Report an error.
      ReportValue(att::Status(HostError::kCanceled), BufferView(), std::move(cb), dispatcher);
      return;
    }

    if (!status) {
      ReportValue(status, BufferView(), std::move(cb), dispatcher);
      return;
    }

    // Copy the blob into our |buffer|. |blob| may be truncated depending on the
    // size of |buffer|.
    ZX_DEBUG_ASSERT(bytes_read < buffer->size());
    size_t copy_size = std::min(blob.size(), buffer->size() - bytes_read);
    buffer->Write(blob.view(0, copy_size), bytes_read);
    bytes_read += copy_size;

    // We are done if the blob is smaller than (ATT_MTU - 1) or we have read the
    // maximum number of bytes requested.
    ZX_DEBUG_ASSERT(bytes_read <= buffer->size());
    if (blob.size() < (self->client_->mtu() - 1) || bytes_read == buffer->size()) {
      ReportValue(att::Status(), buffer->view(0, bytes_read), std::move(cb), dispatcher);
      return;
    }

    // We have more bytes to read. Read the next blob.
    self->ReadLongHelper(value_handle, offset + blob.size(), std::move(buffer), bytes_read,
                         std::move(cb), dispatcher);
  };

  client_->ReadBlobRequest(value_handle, offset, std::move(read_blob_cb));
}

void RemoteService::HandleNotification(att::Handle value_handle, const ByteBuffer& value) {
  ZX_DEBUG_ASSERT(IsOnGattThread());

  if (shut_down_)
    return;

  // Find the characteristic with the given value handle.
  auto iter = std::lower_bound(characteristics_.begin(), characteristics_.end(), value_handle,
                               [](const auto& chr, att::Handle value_handle) {
                                 return chr.info().value_handle < value_handle;
                               });
  if (iter != characteristics_.end() && iter->info().value_handle == value_handle) {
    iter->HandleNotification(value);
  }
}

}  // namespace gatt
}  // namespace bt
