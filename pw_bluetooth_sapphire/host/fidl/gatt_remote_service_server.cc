// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_remote_service_server.h"

#include "helpers.h"

using fuchsia::bluetooth::ErrorCode;
using fuchsia::bluetooth::Status;
using fuchsia::bluetooth::gatt::Characteristic;
using fuchsia::bluetooth::gatt::CharacteristicPtr;
using fuchsia::bluetooth::gatt::Descriptor;

using bt::ByteBuffer;
using bt::MutableBufferView;
using bt::gatt::IdType;
using bt::gatt::RemoteCharacteristic;

namespace bthost {
namespace {

// We mask away the "extended properties" property. We expose extended
// properties in the same bitfield.
constexpr uint8_t kPropertyMask = 0x7F;

Characteristic CharacteristicToFidl(const RemoteCharacteristic& chrc) {
  Characteristic fidl_char;
  fidl_char.id = chrc.id();
  fidl_char.type = chrc.info().type.ToString();
  fidl_char.properties = static_cast<uint16_t>(chrc.info().properties & kPropertyMask);

  // TODO(armansito): Add extended properties.

  for (const auto& descr : chrc.descriptors()) {
    Descriptor fidl_descr;
    fidl_descr.id = descr.id();
    fidl_descr.type = descr.info().type.ToString();
    fidl_char.descriptors.push_back(std::move(fidl_descr));
  }

  return fidl_char;
}

void NopStatusCallback(bt::att::Status) {}

}  // namespace

GattRemoteServiceServer::GattRemoteServiceServer(
    fbl::RefPtr<bt::gatt::RemoteService> service, fbl::RefPtr<bt::gatt::GATT> gatt,
    fidl::InterfaceRequest<fuchsia::bluetooth::gatt::RemoteService> request)
    : GattServerBase(gatt, this, std::move(request)),
      service_(std::move(service)),
      weak_ptr_factory_(this) {
  ZX_DEBUG_ASSERT(service_);
}

GattRemoteServiceServer::~GattRemoteServiceServer() {
  for (const auto& iter : notify_handlers_) {
    if (iter.second != bt::gatt::kInvalidId) {
      service_->DisableNotifications(iter.first, iter.second, NopStatusCallback);
    }
  }
}

void GattRemoteServiceServer::DiscoverCharacteristics(DiscoverCharacteristicsCallback callback) {
  auto res_cb = [callback = std::move(callback)](bt::att::Status status, const auto& chrcs) {
    std::vector<Characteristic> fidl_chrcs;
    if (status) {
      for (const auto& chrc : chrcs) {
        fidl_chrcs.push_back(CharacteristicToFidl(chrc));
      }
    }

    callback(fidl_helpers::StatusToFidl(status, ""), std::move(fidl_chrcs));
  };

  service_->DiscoverCharacteristics(std::move(res_cb));
}

void GattRemoteServiceServer::ReadCharacteristic(uint64_t id, ReadCharacteristicCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status, const bt::ByteBuffer& value) {
    // We always reply with a non-null value.
    std::vector<uint8_t> vec;

    if (status && value.size()) {
      vec.resize(value.size());

      MutableBufferView vec_view(vec.data(), vec.size());
      value.Copy(&vec_view);
    }

    callback(fidl_helpers::StatusToFidl(status), fidl::VectorPtr<uint8_t>(std::move(vec)));
  };

  service_->ReadCharacteristic(id, std::move(cb));
}

void GattRemoteServiceServer::ReadLongCharacteristic(uint64_t id, uint16_t offset,
                                                     uint16_t max_bytes,
                                                     ReadLongCharacteristicCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status, const bt::ByteBuffer& value) {
    // We always reply with a non-null value.
    std::vector<uint8_t> vec;

    if (status && value.size()) {
      vec.resize(value.size());

      MutableBufferView vec_view(vec.data(), vec.size());
      value.Copy(&vec_view);
    }

    callback(fidl_helpers::StatusToFidl(status), fidl::VectorPtr<uint8_t>(std::move(vec)));
  };

  service_->ReadLongCharacteristic(id, offset, max_bytes, std::move(cb));
}

void GattRemoteServiceServer::WriteCharacteristic(uint64_t id, ::std::vector<uint8_t> value,
                                                  WriteCharacteristicCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status) {
    callback(fidl_helpers::StatusToFidl(status, ""));
  };

  service_->WriteCharacteristic(id, std::move(value), std::move(cb));
}

void GattRemoteServiceServer::WriteLongCharacteristic(uint64_t id, uint16_t offset,
                                                      ::std::vector<uint8_t> value,
                                                      WriteLongCharacteristicCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status) {
    callback(fidl_helpers::StatusToFidl(status, ""));
  };

  service_->WriteLongCharacteristic(id, offset, std::move(value), std::move(cb));
}

void GattRemoteServiceServer::WriteCharacteristicWithoutResponse(uint64_t id,
                                                                 ::std::vector<uint8_t> value) {
  service_->WriteCharacteristicWithoutResponse(id, std::move(value));
}

void GattRemoteServiceServer::ReadDescriptor(uint64_t id, ReadDescriptorCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status, const bt::ByteBuffer& value) {
    // We always reply with a non-null value.
    std::vector<uint8_t> vec;

    if (status && value.size()) {
      vec.resize(value.size());

      MutableBufferView vec_view(vec.data(), vec.size());
      value.Copy(&vec_view);
    }

    callback(fidl_helpers::StatusToFidl(status), fidl::VectorPtr<uint8_t>(std::move(vec)));
  };

  service_->ReadDescriptor(id, std::move(cb));
}

void GattRemoteServiceServer::ReadLongDescriptor(uint64_t id, uint16_t offset, uint16_t max_bytes,
                                                 ReadLongDescriptorCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status, const bt::ByteBuffer& value) {
    // We always reply with a non-null value.
    std::vector<uint8_t> vec;

    if (status && value.size()) {
      vec.resize(value.size());

      MutableBufferView vec_view(vec.data(), vec.size());
      value.Copy(&vec_view);
    }

    callback(fidl_helpers::StatusToFidl(status), fidl::VectorPtr<uint8_t>(std::move(vec)));
  };

  service_->ReadLongDescriptor(id, offset, max_bytes, std::move(cb));
}

void GattRemoteServiceServer::WriteDescriptor(uint64_t id, ::std::vector<uint8_t> value,
                                              WriteDescriptorCallback callback) {
  service_->WriteDescriptor(id, std::move(value),
                            [callback = std::move(callback)](bt::att::Status status) {
                              callback(fidl_helpers::StatusToFidl(status, ""));
                            });
}

void GattRemoteServiceServer::WriteLongDescriptor(uint64_t id, uint16_t offset,
                                                  ::std::vector<uint8_t> value,
                                                  WriteLongDescriptorCallback callback) {
  service_->WriteLongDescriptor(id, offset, std::move(value),
                                [callback = std::move(callback)](bt::att::Status status) {
                                  callback(fidl_helpers::StatusToFidl(status, ""));
                                });
}

void GattRemoteServiceServer::NotifyCharacteristic(uint64_t id, bool enable,
                                                   NotifyCharacteristicCallback callback) {
  if (!enable) {
    auto iter = notify_handlers_.find(id);
    if (iter == notify_handlers_.end()) {
      callback(fidl_helpers::NewFidlError(ErrorCode::NOT_FOUND, "characteristic not notifying"));
      return;
    }

    if (iter->second == bt::gatt::kInvalidId) {
      callback(fidl_helpers::NewFidlError(ErrorCode::IN_PROGRESS,
                                          "characteristic notification registration pending"));
      return;
    }

    service_->DisableNotifications(id, iter->second,
                                   [callback = std::move(callback)](bt::att::Status status) {
                                     callback(fidl_helpers::StatusToFidl(status, ""));
                                   });
    notify_handlers_.erase(iter);

    return;
  }

  if (notify_handlers_.count(id) != 0) {
    callback(fidl_helpers::NewFidlError(ErrorCode::ALREADY, "characteristic already notifying"));
    return;
  }

  // Prevent any races and leaks by marking a notification is in progress
  notify_handlers_[id] = bt::gatt::kInvalidId;

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto value_cb = [self, id](const ByteBuffer& value) {
    if (!self)
      return;

    std::vector<uint8_t> vec(value.size());
    MutableBufferView vec_view(vec.data(), vec.size());
    value.Copy(&vec_view);
    self->binding()->events().OnCharacteristicValueUpdated(
        id, fidl::VectorPtr<uint8_t>(std::move(vec)));
  };

  auto status_cb = [self, svc = service_, id, callback = std::move(callback)](
                       bt::att::Status status, IdType handler_id) {
    if (!self) {
      if (status) {
        // Disable this handler so it doesn't leak.
        svc->DisableNotifications(id, handler_id, NopStatusCallback);
      }

      callback(fidl_helpers::NewFidlError(ErrorCode::FAILED, "canceled"));
      return;
    }

    if (status) {
      ZX_DEBUG_ASSERT(handler_id != bt::gatt::kInvalidId);
      ZX_DEBUG_ASSERT(self->notify_handlers_.count(id) == 1u);
      ZX_DEBUG_ASSERT(self->notify_handlers_[id] == bt::gatt::kInvalidId);
      self->notify_handlers_[id] = handler_id;
    } else {
      // Remove our handle holder.
      self->notify_handlers_.erase(id);
    }

    callback(fidl_helpers::StatusToFidl(status, ""));
  };

  service_->EnableNotifications(id, std::move(value_cb), std::move(status_cb));
}

}  // namespace bthost
