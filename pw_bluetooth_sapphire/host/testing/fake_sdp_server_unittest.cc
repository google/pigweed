// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/testing/fake_sdp_server.h"

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_dynamic_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_signaling_server.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::testing {
namespace {

l2cap::ChannelParameters kChannelParams;
hci_spec::ConnectionHandle kConnectionHandle = 0x01;
l2cap::CommandId kCommandId = 0x02;
l2cap::PSM kPsm = l2cap::kSDP;
l2cap::ChannelId src_id = l2cap::kFirstDynamicChannelId;

auto SdpErrorResponse(uint16_t t_id, sdp::ErrorCode code) {
  return StaticByteBuffer(0x01, UpperBits(t_id), LowerBits(t_id), 0x00, 0x02,
                          UpperBits(uint16_t(code)), LowerBits(uint16_t(code)));
}

std::vector<sdp::ServiceRecord> GetSPPServiceRecord() {
  sdp::ServiceRecord record;
  record.SetServiceClassUUIDs({sdp::profile::kSerialPort});
  record.AddProtocolDescriptor(sdp::ServiceRecord::kPrimaryProtocolList, sdp::protocol::kL2CAP,
                               sdp::DataElement());
  record.AddProtocolDescriptor(sdp::ServiceRecord::kPrimaryProtocolList, sdp::protocol::kRFCOMM,
                               sdp::DataElement(uint8_t{0}));
  record.AddProfile(sdp::profile::kSerialPort, 1, 2);
  record.AddInfo("en", "FAKE", "", "");
  std::vector<sdp::ServiceRecord> records;
  records.emplace_back(std::move(record));
  return records;
}

std::vector<sdp::ServiceRecord> GetA2DPServiceRecord() {
  sdp::ServiceRecord record;
  record.SetServiceClassUUIDs({sdp::profile::kAudioSink});
  record.AddProtocolDescriptor(sdp::ServiceRecord::kPrimaryProtocolList, sdp::protocol::kL2CAP,
                               sdp::DataElement(l2cap::kAVDTP));
  record.AddProtocolDescriptor(sdp::ServiceRecord::kPrimaryProtocolList, sdp::protocol::kAVDTP,
                               sdp::DataElement(uint16_t{0x0103}));  // Version
  record.AddProfile(sdp::profile::kAdvancedAudioDistribution, 1, 3);
  record.SetAttribute(sdp::kA2DP_SupportedFeatures,
                      sdp::DataElement(uint16_t{0x0001}));  // Headphones
  std::vector<sdp::ServiceRecord> records;
  records.emplace_back(std::move(record));
  return records;
}

#define UINT32_AS_BE_BYTES(x) \
  UpperBits(x >> 16), LowerBits(x >> 16), UpperBits(x & 0xFFFF), LowerBits(x & 0xFFFF)

TEST(FakeSdpServerTest, SuccessfulSearch) {
  std::unique_ptr<ByteBuffer> sent_packet;
  auto send_cb = [&sent_packet](auto& buffer) {
    sent_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  FakeDynamicChannel channel(kConnectionHandle, kCommandId, src_id, src_id);
  channel.set_send_packet_callback(send_cb);
  channel.set_opened();
  auto sdp_server = FakeSdpServer();

  // Configure the SDP server to provide a response to the search.
  auto NopConnectCallback = [](auto /*channel*/, const sdp::DataElement&) {};
  sdp::Server::RegistrationHandle spp_handle = sdp_server.server()->RegisterService(
      GetSPPServiceRecord(), kChannelParams, NopConnectCallback);
  EXPECT_TRUE(spp_handle);
  sdp::Server::RegistrationHandle a2dp_handle = sdp_server.server()->RegisterService(
      GetA2DPServiceRecord(), kChannelParams, NopConnectCallback);
  EXPECT_TRUE(a2dp_handle);
  const StaticByteBuffer kL2capSearch(0x02,        // SDP_ServiceSearchRequest
                                      0x10, 0x01,  // Transaction ID (0x1001)
                                      0x00, 0x08,  // Parameter length (8 bytes)
                                      // ServiceSearchPattern
                                      0x35, 0x03,        // Sequence uint8 3 bytes
                                      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                      0xFF, 0xFF,        // MaximumServiceRecordCount: (none)
                                      0x00               // Contunuation State: none
  );
  const StaticByteBuffer kL2capSearchResponse(
      0x03,                             // SDP_ServicesearchResponse
      0x10, 0x01,                       // Transaction ID (0x1001)
      0x00, 0x0D,                       // Parameter length (13 bytes)
      0x00, 0x02,                       // Total service record count: 2
      0x00, 0x02,                       // Current service record count: 2
      UINT32_AS_BE_BYTES(a2dp_handle),  // This list isn't specifically ordered
      UINT32_AS_BE_BYTES(spp_handle),
      0x00  // No continuation state
  );
  sdp_server.HandleSdu(channel.AsWeakPtr(), kL2capSearch);
  EXPECT_TRUE(ContainersEqual(kL2capSearchResponse, *sent_packet));
}

TEST(FakeSdpServerTest, ErrorIfTooSmall) {
  std::unique_ptr<ByteBuffer> sent_packet;
  auto send_cb = [&sent_packet](auto& buffer) {
    sent_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  FakeDynamicChannel channel(kConnectionHandle, kCommandId, src_id, src_id);
  channel.set_send_packet_callback(send_cb);
  channel.set_opened();
  auto sdp_server = FakeSdpServer();

  // Expect an error response if the packet is too small
  const StaticByteBuffer kTooSmall(0x01,        // SDP_ServiceSearchRequest
                                   0x10, 0x01,  // Transaction ID (0x1001)
                                   0x00, 0x09   // Parameter length (9 bytes)
  );
  const auto kRspTooSmall = SdpErrorResponse(0x1001, sdp::ErrorCode::kInvalidSize);
  sdp_server.HandleSdu(channel.AsWeakPtr(), kTooSmall);
  EXPECT_TRUE(ContainersEqual(kRspTooSmall, *sent_packet));
}

TEST(FakeSdpServerTest, RegisterWithL2cap) {
  std::unique_ptr<ByteBuffer> received_packet;
  auto send_cb = [&received_packet](auto conn, auto cid, auto& buffer) {
    received_packet = std::make_unique<DynamicByteBuffer>(buffer);
  };
  auto fake_l2cap = FakeL2cap(send_cb);
  auto sdp_server = std::make_unique<FakeSdpServer>();
  sdp_server->RegisterWithL2cap(&fake_l2cap);
  EXPECT_TRUE(fake_l2cap.ServiceRegisteredForPsm(kPsm));
}

}  // namespace
}  // namespace bt::testing
