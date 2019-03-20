// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_

#include <zircon/types.h>

#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/bt/gattsvc.h>

#include <lib/async-loop/cpp/loop.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

#include "lib/fxl/macros.h"

namespace bthost {

// This class is responsible for bridging remote GATT services to the DDK so
// GATT services can be implimented as drivers (eg HID over GATT as HIDBUS
// device)
class GattRemoteServiceDevice final {
 public:
  GattRemoteServiceDevice(zx_device_t* parent_device,
                          bt::gatt::DeviceId peer_id,
                          fbl::RefPtr<bt::gatt::RemoteService> service);

  ~GattRemoteServiceDevice();

  zx_status_t Bind();

 private:
  static bt_gatt_svc_protocol_ops_t proto_ops_;

  // Protocol trampolines.
  static void DdkUnbind(void* ctx) {
    static_cast<GattRemoteServiceDevice*>(ctx)->Unbind();
  }

  static void DdkRelease(void* ctx) {
    static_cast<GattRemoteServiceDevice*>(ctx)->Release();
  }

  static void OpConnect(void* ctx, bt_gatt_svc_connect_callback connect_cb,
                        void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->Connect(connect_cb, cookie);
  }

  static void OpStop(void* ctx) {
    static_cast<GattRemoteServiceDevice*>(ctx)->Stop();
  }

  static void OpReadCharacteristic(
      void* ctx, bt_gatt_id_t id,
      bt_gatt_svc_read_characteristic_callback read_cb, void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->ReadCharacteristic(id, read_cb,
                                                                   cookie);
  }

  static void OpReadLongCharacteristic(
      void* ctx, bt_gatt_id_t id, uint16_t offset, size_t max_bytes,
      bt_gatt_svc_read_characteristic_callback read_cb, void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->ReadLongCharacteristic(
        id, offset, max_bytes, read_cb, cookie);
  }

  static void OpWriteCharacteristic(
      void* ctx, bt_gatt_id_t id, const void* buf, size_t len,
      bt_gatt_svc_write_characteristic_callback status_cb, void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->WriteCharacteristic(
        id, buf, len, status_cb, cookie);
  }

  static void OpEnableNotifications(
      void* ctx, bt_gatt_id_t id, const bt_gatt_notification_value_t* value_cb,
      bt_gatt_svc_enable_notifications_callback status_cb, void* cookie) {
    static_cast<GattRemoteServiceDevice*>(ctx)->EnableNotifications(
        id, value_cb, status_cb, cookie);
  }

  // DDK device ops.
  void Unbind();
  void Release();

  // bt-gatt-svc ops.
  void Connect(bt_gatt_svc_connect_callback connect_cb, void* cookie);
  void Stop();
  void ReadCharacteristic(bt_gatt_id_t id,
                          bt_gatt_svc_read_characteristic_callback read_cb,
                          void* cookie);

  void ReadLongCharacteristic(bt_gatt_id_t id, uint16_t offset,
                              size_t max_bytes,
                              bt_gatt_svc_read_characteristic_callback read_cb,
                              void* cookie);
  void WriteCharacteristic(bt_gatt_id_t id, const void* buff, size_t len,
                           bt_gatt_svc_write_characteristic_callback write_cb,
                           void* cookie);
  void EnableNotifications(bt_gatt_id_t id,
                           const bt_gatt_notification_value_t* value_cb,
                           bt_gatt_svc_enable_notifications_callback status_cb,
                           void* cookie);

  // All device protocol messages are dispatched on this loop to not block the
  // gatt or host thread.
  async::Loop loop_;

  zx_device_t* parent_device_;  // The BT Host device
  zx_device_t* dev_;            // The child we are creating.

  const bt::gatt::DeviceId peer_id_;
  fbl::RefPtr<bt::gatt::RemoteService> service_;

  // The base DDK device ops.
  zx_protocol_device_t dev_proto_ = {};

  FXL_DISALLOW_COPY_AND_ASSIGN(GattRemoteServiceDevice);
};

}  // namespace bthost
#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_
