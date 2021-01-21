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

// TODO(fxbug.dev/63450): The 64 bit `bt_gatt_id_t` can overflow the 16 bits of a bt:att::Handle
// that underlies CharacteristicHandle when directly casted. Fix this.
bt::gatt::CharacteristicHandle CharacteristicHandleFromBanjo(bt_gatt_id_t banjo_id) {
  if (banjo_id & 0xFFFF) {
    bt_log(ERROR, "bt-host",
           "bt-gatt-svc: Casting a 64-bit FIDL GATT ID with `bits[16, 63] != 0` to 16-bit "
           "Characteristic Handle");
  }
  return bt::gatt::CharacteristicHandle(static_cast<bt::att::Handle>(banjo_id));
}
}  // namespace

GattRemoteServiceDevice::GattRemoteServiceDevice(zx_device_t* parent, bt::gatt::PeerId peer_id,
                                                 fbl::RefPtr<bt::gatt::RemoteService> service)
    : ddk::Device<GattRemoteServiceDevice, ddk::Unbindable>(parent),
      loop_(&kAsyncLoopConfigNoAttachToCurrentThread),
      peer_id_(peer_id),
      service_(service) {}

// static
zx_status_t GattRemoteServiceDevice::Publish(zx_device_t* parent, bt::gatt::PeerId peer_id,
                                             fbl::RefPtr<bt::gatt::RemoteService> service) {
  ZX_DEBUG_ASSERT(parent);
  ZX_DEBUG_ASSERT(service);

  // The bind program of an attaching device driver can either bind using to the
  // well known short 16 bit UUID of the service if available or the full 128
  // bit UUID (split across 4 32 bit values).
  const UUID& uuid = service->uuid();
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

  bt_log(DEBUG, "bt-host",
         "bt-gatt-svc: binding to UUID16(%#04x), UUID128(1: %08x, 2: %08x, 3: %08x, 4: %08x), "
         "peer: %s",
         uuid16, uuid01, uuid02, uuid03, uuid04, bt_str(peer_id));

  zx_device_prop_t props[] = {
      {BIND_BT_GATT_SVC_UUID16, 0, uuid16},    {BIND_BT_GATT_SVC_UUID128_1, 0, uuid01},
      {BIND_BT_GATT_SVC_UUID128_2, 0, uuid02}, {BIND_BT_GATT_SVC_UUID128_3, 0, uuid03},
      {BIND_BT_GATT_SVC_UUID128_4, 0, uuid04},
  };
  auto args = ddk::DeviceAddArgs("bt-gatt-svc").set_props(props);
  auto dev = std::make_unique<GattRemoteServiceDevice>(parent, peer_id, service);

  TRACE_DURATION_BEGIN("bluetooth", "GattRemoteServiceDevice::Bind DdkAdd");
  zx_status_t status = dev->DdkAdd(args);
  TRACE_DURATION_END("bluetooth", "GattRemoteServiceDevice::Bind DdkAdd");

  if (status != ZX_OK) {
    bt_log(ERROR, "bt-gatt-svc", "failed to publish device: %s", zx_status_get_string(status));
    return status;
  }

  // Kick off the dedicated dispatcher where our children will process GATT events from the stack.
  dev->StartThread();

  // The DDK owns the memory now.
  __UNUSED auto* ptr = dev.release();

  return status;
}

void GattRemoteServiceDevice::DdkRelease() {
  bt_log(TRACE, "bt-host", "bt-gatt-svc: release");
  // The DDK no longer owns this context object, so delete it.
  delete this;
}

void GattRemoteServiceDevice::DdkUnbind(ddk::UnbindTxn txn) {
  bt_log(DEBUG, "bt-host", "bt-gatt-svc: unbind");

  async::PostTask(loop_.dispatcher(), [this]() { loop_.Shutdown(); });
  loop_.JoinThreads();

  txn.Reply();
}

void GattRemoteServiceDevice::StartThread() {
  // NOTE: This method is expected to run on the bt-host thread.
  ZX_DEBUG_ASSERT(thread_checker_.is_thread_valid());

  TRACE_DURATION_BEGIN("bluetooth", "GattRemoteServiceDevice::StartThread");

  // Start processing GATT tasks.
  loop_.StartThread("bt-host bt-gatt-svc");

  // There are a few possible race conditions between the "GATT service removed handler" and the
  // GattRemoteServiceDevice destruction. The following race conditions *could* lead to a
  // use-after-free:
  //
  //   * During initialization, it is possible for Publish() to fail to add the bt-gatt-svc device
  //     if the service handler runs while bt-host is unbinding. We register the removal handler
  //     only after the device has been successfully created, to ensure that it doesn't get run
  //     after Publish() returns early and the context object gets deleted.
  //
  //   * Under regular conditions that lead to unbinding a bt-gatt-svc.
  //
  // A bt-gatt-svc device can be unbound during the following events:
  //
  //   1. The parent HostDevice finishes its unbind, which triggers its children to unbind.
  //   2. The removed handler gets called due to a Bluetooth stack event, such as a disconnection
  //      or a change in the peer's ATT database.
  //   3. The removed handler gets called while HostDevice is shutting down, as part of the
  //      Bluetooth stack's clean up procedure. The GATT layer always notifies this callback during
  //      its destruction.
  //
  // The following invariants *should* prevent this callback to run with a dangling pointer in the
  // unbind conditions:
  //
  //   a. HostDevice always drains and shuts down its event loop completely before it finishes
  //      unbinding.
  //   b. GattRemoteServiceDevice always drains and shuts down its own |loop_| before its unbind
  //      finishes, and thus this callback must finish running before release gets called.
  //   c. #1 is not an issue because DdkUnbind should only get called after HostDevice finishes
  //      unbinding.
  //   d. #3 is not an issue because it is guaranteed to occur before HostDevice finishes unbinding,
  //      due to invariant a.
  //   e. #2 is not an issue because it involves regular operation outside of shutdown.
  service_->AddRemovedHandler([this] { DdkAsyncRemove(); }, loop_.dispatcher());

  TRACE_DURATION_END("bluetooth", "GattRemoteServiceDevice::StartThread");
}

void GattRemoteServiceDevice::BtGattSvcConnect(bt_gatt_svc_connect_callback connect_cb,
                                               void* cookie) {
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

          bt_log(DEBUG, "bt-host", "bt-gatt-svc: connected; discovered %zu characteristics",
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

void GattRemoteServiceDevice::BtGattSvcStop() {
  // TODO(zbowling): Unregister notifications on the remote service.
  // We may replace this with an explicit unregister for notifications instead.
}

void GattRemoteServiceDevice::BtGattSvcReadCharacteristic(
    bt_gatt_id_t id, bt_gatt_svc_read_characteristic_callback read_cb, void* cookie) {
  auto read_callback = [id, cookie, read_cb](att::Status status, const ByteBuffer& buff) {
    bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
    read_cb(cookie, &ddk_status, id, buff.data(), buff.size());
  };

  // TODO(fxbug.dev/63450): Fix CharacteristicHandleFromBanjo to not cast a uint64_t into uint16_t.
  service_->ReadCharacteristic(CharacteristicHandleFromBanjo(id), std::move(read_callback),
                               loop_.dispatcher());

  return;
}

void GattRemoteServiceDevice::BtGattSvcReadLongCharacteristic(
    bt_gatt_id_t id, uint16_t offset, size_t max_bytes,
    bt_gatt_svc_read_characteristic_callback read_cb, void* cookie) {
  auto read_callback = [id, cookie, read_cb](att::Status status, const ByteBuffer& buff) {
    bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
    read_cb(cookie, &ddk_status, id, buff.data(), buff.size());
  };

  // TODO(fxbug.dev/63450): Fix CharacteristicHandleFromBanjo to not cast a uint64_t into uint16_t.
  service_->ReadLongCharacteristic(CharacteristicHandleFromBanjo(id), offset, max_bytes,
                                   std::move(read_callback), loop_.dispatcher());

  return;
}

void GattRemoteServiceDevice::BtGattSvcWriteCharacteristic(
    bt_gatt_id_t id, const uint8_t* buff, size_t len,
    bt_gatt_svc_write_characteristic_callback write_cb, void* cookie) {
  auto* buf = static_cast<const uint8_t*>(buff);
  std::vector<uint8_t> data(buf, buf + len);
  // TODO(fxbug.dev/63450): Fix CharacteristicHandleFromBanjo to not cast a uint64_t into uint16_t.
  auto handle = CharacteristicHandleFromBanjo(id);

  if (write_cb == nullptr) {
    service_->WriteCharacteristicWithoutResponse(handle, std::move(data));
  } else {
    auto status_callback = [cookie, id, write_cb](bt::att::Status status) {
      bt_gatt_status_t ddk_status = AttStatusToDdkStatus(status);
      write_cb(cookie, &ddk_status, id);
    };

    service_->WriteCharacteristic(handle, std::move(data), std::move(status_callback),
                                  loop_.dispatcher());
  }
  return;
}

void GattRemoteServiceDevice::BtGattSvcEnableNotifications(
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

  // TODO(fxbug.dev/63450): Fix CharacteristicHandleFromBanjo to not cast a uint64_t into uint16_t.
  service_->EnableNotifications(CharacteristicHandleFromBanjo(id), notif_callback,
                                std::move(status_callback), loop_.dispatcher());

  return;
}

}  // namespace bthost
