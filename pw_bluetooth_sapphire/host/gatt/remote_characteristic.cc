// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_characteristic.h"

#include <zircon/assert.h>

#include "client.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/run_or_post.h"
#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator.h"

namespace bt {
namespace gatt {

namespace {

void ReportNotifyStatus(att::Status status, IdType id,
                        RemoteCharacteristic::NotifyStatusCallback callback,
                        async_dispatcher_t* dispatcher) {
  RunOrPost([status, id, cb = std::move(callback)] { cb(status, id); }, dispatcher);
}

void NotifyValue(const ByteBuffer& value, RemoteCharacteristic::ValueCallback callback,
                 async_dispatcher_t* dispatcher) {
  if (!dispatcher) {
    callback(value);
    return;
  }

  auto buffer = NewSlabBuffer(value.size());
  if (buffer) {
    value.Copy(buffer.get());
    async::PostTask(dispatcher,
                    [callback = std::move(callback), val = std::move(buffer)] { callback(*val); });
  } else {
    bt_log(TRACE, "gatt", "out of memory!");
  }
}

}  // namespace

RemoteCharacteristic::PendingNotifyRequest::PendingNotifyRequest(async_dispatcher_t* d,
                                                                 ValueCallback value_cb,
                                                                 NotifyStatusCallback status_cb)
    : dispatcher(d), value_callback(std::move(value_cb)), status_callback(std::move(status_cb)) {
  ZX_DEBUG_ASSERT(value_callback);
  ZX_DEBUG_ASSERT(status_callback);
}

RemoteCharacteristic::NotifyHandler::NotifyHandler(async_dispatcher_t* d, ValueCallback cb)
    : dispatcher(d), callback(std::move(cb)) {
  ZX_DEBUG_ASSERT(callback);
}

RemoteCharacteristic::RemoteCharacteristic(fxl::WeakPtr<Client> client,
                                           const CharacteristicData& info)
    : info_(info),
      discovery_error_(false),
      shut_down_(false),
      ccc_handle_(att::kInvalidHandle),
      next_notify_handler_id_(1u),
      client_(client),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(client_);
}

RemoteCharacteristic::RemoteCharacteristic(RemoteCharacteristic&& other)
    : info_(other.info_),
      discovery_error_(other.discovery_error_),
      shut_down_(other.shut_down_.load()),
      ccc_handle_(other.ccc_handle_),
      next_notify_handler_id_(other.next_notify_handler_id_),
      client_(other.client_),
      weak_ptr_factory_(this) {
  other.weak_ptr_factory_.InvalidateWeakPtrs();
}

void RemoteCharacteristic::ShutDown() {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());

  // Make sure that all weak pointers are invalidated on the GATT thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
  shut_down_ = true;

  if (ccc_handle_ != att::kInvalidHandle) {
    ResolvePendingNotifyRequests(att::Status(HostError::kFailed));

    // Clear the CCC if we have enabled notifications.
    // TODO(armansito): Don't write to the descriptor if ShutDown() was called
    // as a result of a "Service Changed" indication.
    if (!notify_handlers_.empty()) {
      notify_handlers_.clear();
      DisableNotificationsInternal();
    }
  }
}

void RemoteCharacteristic::DiscoverDescriptors(att::Handle range_end,
                                               att::StatusCallback callback) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(client_);
  ZX_DEBUG_ASSERT(callback);
  ZX_DEBUG_ASSERT(!shut_down_);
  ZX_DEBUG_ASSERT(range_end >= info().value_handle);

  discovery_error_ = false;
  descriptors_.clear();

  if (info().value_handle == range_end) {
    callback(att::Status());
    return;
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto desc_cb = [self](const DescriptorData& desc) {
    if (!self)
      return;

    ZX_DEBUG_ASSERT(self->thread_checker_.IsCreationThreadCurrent());
    if (self->discovery_error_)
      return;

    if (desc.type == types::kClientCharacteristicConfig) {
      if (self->ccc_handle_ != att::kInvalidHandle) {
        bt_log(TRACE, "gatt", "characteristic has more than one CCC descriptor!");
        self->discovery_error_ = true;
        return;
      }
      self->ccc_handle_ = desc.handle;
    }

    // As descriptors must be strictly increasing, this emplace should always succeed
    auto [_unused, success] = self->descriptors_.try_emplace(DescriptorHandle(desc.handle), desc);
    ZX_DEBUG_ASSERT(success);
  };

  auto status_cb = [self, cb = std::move(callback)](att::Status status) {
    if (!self) {
      cb(att::Status(HostError::kFailed));
      return;
    }

    ZX_DEBUG_ASSERT(self->thread_checker_.IsCreationThreadCurrent());

    if (self->discovery_error_) {
      status = att::Status(HostError::kFailed);
    }

    if (!status) {
      self->descriptors_.clear();
    }
    cb(status);
  };

  client_->DiscoverDescriptors(info().value_handle + 1, range_end, std::move(desc_cb),
                               std::move(status_cb));
}

void RemoteCharacteristic::EnableNotifications(ValueCallback value_callback,
                                               NotifyStatusCallback status_callback,
                                               async_dispatcher_t* dispatcher) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(client_);
  ZX_DEBUG_ASSERT(value_callback);
  ZX_DEBUG_ASSERT(status_callback);
  ZX_DEBUG_ASSERT(!shut_down_);

  if (!(info().properties & (Property::kNotify | Property::kIndicate)) ||
      ccc_handle_ == att::kInvalidHandle) {
    bt_log(TRACE, "gatt", "characteristic does not support notifications");
    ReportNotifyStatus(att::Status(HostError::kNotSupported), kInvalidId,
                       std::move(status_callback), dispatcher);
    return;
  }

  // If notifications are already enabled then succeed right away.
  if (!notify_handlers_.empty()) {
    ZX_DEBUG_ASSERT(pending_notify_reqs_.empty());

    IdType id = next_notify_handler_id_++;
    notify_handlers_[id] = NotifyHandler(dispatcher, std::move(value_callback));
    ReportNotifyStatus(att::Status(), id, std::move(status_callback), dispatcher);
    return;
  }

  pending_notify_reqs_.emplace(dispatcher, std::move(value_callback), std::move(status_callback));

  // If there are other pending requests to enable notifications then we'll wait
  // until the descriptor write completes.
  if (pending_notify_reqs_.size() > 1u)
    return;

  StaticByteBuffer<2> ccc_value;
  ccc_value.SetToZeros();

  // Enable indications if supported. Otherwise enable notifications.
  if (info().properties & Property::kIndicate) {
    ccc_value[0] = static_cast<uint8_t>(kCCCIndicationBit);
  } else {
    ccc_value[0] = static_cast<uint8_t>(kCCCNotificationBit);
  }

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto ccc_write_cb = [self](att::Status status) {
    bt_log(TRACE, "gatt", "CCC write status (enable): %s", status.ToString().c_str());
    if (self) {
      self->ResolvePendingNotifyRequests(status);
    }
  };

  client_->WriteRequest(ccc_handle_, ccc_value, std::move(ccc_write_cb));
}

bool RemoteCharacteristic::DisableNotifications(IdType handler_id) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(client_);
  ZX_DEBUG_ASSERT(!shut_down_);

  if (!notify_handlers_.erase(handler_id)) {
    bt_log(SPEW, "gatt", "notify handler not found (id: %lu)", handler_id);
    return false;
  }

  if (!notify_handlers_.empty())
    return true;

  DisableNotificationsInternal();
  return true;
}

void RemoteCharacteristic::DisableNotificationsInternal() {
  ZX_DEBUG_ASSERT(ccc_handle_ != att::kInvalidHandle);

  if (!client_) {
    bt_log(SPEW, "gatt", "client bearer invalid!");
    return;
  }

  // Disable notifications.
  StaticByteBuffer<2> ccc_value;
  ccc_value.SetToZeros();

  auto ccc_write_cb = [](att::Status status) {
    bt_log(TRACE, "gatt", "CCC write status (disable): %s", status.ToString().c_str());
  };

  // We send the request without handling the status as there is no good way to
  // recover from failing to disable notifications. If the peer continues to
  // send notifications, they will be dropped as no handlers are registered.
  client_->WriteRequest(ccc_handle_, ccc_value, std::move(ccc_write_cb));
}

void RemoteCharacteristic::ResolvePendingNotifyRequests(att::Status status) {
  // Move the contents of the queue so that a handler can remove itself (this
  // matters when no dispatcher is provided).
  auto pending = std::move(pending_notify_reqs_);
  while (!pending.empty()) {
    auto req = std::move(pending.front());
    pending.pop();

    IdType id = kInvalidId;

    if (status) {
      id = next_notify_handler_id_++;
      notify_handlers_[id] = NotifyHandler(req.dispatcher, std::move(req.value_callback));
    }

    ReportNotifyStatus(status, id, std::move(req.status_callback), req.dispatcher);
  }
}

void RemoteCharacteristic::HandleNotification(const ByteBuffer& value) {
  ZX_DEBUG_ASSERT(thread_checker_.IsCreationThreadCurrent());
  ZX_DEBUG_ASSERT(client_);
  ZX_DEBUG_ASSERT(!shut_down_);

  for (auto& iter : notify_handlers_) {
    auto& handler = iter.second;
    NotifyValue(value, handler.callback.share(), handler.dispatcher);
  }
}

}  // namespace gatt
}  // namespace bt
