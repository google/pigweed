// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SIGNALING_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SIGNALING_SERVER_H_

#include "fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"

namespace bt {
namespace testing {

// This class unpacks data units received from ACL-U and LE-U logical links
// into L2CAP SDUs and then routes them to indvidually-registered handler
// functions. Each FakePeer should have its own FakeL2cap instance and its
// own set of ACL-U and LE-U logical links.

// This class unpacks signaling packets (generally received over a FakeL2cap
// link). Each FakePeer should own its own FakeSignalingServer and should
// instantiate it with its own SendFrameCallback method that handles additional
// PDU header processing and sends the packet using the FakePeer.
class FakeSignalingServer final {
 public:
  // Entities that instantiate FakeSignalingServer must provide a
  // SendFrameCallback function to handle adding necessary protocol data unit
  // header information to the packet and actually sending the packet using
  // the associated device.
  using SendFrameCallback = fit::function<void(hci::ConnectionHandle conn, const ByteBuffer& sdu)>;

  // Calls |send_frame_callback| with response signaling packets associated
  // with requests received by means of the Handledu method.
  // Has no default value.
  explicit FakeSignalingServer(SendFrameCallback send_frame_callback);

  // Registers this FakeSignalingServer's HandleSdu function with |l2cap_| on
  // kSignalingChannelId such that all packets processed by |l2cap_| with the
  // ChannelId kSignalingChanneld will be processed by this server.
  void RegisterWithL2cap(FakeL2cap* l2cap_);

  // Handles the service data unit |sdu| received over link with handle |conn|
  // by confirming that the received packet is valid and then calling
  // ProcessSignalingPacket.
  void HandleSdu(hci::ConnectionHandle conn, const ByteBuffer& sdu);

  // Parses the InformationRequest signaling packet |info_req| and then
  // constructs a response packet using the associated ID| id|, and then sends
  // the packet using the FakeSignalingServer's SendFrameCallback using the
  // handle |conn| and constructed response packet.
  void ProcessInformationRequest(hci::ConnectionHandle conn, l2cap::CommandId id,
                                 const ByteBuffer& info_req);

  // Reject a command packet if it is not understood. Sends a command reject
  // packet using the handle |conn| and the CommandId |id| using the
  // FakeSignalingServer's send_frame_callback.
  void SendCommandRejectNotUnderstood(hci::ConnectionHandle conn, l2cap::CommandId id);

 private:
  // Function to send signaling packets after the server constructs them.
  SendFrameCallback send_frame_callback_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeSignalingServer);
};

}  // namespace testing
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SIGNALING_SERVER_H_
