// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_

#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/bt/gattsvc.h>
#include <fbl/macros.h>
#include <lib/async-loop/cpp/loop.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <zircon/types.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

namespace bthost {

// This class is responsible for bridging remote GATT services to the DDK so
// GATT services can be implimented as drivers (eg HID over GATT as HIDBUS
// device)
class GattRemoteServiceDevice final {
 public:
  GattRemoteServiceDevice(bt::gatt::PeerId peer_id, fbl::RefPtr<bt::gatt::RemoteService> service);
  ~GattRemoteServiceDevice() = default;

  // Publish a bt-gatt-svc device as a child of |parent|.
  zx_status_t Bind(zx_device_t* parent);

  // Unpublishes any associated bt-gatt-svc and stops processing GATT callbacks. This will be called
  // by the DDK as part of regular device lifecycle but it can also be called directly to remove a
  // device.
  void Unbind();

 private:
  static bt_gatt_svc_protocol_ops_t proto_ops_;

  // Protocol trampolines.
  static void DdkUnbind(void* ctx) { static_cast<GattRemoteServiceDevice*>(ctx)->Unbind(); }

  static void DdkRelease(void* ctx) { static_cast<GattRemoteServiceDevice*>(ctx)->Release(); }

  static void OpConnect(void* ctx, bt_gatt_svc_connect_callback connect_cb, void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->Connect(connect_cb, cookie);
  }

  static void OpStop(void* ctx) { static_cast<GattRemoteServiceDevice*>(ctx)->Stop(); }

  static void OpReadCharacteristic(void* ctx, bt_gatt_id_t id,
                                   bt_gatt_svc_read_characteristic_callback read_cb, void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->ReadCharacteristic(id, read_cb, cookie);
  }

  static void OpReadLongCharacteristic(void* ctx, bt_gatt_id_t id, uint16_t offset,
                                       size_t max_bytes,
                                       bt_gatt_svc_read_characteristic_callback read_cb,
                                       void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->ReadLongCharacteristic(id, offset, max_bytes,
                                                                       read_cb, cookie);
  }

  static void OpWriteCharacteristic(void* ctx, bt_gatt_id_t id, const void* buf, size_t len,
                                    bt_gatt_svc_write_characteristic_callback status_cb,
                                    void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->WriteCharacteristic(id, buf, len, status_cb,
                                                                    cookie);
  }

  static void OpEnableNotifications(void* ctx, bt_gatt_id_t id,
                                    const bt_gatt_notification_value_t* value_cb,
                                    bt_gatt_svc_enable_notifications_callback status_cb,
                                    void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->EnableNotifications(id, value_cb, status_cb,
                                                                    cookie);
  }

  // DDK device ops.
  void Release();

  // bt-gatt-svc ops.
  void Connect(bt_gatt_svc_connect_callback connect_cb, void* cookie);
  void Stop();
  void ReadCharacteristic(bt_gatt_id_t id, bt_gatt_svc_read_characteristic_callback read_cb,
                          void* cookie);

  void ReadLongCharacteristic(bt_gatt_id_t id, uint16_t offset, size_t max_bytes,
                              bt_gatt_svc_read_characteristic_callback read_cb, void* cookie);
  void WriteCharacteristic(bt_gatt_id_t id, const void* buff, size_t len,
                           bt_gatt_svc_write_characteristic_callback write_cb, void* cookie);
  void EnableNotifications(bt_gatt_id_t id, const bt_gatt_notification_value_t* value_cb,
                           bt_gatt_svc_enable_notifications_callback status_cb, void* cookie);

  // All device protocol messages are dispatched on this loop to not block the
  // gatt or host thread.
  async::Loop loop_;

  std::mutex mtx_;

  // The bt-gatt-svc device that is managed by this object. This member is null if no device has
  // been published.
  zx_device_t* dev_ __TA_GUARDED(mtx_);

  const bt::gatt::PeerId peer_id_;
  const fbl::RefPtr<bt::gatt::RemoteService> service_;

  // The base DDK device ops.
  zx_protocol_device_t dev_proto_ = {};

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattRemoteServiceDevice);
};

}  // namespace bthost
#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_
