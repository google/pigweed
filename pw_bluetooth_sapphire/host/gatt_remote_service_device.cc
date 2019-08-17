// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_remote_service_device.h"

#include <zircon/status.h>

#include <memory>

#include <ddk/binding.h>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"

using namespace bt;

using bt::gatt::CharacteristicHandle;

namespace bthost {

namespace {

void CopyUUIDBytes(bt_gatt_uuid_t* dest, const UUID source) {
  memcpy(dest->bytes, source.value().data(), sizeof(dest->bytes));
}

bt_gatt_err_t AttErrorToDdkError(bt::att::ErrorCode error) {
  // Both of these enums *should* be identical and values.
  // Being explicit so we get compiler warnings if either changes.
  switch (error) {
    case bt::att::ErrorCode::kNoError:
      return BT_GATT_ERR_NO_ERROR;
    case bt::att::ErrorCode::kInvalidHandle:
      return BT_GATT_ERR_INVALID_HANDLE;
    case bt::att::ErrorCode::kReadNotPermitted:
      return BT_GATT_ERR_READ_NOT_PERMITTED;
    case bt::att::ErrorCode::kWriteNotPermitted:
      return BT_GATT_ERR_WRITE_NOT_PERMITTED;
    case bt::att::ErrorCode::kInvalidPDU:
      return BT_GATT_ERR_INVALID_PDU;
    case bt::att::ErrorCode::kInsufficientAuthentication:
      return BT_GATT_ERR_INSUFFICIENT_AUTHENTICATION;
    case bt::att::ErrorCode::kRequestNotSupported:
      return BT_GATT_ERR_REQUEST_NOT_SUPPORTED;
    case bt::att::ErrorCode::kInvalidOffset:
      return BT_GATT_ERR_INVALID_OFFSET;
    case bt::att::ErrorCode::kInsufficientAuthorization:
      return BT_GATT_ERR_INSUFFICIENT_AUTHORIZATION;
    case bt::att::ErrorCode::kPrepareQueueFull:
      return BT_GATT_ERR_PREPARE_QUEUE_FULL;
    case bt::att::ErrorCode::kAttributeNotFound:
      return BT_GATT_ERR_ATTRIBUTE_NOT_FOUND;
    case bt::att::ErrorCode::kAttributeNotLong:
      return BT_GATT_ERR_INVALID_ATTRIBUTE_VALUE_LENGTH;
    case bt::att::ErrorCode::kInsufficientEncryptionKeySize:
      return BT_GATT_ERR_INSUFFICIENT_ENCRYPTION_KEY_SIZE;
    case bt::att::ErrorCode::kInvalidAttributeValueLength:
      return BT_GATT_ERR_INVALID_ATTRIBUTE_VALUE_LENGTH;
    case bt::att::ErrorCode::kUnlikelyError:
      return BT_GATT_ERR_UNLIKELY_ERROR;
    case bt::att::ErrorCode::kInsufficientEncryption:
      return BT_GATT_ERR_INSUFFICIENT_ENCRYPTION;
    case bt::att::ErrorCode::kUnsupportedGroupType:
      return BT_GATT_ERR_UNSUPPORTED_GROUP_TYPE;
    case bt::att::ErrorCode::kInsufficientResources:
      return BT_GATT_ERR_INSUFFICIENT_RESOURCES;
  }
  return BT_GATT_ERR_NO_ERROR;
}

zx_status_t HostErrorToZxError(bt::HostError error) {
  switch (error) {
    case bt::HostError::kNoError:
      return ZX_OK;
    case bt::HostError::kNotFound:
      return ZX_ERR_NOT_FOUND;
    case bt::HostError::kNotReady:
      return ZX_ERR_SHOULD_WAIT;
    case bt::HostError::kTimedOut:
      return ZX_ERR_TIMED_OUT;
    case bt::HostError::kInvalidParameters:
      return ZX_ERR_INVALID_ARGS;
    case bt::HostError::kCanceled:
      return ZX_ERR_CANCELED;
    case bt::HostError::kNotSupported:
      return ZX_ERR_NOT_SUPPORTED;
    case bt::HostError::kLinkDisconnected:
      return ZX_ERR_CONNECTION_ABORTED;
    case bt::HostError::kOutOfMemory:
      return ZX_ERR_NO_MEMORY;
    default:
      return ZX_ERR_INTERNAL;
  }
}

bt_gatt_status_t AttStatusToDdkStatus(const bt::att::Status& status) {
  bt_gatt_status_t ddk_status = {
      .status = HostErrorToZxError(status.error()),
      .att_ecode = BT_GATT_ERR_NO_ERROR,
  };

  if (status.is_protocol_error()) {
    ddk_status.att_ecode = AttErrorToDdkError(status.protocol_error());
  }
  return ddk_status;
}

}  // namespace

GattRemoteServiceDevice::GattRemoteServiceDevice(bt::gatt::PeerId peer_id,
                                                 fbl::RefPtr<bt::gatt::RemoteService> service)
    : loop_(&kAsyncLoopConfigNoAttachToThread),
      dev_(nullptr),
      peer_id_(peer_id),
      service_(service) {
  dev_proto_.version = DEVICE_OPS_VERSION;
  dev_proto_.unbind = &GattRemoteServiceDevice::DdkUnbind;
  dev_proto_.release = &GattRemoteServiceDevice::DdkRelease;
}

bt_gatt_svc_protocol_ops_t GattRemoteServiceDevice::proto_ops_ = {
    .connect = &GattRemoteServiceDevice::OpConnect,
    .stop = &GattRemoteServiceDevice::OpStop,
    .read_characteristic = &GattRemoteServiceDevice::OpReadCharacteristic,
    .read_long_characteristic = &GattRemoteServiceDevice::OpReadLongCharacteristic,
    .write_characteristic = &GattRemoteServiceDevice::OpWriteCharacteristic,
    .enable_notifications = &GattRemoteServiceDevice::OpEnableNotifications,
};

zx_status_t GattRemoteServiceDevice::Bind(zx_device_t* parent) {
  ZX_DEBUG_ASSERT(parent);

  std::lock_guard<std::mutex> lock(mtx_);

  if (dev_) {
    return ZX_ERR_ALREADY_BOUND;
  }

  // The bind program of an attaching device driver can either bind using to the
  // well known short 16 bit UUID of the service if available or the full 128
  // bit UUID (split across 4 32 bit values).
  const UUID& uuid = service_->uuid();
  uint32_t uuid16 = 0;

  if (uuid.CompactSize() == 2) {
    uuid16 = le16toh(*reinterpret_cast<const uint16_t*>(uuid.CompactView().data()));
  }

  uint32_t uuid01, uuid02, uuid03, uuid04 = 0;
  UInt128 uuid_bytes = uuid.value();

  uuid01 = le32toh(*reinterpret_cast<uint32_t*>(&uuid_bytes[0]));
  uuid02 = le32toh(*reinterpret_cast<uint32_t*>(&uuid_bytes[4]));
  uuid03 = le32toh(*reinterpret_cast<uint32_t*>(&uuid_bytes[8]));
  uuid04 = le32toh(*reinterpret_cast<uint32_t*>(&uuid_bytes[12]));

  zx_device_prop_t props[] = {
      {BIND_BT_GATT_SVC_UUID16, 0, uuid16},    {BIND_BT_GATT_SVC_UUID128_1, 0, uuid01},
      {BIND_BT_GATT_SVC_UUID128_2, 0, uuid02}, {BIND_BT_GATT_SVC_UUID128_3, 0, uuid03},
      {BIND_BT_GATT_SVC_UUID128_4, 0, uuid04},
  };

  bt_log(TRACE, "bt-host",
         "bt-gatt-svc binding to UUID16(%#04x), UUID128(1: %08x, 2: %08x,"
         " 3: %08x, 4: %08x), peer: %s",
         uuid16, uuid01, uuid02, uuid03, uuid04, bt_str(peer_id_));

  device_add_args_t args = {
      .version = DEVICE_ADD_ARGS_VERSION,
      .name = "bt-gatt-svc",
      .ctx = this,
      .ops = &dev_proto_,
      .proto_id = ZX_PROTOCOL_BT_GATT_SVC,
      .proto_ops = &proto_ops_,
      .props = props,
      .prop_count = 5,
      .flags = 0,
  };

  zx_status_t status = device_add(parent, &args, &dev_);
  if (status != ZX_OK) {
    dev_ = nullptr;
    bt_log(ERROR, "bt-host", "bt-gatt-svc: failed to publish child gatt device: %s",
           zx_status_get_string(status));
    return status;
  }

  loop_.StartThread("bt-host bt-gatt-svc");

  return status;
}

void GattRemoteServiceDevice::Unbind() {
  bt_log(TRACE, "bt-host", "bt-gatt-svc: unbinding service");
  async::PostTask(loop_.dispatcher(), [this]() { loop_.Shutdown(); });
  loop_.JoinThreads();

  zx_device_t* dev;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    dev = dev_;
    dev_ = nullptr;
  }
  if (dev) {
    device_remove(dev);
  }
}

void GattRemoteServiceDevice::Release() {
  {
    // We expect no associated bt-gatt-svc device to exist in this state.
    std::lock_guard<std::mutex> lock(mtx_);
    ZX_ASSERT(!dev_);  // We expect the device to have been unpublished
  }

  // The DDK no longer owns this context object, so delete it.
  delete this;
}

void GattRemoteServiceDevice::Connect(bt_gatt_svc_connect_callback connect_cb, void* cookie) {
  async::PostTask(loop_.dispatcher(), [this, connect_cb, cookie]() {
    service_->DiscoverCharacteristics(
        [connect_cb, cookie](att::Status cb_status, const auto& chrcs) {
          auto ddk_chars = std::make_unique<bt_gatt_chr[]>(chrcs.size());
          size_t char_idx = 0;
          for (const auto& [id, chrc_pair] : chrcs) {
            const auto& [chr, descriptors] = chrc_pair;
            ddk_chars[char_idx].id = static_cast<bt_gatt_id_t>(id.value);
            CopyUUIDBytes(&ddk_chars[char_idx].type, chr.type);
            ddk_chars[char_idx].properties = chr.properties;

            // TODO(zbowling): remote extended properties are not implemented.
            // ddk_chars[char_idx].extended_properties =
            // chr.extended_properties;

            if (descriptors.size() > 0) {
              ddk_chars[char_idx].descriptor_list = new bt_gatt_descriptor_t[descriptors.size()];
              ddk_chars[char_idx].descriptor_count = descriptors.size();
              size_t desc_idx = 0;
              for (auto& [id, descriptor] : descriptors) {
                ddk_chars[char_idx].descriptor_list[desc_idx].id =
                    static_cast<bt_gatt_id_t>(id.value);
                CopyUUIDBytes(&ddk_chars[char_idx].descriptor_list[desc_idx].type, descriptor.type);
                desc_idx++;
              }
            } else {
              ddk_chars[char_idx].descriptor_count = 0;
              ddk_chars[char_idx].descriptor_list = nullptr;
            }

            char_idx++;
          }

          bt_log(TRACE, "bt-host", "bt-gatt-svc: connected; discovered %zu characteristics",
                 char_idx);
          bt_gatt_status_t status = {.status = ZX_OK};
          connect_cb(cookie, &status, ddk_chars.get(), char_idx);

          // Cleanup.
          for (char_idx = 0; char_idx < chrcs.size(); char_idx++) {
            if (ddk_chars[char_idx].descriptor_list != nullptr) {
              delete[] ddk_chars[char_idx].descriptor_list;
              ddk_chars[char_idx].descriptor_list = nullptr;
            }
          }
        },
        loop_.dispatcher());
  });

  return;
}

void GattRemoteServiceDevice::Stop() {
  // TODO(zbowling): Unregister notifications on the remote service.
  // We may replace this with an explicit unregister for notifications instead.
}

void GattRemoteServiceDevice::ReadCharacteristic(bt_gatt_id_t id,
                                                 bt_gatt_svc_read_characteristic_callback read_cb,
                                                 void* cookie) {
  auto read_callback = [id, cookie, read_cb](att::Status status, const ByteBuffer& buff) {
    bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
    read_cb(cookie, &ddk_status, id, buff.data(), buff.size());
  };
  service_->ReadCharacteristic(CharacteristicHandle(id), std::move(read_callback),
                               loop_.dispatcher());

  return;
}

void GattRemoteServiceDevice::ReadLongCharacteristic(
    bt_gatt_id_t id, uint16_t offset, size_t max_bytes,
    bt_gatt_svc_read_characteristic_callback read_cb, void* cookie) {
  auto read_callback = [id, cookie, read_cb](att::Status status, const ByteBuffer& buff) {
    bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
    read_cb(cookie, &ddk_status, id, buff.data(), buff.size());
  };
  service_->ReadLongCharacteristic(CharacteristicHandle(id), offset, max_bytes,
                                   std::move(read_callback), loop_.dispatcher());

  return;
}

void GattRemoteServiceDevice::WriteCharacteristic(
    bt_gatt_id_t id, const void* buff, size_t len,
    bt_gatt_svc_write_characteristic_callback write_cb, void* cookie) {
  auto* buf = static_cast<const uint8_t*>(buff);
  std::vector<uint8_t> data(buf, buf + len);
  if (write_cb == nullptr) {
    service_->WriteCharacteristicWithoutResponse(CharacteristicHandle(id), std::move(data));
  } else {
    auto status_callback = [cookie, id, write_cb](bt::att::Status status) {
      bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
      write_cb(cookie, &ddk_status, id);
    };

    service_->WriteCharacteristic(CharacteristicHandle(id), std::move(data),
                                  std::move(status_callback), loop_.dispatcher());
  }
  return;
}

void GattRemoteServiceDevice::EnableNotifications(
    bt_gatt_id_t id, const bt_gatt_notification_value_t* value,
    bt_gatt_svc_enable_notifications_callback status_cb, void* cookie) {
  auto value_cb = *value;
  auto notif_callback = [id, value_cb](const ByteBuffer& buff) {
    value_cb.callback(value_cb.ctx, id, buff.data(), buff.size());
  };

  auto status_callback = [cookie, id, status_cb](bt::att::Status status,
                                                 bt::gatt::IdType handler_id) {
    bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
    status_cb(cookie, &ddk_status, id);
  };

  service_->EnableNotifications(CharacteristicHandle(id), notif_callback,
                                std::move(status_callback), loop_.dispatcher());

  return;
}

}  // namespace bthost
