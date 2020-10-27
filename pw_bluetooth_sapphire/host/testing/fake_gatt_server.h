// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_GATT_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_GATT_SERVER_H_

#include "fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"

namespace bt::testing {

class FakePeer;

// Emulates a GATT server.
class FakeGattServer final {
 public:
  explicit FakeGattServer(FakePeer* dev);

  // Handle the ATT |pdu| received over link with handle |conn|.
  void HandlePdu(hci::ConnectionHandle conn, const ByteBuffer& pdu);

  // Register with FakleL2cap |l2cap_| associated with the device that owns
  // the server such that this FakeGattServer instance receives all packets
  // sent on kATTChannelId.
  void RegisterWithL2cap(FakeL2cap* l2cap_);

 private:
  void HandleReadByGrpType(hci::ConnectionHandle conn, const ByteBuffer& params);

  void Send(hci::ConnectionHandle conn, const ByteBuffer& pdu);
  void SendErrorRsp(hci::ConnectionHandle conn, att::OpCode opcode, att::Handle handle,
                    att::ErrorCode ecode);

  // The fake device that owns this server. Must outlive this instance.
  FakePeer* dev_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeGattServer);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_GATT_SERVER_H_
