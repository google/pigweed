// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_GATT_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_GATT_SERVER_H_

#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt {
namespace testing {

class FakePeer;

// Emulates a GATT server.
class FakeGattServer final {
 public:
  explicit FakeGattServer(FakePeer* dev);

  // Handle the ATT |pdu| received over link with handle |conn|.
  void HandlePdu(hci::ConnectionHandle conn, const ByteBuffer& pdu);

 private:
  void HandleReadByGrpType(hci::ConnectionHandle conn,
                           const ByteBuffer& params);

  void Send(hci::ConnectionHandle conn, const ByteBuffer& pdu);
  void SendErrorRsp(hci::ConnectionHandle conn, att::OpCode opcode,
                    att::Handle handle, att::ErrorCode ecode);

  // The fake device that owns this server. Must outlive this instance.
  FakePeer* dev_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeGattServer);
};

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_GATT_SERVER_H_
