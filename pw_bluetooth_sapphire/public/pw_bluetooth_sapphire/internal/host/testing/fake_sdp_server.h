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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SDP_SERVER_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SDP_SERVER_H_

#include "fake_l2cap.h"
#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"
#include "pw_bluetooth_sapphire/internal/host/common/packet_view.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/protocol.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/fake_l2cap.h"
#include "pw_bluetooth_sapphire/internal/host/l2cap/l2cap_defs.h"
#include "pw_bluetooth_sapphire/internal/host/sdp/server.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_dynamic_channel.h"

namespace bt::testing {

// Emulate Sdp Server capability leveraging the production SDP server to
// generate response packets as necessary.
class FakeSdpServer {
 public:
  // Initialize a FakeSdpServer instance and create an associated instance of
  // the production SDP server.
  explicit FakeSdpServer(pw::async::Dispatcher& pw_dispatcher);

  // Register this FakeSdpServer as a service on PSM l2cap::kSDP on |l2cap|.
  // Any channel registered with this service will have its packet handler
  // calllback set to FakeSdpServer::HandleSdu()
  void RegisterWithL2cap(FakeL2cap* l2cap_);

  // Handle an inbound packet |sdu| using the production SDP server instance,
  // and then respond using the |channel| send_packet_callback.
  void HandleSdu(FakeDynamicChannel::WeakPtr channel, const ByteBuffer& sdu);

  // Return the production SDP server associated with this FakeSdpServer.
  sdp::Server* server() { return &server_; }

 private:
  std::unique_ptr<l2cap::testing::FakeL2cap> l2cap_;

  // The production SDP server associated with this FakeSdpServer,
  sdp::Server server_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FakeSdpServer);
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_FAKE_SDP_SERVER_H_
