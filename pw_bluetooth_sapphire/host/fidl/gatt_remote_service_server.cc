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
using fuchsia::bluetooth::gatt::WriteOptions;

using bt::ByteBuffer;
using bt::MutableBufferView;
using bt::gatt::CharacteristicData;
using bt::gatt::CharacteristicHandle;
using bt::gatt::DescriptorData;
using bt::gatt::DescriptorHandle;

namespace bthost {
namespace {

// We mask away the "extended properties" property. We expose extended
// properties in the same bitfield.
constexpr uint8_t kPropertyMask = 0x7F;

Characteristic CharacteristicToFidl(const CharacteristicData& characteristic,
                                    const std::map<DescriptorHandle, DescriptorData>& descriptors) {
  Characteristic fidl_char;
  fidl_char.id = static_cast<uint64_t>(characteristic.value_handle);
  fidl_char.type = characteristic.type.ToString();
  fidl_char.properties = static_cast<uint16_t>(characteristic.properties & kPropertyMask);
  fidl_char.descriptors.emplace();  // initialize an empty vector

  // TODO(armansito): Add extended properties.

  for (const auto& [id, descr] : descriptors) {
    Descriptor fidl_descr;
    fidl_descr.id = static_cast<uint64_t>(id.value);
    fidl_descr.type = descr.type.ToString();
    fidl_char.descriptors->push_back(std::move(fidl_descr));
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
      for (const auto& [id, chrc] : chrcs) {
        auto& [chr, descs] = chrc;
        fidl_chrcs.push_back(CharacteristicToFidl(chr, descs));
      }
    }

    callback(fidl_helpers::StatusToFidlDeprecated(status, ""), std::move(fidl_chrcs));
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

    callback(fidl_helpers::StatusToFidlDeprecated(status), std::move(vec));
  };

  service_->ReadCharacteristic(CharacteristicHandle(id), std::move(cb));
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

    callback(fidl_helpers::StatusToFidlDeprecated(status), std::move(vec));
  };

  service_->ReadLongCharacteristic(CharacteristicHandle(id), offset, max_bytes, std::move(cb));
}

void GattRemoteServiceServer::WriteCharacteristic(uint64_t id, ::std::vector<uint8_t> value,
                                                  WriteCharacteristicCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status) {
    callback(fidl_helpers::StatusToFidlDeprecated(status, ""));
  };

  service_->WriteCharacteristic(CharacteristicHandle(id), std::move(value), std::move(cb));
}

void GattRemoteServiceServer::WriteLongCharacteristic(uint64_t id, uint16_t offset,
                                                      ::std::vector<uint8_t> value,
                                                      WriteOptions write_options,
                                                      WriteLongCharacteristicCallback callback) {
  auto cb = [callback = std::move(callback)](bt::att::Status status) {
    callback(fidl_helpers::StatusToFidlDeprecated(status, ""));
  };

  service_->WriteLongCharacteristic(CharacteristicHandle(id), offset, std::move(value),
                                    std::move(cb));
}

void GattRemoteServiceServer::WriteCharacteristicWithoutResponse(uint64_t id,
                                                                 ::std::vector<uint8_t> value) {
  service_->WriteCharacteristicWithoutResponse(CharacteristicHandle(id), std::move(value));
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

    callback(fidl_helpers::StatusToFidlDeprecated(status), std::move(vec));
  };

  service_->ReadDescriptor(DescriptorHandle(id), std::move(cb));
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

    callback(fidl_helpers::StatusToFidlDeprecated(status), std::move(vec));
  };

  service_->ReadLongDescriptor(DescriptorHandle(id), offset, max_bytes, std::move(cb));
}

void GattRemoteServiceServer::WriteDescriptor(uint64_t id, ::std::vector<uint8_t> value,
                                              WriteDescriptorCallback callback) {
  service_->WriteDescriptor(DescriptorHandle(id), std::move(value),
                            [callback = std::move(callback)](bt::att::Status status) {
                              callback(fidl_helpers::StatusToFidlDeprecated(status, ""));
                            });
}

void GattRemoteServiceServer::WriteLongDescriptor(uint64_t id, uint16_t offset,
                                                  ::std::vector<uint8_t> value,
                                                  WriteLongDescriptorCallback callback) {
  service_->WriteLongDescriptor(DescriptorHandle(id), offset, std::move(value),
                                [callback = std::move(callback)](bt::att::Status status) {
                                  callback(fidl_helpers::StatusToFidlDeprecated(status, ""));
                                });
}

void GattRemoteServiceServer::NotifyCharacteristic(uint64_t id, bool enable,
                                                   NotifyCharacteristicCallback callback) {
  auto handle = CharacteristicHandle(id);
  if (!enable) {
    auto iter = notify_handlers_.find(handle);
    if (iter == notify_handlers_.end()) {
      callback(fidl_helpers::NewFidlError(ErrorCode::NOT_FOUND, "characteristic not notifying"));
      return;
    }

    if (iter->second == bt::gatt::kInvalidId) {
      callback(fidl_helpers::NewFidlError(ErrorCode::IN_PROGRESS,
                                          "characteristic notification registration pending"));
      return;
    }

    service_->DisableNotifications(handle, iter->second,
                                   [callback = std::move(callback)](bt::att::Status status) {
                                     callback(fidl_helpers::StatusToFidlDeprecated(status, ""));
                                   });
    notify_handlers_.erase(iter);

    return;
  }

  if (notify_handlers_.count(handle) != 0) {
    callback(fidl_helpers::NewFidlError(ErrorCode::ALREADY, "characteristic already notifying"));
    return;
  }

  // Prevent any races and leaks by marking a notification is in progress
  notify_handlers_[handle] = bt::gatt::kInvalidId;

  auto self = weak_ptr_factory_.GetWeakPtr();
  auto value_cb = [self, id](const ByteBuffer& value) {
    if (!self)
      return;

    self->binding()->events().OnCharacteristicValueUpdated(id, value.ToVector());
  };

  auto status_cb = [self, svc = service_, handle, callback = std::move(callback)](
                       bt::att::Status status, HandlerId handler_id) {
    if (!self) {
      if (status) {
        // Disable this handler so it doesn't leak.
        svc->DisableNotifications(handle, handler_id, NopStatusCallback);
      }

      callback(fidl_helpers::NewFidlError(ErrorCode::FAILED, "canceled"));
      return;
    }

    if (status) {
      ZX_DEBUG_ASSERT(handler_id != bt::gatt::kInvalidId);
      ZX_DEBUG_ASSERT(self->notify_handlers_.count(handle) == 1u);
      ZX_DEBUG_ASSERT(self->notify_handlers_[handle] == bt::gatt::kInvalidId);
      self->notify_handlers_[handle] = handler_id;
    } else {
      // Remove our handle holder.
      self->notify_handlers_.erase(handle);
    }

    callback(fidl_helpers::StatusToFidlDeprecated(status, ""));
  };

  service_->EnableNotifications(handle, std::move(value_cb), std::move(status_cb));
}

}  // namespace bthost
