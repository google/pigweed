// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once
#include <map>

#include "fake_l2cap.h"
#include "pw_bluetooth_sapphire/internal/host/att/att.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"

namespace bt::testing {

class FakePeer;

// Emulates a GATT server.
class FakeGattServer final {
 public:
  explicit FakeGattServer(FakePeer* dev);

  // Handle the ATT |pdu| received over link with handle |conn|.
  void HandlePdu(hci_spec::ConnectionHandle conn, const ByteBuffer& pdu);

  // Register with FakleL2cap |l2cap_| associated with the device that owns
  // the server such that this FakeGattServer instance receives all packets
  // sent on kATTChannelId.
  void RegisterWithL2cap(FakeL2cap* l2cap_);

 private:
  struct Service {
    att::Handle start_handle;
    att::Handle end_handle;
    UUID type;
  };

  void HandleReadByGrpType(hci_spec::ConnectionHandle conn,
                           const ByteBuffer& bytes);
  void HandleFindByTypeValue(hci_spec::ConnectionHandle conn,
                             const ByteBuffer& bytes);

  void Send(hci_spec::ConnectionHandle conn, const ByteBuffer& pdu);
  void SendErrorRsp(hci_spec::ConnectionHandle conn,
                    att::OpCode opcode,
                    att::Handle handle,
                    att::ErrorCode ecode);

  // Map of service start handles to services.
  std::map<att::Handle, Service> services_;

  // The fake device that owns this server. Must outlive this instance.
  FakePeer* dev_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeGattServer);
};

}  // namespace bt::testing
