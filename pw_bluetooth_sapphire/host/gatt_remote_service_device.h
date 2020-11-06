// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_

#include <lib/async-loop/cpp/loop.h>
#include <lib/async-loop/default.h>
#include <lib/async/cpp/task.h>
#include <lib/async/dispatcher.h>
#include <lib/fit/thread_checker.h>
#include <zircon/types.h>

#include <ddk/driver.h>
#include <ddktl/device.h>
#include <ddktl/protocol/bt/gattsvc.h>
#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt.h"

namespace bthost {

// This class is responsible for bridging remote GATT services to the DDK so
// GATT services can be implimented as drivers (eg HID over GATT as HIDBUS
// device).
class GattRemoteServiceDevice final
    : public ddk::Device<GattRemoteServiceDevice, ddk::Unbindable>,
      public ddk::BtGattSvcProtocol<GattRemoteServiceDevice, ddk::base_protocol> {
 public:
  GattRemoteServiceDevice(zx_device_t* parent, bt::gatt::PeerId peer_id,
                          fbl::RefPtr<bt::gatt::RemoteService> service);
  ~GattRemoteServiceDevice() = default;

  // Publish a bt-gatt-svc device as a child of |parent|.
  static zx_status_t Publish(zx_device_t* parent, bt::gatt::PeerId peer_id,
                             fbl::RefPtr<bt::gatt::RemoteService> service);

  // ddk::Device operations
  void DdkRelease();
  void DdkUnbind(ddk::UnbindTxn txn);

  // ddk::BtGattSvcProtocol operations:
  void BtGattSvcConnect(bt_gatt_svc_connect_callback connect_cb, void* cookie);
  void BtGattSvcStop();
  void BtGattSvcReadCharacteristic(bt_gatt_id_t id,
                                   bt_gatt_svc_read_characteristic_callback read_cb, void* cookie);
  void BtGattSvcReadLongCharacteristic(bt_gatt_id_t id, uint16_t offset, size_t max_bytes,
                                       bt_gatt_svc_read_characteristic_callback read_cb,
                                       void* cookie);
  void BtGattSvcWriteCharacteristic(bt_gatt_id_t id, const void* buff, size_t len,
                                    bt_gatt_svc_write_characteristic_callback write_cb,
                                    void* cookie);
  void BtGattSvcEnableNotifications(bt_gatt_id_t id, const bt_gatt_notification_value_t* value_cb,
                                    bt_gatt_svc_enable_notifications_callback status_cb,
                                    void* cookie);

 private:
  void StartThread();

  // All device protocol messages are dispatched on this loop to not block the
  // gatt or host thread.
  async::Loop loop_;

  const bt::gatt::PeerId peer_id_;
  const fbl::RefPtr<bt::gatt::RemoteService> service_;

  fit::thread_checker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattRemoteServiceDevice);
};

}  // namespace bthost
#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GATT_REMOTE_SERVICE_DEVICE_H_
