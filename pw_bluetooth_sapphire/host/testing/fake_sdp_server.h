// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SDP_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SDP_SERVER_H_

#include "fake_dynamic_channel.h"
#include "fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/packet_view.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/server.h"

namespace bt::testing {

// Emulate Sdp Server capability leveraging the production SDP server to
// generate response packets as necessary.
class FakeSdpServer {
 public:
  // Initialize a FakeSdpServer instance and create an associated instance of
  // the production SDP server.
  FakeSdpServer();

  // Register this FakeSdpServer as a service on PSM l2cap::kSDP on |l2cap|.
  // Any channel registered with this service will have its packet handler
  // calllback set to FakeSdpServer::HandleSdu()
  void RegisterWithL2cap(FakeL2cap* l2cap_);

  // Handle an inbound packet |sdu| using the production SDP server instance,
  // and then respond using the |channel| send_packet_callback.
  void HandleSdu(fxl::WeakPtr<FakeDynamicChannel> channel, const ByteBuffer& sdu);

  // Return the production SDP server associated with this FakeSdpServer.
  sdp::Server* server() { return &server_; }

 private:
  // The production SDP server associated with this FakeSdpServer,
  sdp::Server server_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeSdpServer);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SDP_SERVER_H_
