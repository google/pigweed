// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sdp/server.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/status.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"

namespace bt {
namespace sdp {
namespace {

using bt::testing::FakeController;

using TestingBase = bt::testing::FakeControllerTest<FakeController>;

constexpr hci::ConnectionHandle kTestHandle1 = 1;
constexpr hci::ConnectionHandle kTestHandle2 = 2;

void NopConnectCallback(l2cap::ChannelSocket, hci::ConnectionHandle, const DataElement&) {}

constexpr l2cap::ChannelParameters kChannelParams;

class SDP_ServerTest : public TestingBase {
 public:
  SDP_ServerTest() = default;
  ~SDP_ServerTest() = default;

 protected:
  void SetUp() override {
    l2cap_ = data::testing::FakeDomain::Create();
    l2cap_->set_channel_callback([this](auto fake_chan) { channel_ = std::move(fake_chan); });
    l2cap_->Initialize();
    l2cap_->AddACLConnection(kTestHandle1, hci::Connection::Role::kSlave, nullptr, nullptr,
                             nullptr);
    l2cap_->AddACLConnection(kTestHandle2, hci::Connection::Role::kSlave, nullptr, nullptr,
                             nullptr);
    server_ = std::make_unique<Server>(l2cap_);
  }

  void TearDown() override {
    channel_ = nullptr;
    server_ = nullptr;
    l2cap_ = nullptr;
  }

  Server* server() const { return server_.get(); }

  fbl::RefPtr<data::testing::FakeDomain> l2cap() const { return l2cap_; }

  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan() const { return channel_; }

  bool Expect(const ByteBuffer& expected) {
    if (!fake_chan()) {
      bt_log(ERROR, "unittest", "no channel, failing!");
      return false;
    }

    bool success = false;
    auto cb = [&expected, &success](auto cb_packet) {
      success = ContainersEqual(expected, *cb_packet);
    };

    fake_chan()->SetSendCallback(cb, dispatcher());
    RunLoopUntilIdle();

    return success;
  }

  bool ReceiveAndExpect(const ByteBuffer& packet, const ByteBuffer& expected_response) {
    if (!fake_chan()) {
      bt_log(ERROR, "unittest", "no channel, failing!");
      return false;
    }

    fake_chan()->Receive(packet);

    return Expect(expected_response);
  }

  ServiceHandle AddSPP(sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;

    record.SetServiceClassUUIDs({profile::kSerialPort});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement());
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kRFCOMM,
                                 DataElement(uint8_t(0)));
    record.AddProfile(profile::kSerialPort, 1, 2);
    record.AddInfo("en", "FAKE", "", "");
    ServiceHandle handle =
        server()->RegisterService(std::move(record), kChannelParams, std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

  ServiceHandle AddA2DPSink(sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;
    record.SetServiceClassUUIDs({profile::kAudioSink});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement(l2cap::kAVDTP));
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kAVDTP,
                                 DataElement(uint16_t(0x0103)));  // Version
    record.AddProfile(profile::kAdvancedAudioDistribution, 1, 3);
    record.SetAttribute(kA2DP_SupportedFeatures,
                        DataElement(uint16_t(0x0001)));  // Headphones
    ServiceHandle handle =
        server()->RegisterService(std::move(record), kChannelParams, std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

  ServiceHandle AddL2capService(l2cap::PSM channel,
                                l2cap::ChannelParameters chan_params = kChannelParams,
                                sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;
    record.SetServiceClassUUIDs({profile::kAudioSink});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement(channel));
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kAVDTP,
                                 DataElement(uint16_t(0x0103)));  // Version
    record.AddProfile(profile::kAdvancedAudioDistribution, 1, 3);
    record.SetAttribute(kA2DP_SupportedFeatures,
                        DataElement(uint16_t(0x0001)));  // Headphones
    ServiceHandle handle = server()->RegisterService(std::move(record), chan_params, std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

 private:
  fbl::RefPtr<l2cap::testing::FakeChannel> channel_;
  fbl::RefPtr<data::testing::FakeDomain> l2cap_;
  std::unique_ptr<Server> server_;
};

constexpr l2cap::ChannelId kSdpChannel = 0x0041;

#define SDP_ERROR_RSP(t_id, code)                                            \
  CreateStaticByteBuffer(0x01, UpperBits(t_id), LowerBits(t_id), 0x00, 0x02, \
                         UpperBits(uint16_t(code)), LowerBits(uint16_t(code)));

// Test:
//  - Accepts channels and holds channel open correctly.
//  - Packets that are the wrong length are responded to with kInvalidSize
//  - Answers with the same TransactionID as sent
TEST_F(SDP_ServerTest, BasicError) {
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();
  ASSERT_TRUE(fake_chan());
  EXPECT_TRUE(fake_chan()->activated());

  const auto kRspErrSize = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);

  const auto kTooSmall = CreateStaticByteBuffer(0x01,        // SDP_ServiceSearchRequest
                                                0x10, 0x01,  // Transaction ID (0x1001)
                                                0x00, 0x09   // Parameter length (9 bytes)
  );

  const auto kRspTooSmall = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);

  const auto kTooBig = CreateStaticByteBuffer(0x01,             // SDP_ServiceSearchRequest
                                              0x20, 0x10,       // Transaction ID (0x2010)
                                              0x00, 0x02,       // Parameter length (2 bytes)
                                              0x01, 0x02, 0x03  // 3 bytes of parameters
  );

  const auto kRspTooBig = SDP_ERROR_RSP(0x2010, ErrorCode::kInvalidSize);

  EXPECT_TRUE(ReceiveAndExpect(kTooSmall, kRspTooSmall));
  EXPECT_TRUE(ReceiveAndExpect(kTooBig, kRspTooBig));

  const auto kRspInvalidSyntax = SDP_ERROR_RSP(0x2010, ErrorCode::kInvalidRequestSyntax);

  // Responses aren't valid requests
  EXPECT_TRUE(ReceiveAndExpect(kRspTooBig, kRspInvalidSyntax));
}

// Test:
//  - Passes an initialized ServiceRecord that has a matching ServiceHandle
//  - Doesn't add a service that doesn't contain a ServiceClassIDList
//  - Adds a service that is valid.
//  - Services can be Unregistered.
TEST_F(SDP_ServerTest, RegisterService) {
  EXPECT_FALSE(server()->RegisterService(ServiceRecord(), kChannelParams, {}));

  ServiceRecord record;
  record.SetAttribute(kServiceClassIdList, DataElement(uint16_t(42)));
  EXPECT_FALSE(server()->RegisterService(std::move(record), kChannelParams, {}));

  ServiceRecord has_handle;
  has_handle.SetHandle(42);
  EXPECT_FALSE(server()->RegisterService(std::move(has_handle), kChannelParams, {}));

  ServiceRecord valid;
  valid.SetServiceClassUUIDs({profile::kAVRemoteControl});
  ServiceHandle handle = server()->RegisterService(std::move(valid), kChannelParams, {});

  EXPECT_TRUE(handle);

  EXPECT_TRUE(server()->UnregisterService(handle));
  EXPECT_FALSE(server()->UnregisterService(handle));
}

// Test:
// - Adds a primary protocol to the service defintion.
// - Adds multiple additional protocols to the service definition.
// - Tests registration and removal are successful.
// - Tests callback correctness when inbound l2cap channels are connected.
TEST_F(SDP_ServerTest, RegisterServiceWithAdditionalProtocol) {
  std::vector<l2cap::PSM> psms{500, 27, 29};

  ServiceRecord psm_additional;
  psm_additional.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_additional.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                       DataElement(uint16_t(psms[0])));
  psm_additional.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t(psms[1])));
  psm_additional.AddProtocolDescriptor(2, protocol::kL2CAP, DataElement(uint16_t(psms[2])));

  std::vector<uint16_t> protocols_discovered;
  auto cb = [&](auto /*socket*/, auto /*handle*/, auto& protocol_list) {
    EXPECT_EQ(DataElement::Type::kSequence, protocol_list.type());
    auto* psm = protocol_list.At(0);
    EXPECT_EQ(DataElement::Type::kSequence, psm->type());
    psm = psm->At(1);
    EXPECT_EQ(DataElement::Type::kUnsignedInt, psm->type());
    protocols_discovered.emplace_back(*psm->template Get<uint16_t>());
  };

  ServiceHandle handle =
      server()->RegisterService(std::move(psm_additional), kChannelParams, std::move(cb));

  EXPECT_TRUE(handle);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, psms[0], 0x40, 0x41));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, psms[1], 0x42, 0x43));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, psms[2], 0x44, 0x44));
  RunLoopUntilIdle();

  ASSERT_EQ(3u, protocols_discovered.size());
  // There should be one connection (and therefore protocol_list) per psm registered.
  for (auto& psm : psms) {
    ASSERT_EQ(1u, std::count(protocols_discovered.begin(), protocols_discovered.end(), psm));
  }

  EXPECT_TRUE(server()->UnregisterService(handle));
}

// Test:
// - Adds a primary protocol to the service defintion.
// - Adds an additional protocol to the service definition.
// - Adds an additional protocol with missing information.
// - Tests that none of protocols are registered.
TEST_F(SDP_ServerTest, RegisterServiceWithIncompleteAdditionalProtocol) {
  ServiceRecord psm_additional;
  psm_additional.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_additional.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                       DataElement(uint16_t(500)));
  psm_additional.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t(27)));
  psm_additional.AddProtocolDescriptor(2, protocol::kL2CAP, DataElement());

  size_t cb_count = 0;
  auto cb = [&](auto /*socket*/, auto /*handle*/, auto& /* protocol_list */) { cb_count++; };

  ServiceHandle handle =
      server()->RegisterService(std::move(psm_additional), kChannelParams, std::move(cb));

  EXPECT_FALSE(handle);
  EXPECT_FALSE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 500, 0x40, 0x41));
  RunLoopUntilIdle();

  // Despite an incoming L2CAP connection, the callback should never be triggered since
  // no services should be registered.
  EXPECT_EQ(0u, cb_count);

  EXPECT_FALSE(server()->UnregisterService(handle));
}

TEST_F(SDP_ServerTest, PSMVerification) {
  ServiceRecord no_psm;
  no_psm.SetServiceClassUUIDs({profile::kAVRemoteControl});
  no_psm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                               DataElement());

  EXPECT_FALSE(server()->RegisterService(std::move(no_psm), kChannelParams, {}));

  ServiceRecord psm_wrong_argtype;
  psm_wrong_argtype.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_wrong_argtype.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                          DataElement(bool(true)));

  EXPECT_FALSE(server()->RegisterService(std::move(psm_wrong_argtype), kChannelParams, {}));

  ServiceRecord psm_rfcomm;
  psm_rfcomm.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_rfcomm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                   DataElement());
  psm_rfcomm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kRFCOMM,
                                   DataElement(uint16_t(5)));

  EXPECT_TRUE(server()->RegisterService(std::move(psm_rfcomm), kChannelParams, NopConnectCallback));

  // Another RFCOMM should fail, even with a different channel.
  ServiceRecord psm_rfcomm2;
  psm_rfcomm2.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_rfcomm2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                    DataElement());
  psm_rfcomm2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kRFCOMM,
                                    DataElement(uint16_t(5)));

  EXPECT_FALSE(
      server()->RegisterService(std::move(psm_rfcomm2), kChannelParams, NopConnectCallback));

  ServiceRecord psm_ok;
  psm_ok.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_ok.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                               DataElement(uint16_t(500)));

  auto handle = server()->RegisterService(std::move(psm_ok), kChannelParams, NopConnectCallback);
  EXPECT_TRUE(handle);

  ServiceRecord psm_same;
  psm_same.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_same.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement(uint16_t(500)));

  EXPECT_FALSE(server()->RegisterService(std::move(psm_same), kChannelParams, NopConnectCallback));

  // Unregistering allows us to re-register with PSM.
  server()->UnregisterService(handle);
  ServiceRecord psm_readd;
  psm_readd.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_readd.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                  DataElement(uint16_t(500)));

  EXPECT_TRUE(server()->RegisterService(std::move(psm_readd), kChannelParams, NopConnectCallback));
}

#define UINT32_AS_BE_BYTES(x) \
  UpperBits(x >> 16), LowerBits(x >> 16), UpperBits(x & 0xFFFF), LowerBits(x & 0xFFFF)

// Test ServiceSearchRequest:
//  - returns services with the UUID included
//  - doesn't return services that don't have the UUID
//  - fails when there are no items or too many items in the search
//  - doesn't return more than the max requested
TEST_F(SDP_ServerTest, ServiceSearchRequest) {
  ServiceHandle spp_handle = AddSPP();
  ServiceHandle a2dp_handle = AddA2DPSink();

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kL2capSearch = CreateStaticByteBuffer(0x02,        // SDP_ServiceSearchRequest
                                                   0x10, 0x01,  // Transaction ID (0x1001)
                                                   0x00, 0x08,  // Parameter length (8 bytes)
                                                   // ServiceSearchPattern
                                                   0x35, 0x03,        // Sequence uint8 3 bytes
                                                   0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                                   0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
                                                   0x00         // Contunuation State: none
  );

  ServiceSearchRequest search_req;
  EXPECT_FALSE(search_req.valid());
  EXPECT_EQ(nullptr, search_req.GetPDU(0x1001));

  search_req.set_search_pattern({protocol::kL2CAP});

  auto pdu = search_req.GetPDU(0x1001);
  EXPECT_NE(nullptr, pdu);

  EXPECT_TRUE(ContainersEqual(kL2capSearch, *pdu));

  const auto kL2capSearchResponse = CreateStaticByteBuffer(
      0x03,                            // SDP_ServicesearchResponse
      0x10, 0x01,                      // Transaction ID (0x1001)
      0x00, 0x0D,                      // Parameter length (13 bytes)
      0x00, 0x02,                      // Total service record count: 2
      0x00, 0x02,                      // Current service record count: 2
      UINT32_AS_BE_BYTES(spp_handle),  // This list isn't specifically ordered
      UINT32_AS_BE_BYTES(a2dp_handle),
      0x00  // No continuation state
  );

  bool recv = false;
  std::vector<ServiceHandle> handles;
  TransactionId tid;
  auto cb = [&recv, &handles, &tid](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<Header> packet(cb_packet.get());
    EXPECT_EQ(kServiceSearchResponse, packet.header().pdu_id);
    tid = betoh16(packet.header().tid);
    uint16_t len = betoh16(packet.header().param_length);
    bt_log(SPEW, "unittest", "resize packet to %d", len);
    packet.Resize(len);
    ServiceSearchResponse resp;
    auto status = resp.Parse(packet.payload_data());
    EXPECT_TRUE(status);
    handles = resp.service_record_handle_list();
    recv = true;
  };

  fake_chan()->SetSendCallback(cb, dispatcher());
  fake_chan()->Receive(kL2capSearch);
  RunLoopUntilIdle();

  EXPECT_TRUE(recv);
  EXPECT_EQ(0x1001, tid);
  EXPECT_EQ(2u, handles.size());
  EXPECT_NE(handles.end(), std::find(handles.begin(), handles.end(), spp_handle));
  EXPECT_NE(handles.end(), std::find(handles.begin(), handles.end(), a2dp_handle));

  const auto kInvalidNoItems =
      CreateStaticByteBuffer(0x02,        // SDP_ServiceSearchRequest
                             0x10, 0xA1,  // Transaction ID (0x10A1)
                             0x00, 0x05,  // Parameter length (5 bytes)
                             // ServiceSearchPattern
                             0x35, 0x00,  // Sequence uint8 0 bytes
                             0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
                             0x00         // Contunuation State: none
      );

  const auto kRspErrSyntax = SDP_ERROR_RSP(0x10A1, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidNoItems, kRspErrSyntax));

  const auto kInvalidTooManyItems = CreateStaticByteBuffer(
      0x02,        // SDP_ServiceSearchRequest
      0x10, 0xA1,  // Transaction ID (0x10B1)
      0x00, 0x2C,  // Parameter length (44 bytes)
      // ServiceSearchPattern
      0x35, 0x27,        // Sequence uint8 27 bytes
      0x19, 0x30, 0x01,  // 13 UUIDs in the search
      0x19, 0x30, 0x02, 0x19, 0x30, 0x03, 0x19, 0x30, 0x04, 0x19, 0x30, 0x05, 0x19, 0x30, 0x06,
      0x19, 0x30, 0x07, 0x19, 0x30, 0x08, 0x19, 0x30, 0x09, 0x19, 0x30, 0x10, 0x19, 0x30, 0x11,
      0x19, 0x30, 0x12, 0x19, 0x30, 0x13, 0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
      0x00                                             // Contunuation State: none
  );

  EXPECT_TRUE(ReceiveAndExpect(kInvalidTooManyItems, kRspErrSyntax));
}

// Test ServiceSearchRequest:
//  - doesn't return more than the max requested
TEST_F(SDP_ServerTest, ServiceSearchRequestOneOfMany) {
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  ServiceHandle spp_handle = AddSPP();
  ServiceHandle a2dp_handle = AddA2DPSink();

  bool recv = false;
  std::vector<ServiceHandle> handles;
  TransactionId tid;
  auto cb = [&recv, &handles, &tid](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<Header> packet(cb_packet.get());
    EXPECT_EQ(kServiceSearchResponse, packet.header().pdu_id);
    tid = betoh16(packet.header().tid);
    uint16_t len = betoh16(packet.header().param_length);
    bt_log(SPEW, "unittests", "resizing packet to %d", len);
    packet.Resize(len);
    ServiceSearchResponse resp;
    auto status = resp.Parse(packet.payload_data());
    EXPECT_TRUE(status);
    handles = resp.service_record_handle_list();
    recv = true;
  };

  const auto kL2capSearchOne = CreateStaticByteBuffer(0x02,        // SDP_ServiceSearchRequest
                                                      0x10, 0xC1,  // Transaction ID (0x10C1)
                                                      0x00, 0x08,  // Parameter length (8 bytes)
                                                      // ServiceSearchPattern
                                                      0x35, 0x03,        // Sequence uint8 3 bytes
                                                      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                                      0x00, 0x01,  // MaximumServiceRecordCount: 1
                                                      0x00         // Contunuation State: none
  );

  handles.clear();
  recv = false;

  fake_chan()->SetSendCallback(cb, dispatcher());
  fake_chan()->Receive(kL2capSearchOne);
  RunLoopUntilIdle();

  EXPECT_TRUE(recv);
  EXPECT_EQ(0x10C1, tid);
  EXPECT_EQ(1u, handles.size());
  bool found_spp = std::find(handles.begin(), handles.end(), spp_handle) != handles.end();
  bool found_a2dp = std::find(handles.begin(), handles.end(), a2dp_handle) != handles.end();
  EXPECT_TRUE(found_spp || found_a2dp);
}

// Test ServiceSearchRequest:
//  - returns continuation state if too many services match
//  - continuation state in request works correctly
TEST_F(SDP_ServerTest, ServiceSearchContinuationState) {
  // Set the TX MTU to the lowest that's allowed (48)
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad,
                                                  48 /* tx_mtu */));
  RunLoopUntilIdle();

  // Add enough services to generate a continuation state.
  AddL2capService(0x1001);
  AddL2capService(0x1003);
  AddL2capService(0x1005);
  AddL2capService(0x1007);
  AddL2capService(0x1009);
  AddL2capService(0x100B);
  AddL2capService(0x100D);
  AddL2capService(0x100F);
  AddL2capService(0x1011);
  AddL2capService(0x1013);
  AddL2capService(0x1015);

  size_t received = 0;
  ServiceSearchResponse rsp;

  auto send_cb = [this, &rsp, &received](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x03, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    EXPECT_LE(len,
              0x2F);  // 10 records (4 * 10) + 2 (total count) + 2 (current count) + 3 (cont state)
    packet.Resize(len);
    Status st = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_FALSE(st);
      EXPECT_EQ(HostError::kInProgress, st.error());
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (!st && (st.error() != HostError::kInProgress)) {
      // This isn't a valid packet and we shouldn't try to get
      // a continuation.
      return;
    }
    if (!rsp.complete()) {
      // Repeat the request with the continuation state if it was returned.
      auto continuation = rsp.ContinuationState();
      uint8_t cont_size = continuation.size();
      EXPECT_NE(0u, cont_size);
      // Make another request with the continutation data.
      size_t param_size = 8 + cont_size;
      auto kContinuedRequestStart =
          CreateStaticByteBuffer(0x02,        // SDP_ServiceSearchRequest
                                 0x10, 0xC1,  // Transaction ID (0x10C1)
                                 UpperBits(param_size), LowerBits(param_size),  // Parameter length
                                 // ServiceSearchPattern
                                 0x35, 0x03,        // Sequence uint8 3 bytes
                                 0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                 0x00, 0xFF         // MaximumServiceRecordCount: 256
          );

      DynamicByteBuffer req(kContinuedRequestStart.size() + sizeof(uint8_t) + cont_size);

      kContinuedRequestStart.Copy(&req);
      req.Write(&cont_size, sizeof(uint8_t), kContinuedRequestStart.size());
      req.Write(continuation, kContinuedRequestStart.size() + sizeof(uint8_t));

      fake_chan()->Receive(req);
    }
  };

  const auto kL2capSearch = CreateStaticByteBuffer(0x02,        // SDP_ServiceSearchRequest
                                                   0x10, 0xC1,  // Transaction ID (0x10C1)
                                                   0x00, 0x08,  // Parameter length (8 bytes)
                                                   // ServiceSearchPattern
                                                   0x35, 0x03,        // Sequence uint8 3 bytes
                                                   0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                                   0x00, 0xFF,  // MaximumServiceRecordCount: 256
                                                   0x00         // Contunuation State: none
  );

  fake_chan()->SetSendCallback(send_cb, dispatcher());
  fake_chan()->Receive(kL2capSearch);
  RunLoopUntilIdle();

  EXPECT_GE(received, 1u);
  EXPECT_EQ(11u, rsp.service_record_handle_list().size());
}

// Test:
//  - Answers ServiceAttributeRequest correctly
//  - Continuation state is generated correctly re:
//    MaximumAttributeListByteCount
//  - Valid Continuation state continues response
TEST_F(SDP_ServerTest, ServiceAttributeRequest) {
  ServiceRecord record;
  record.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record.SetAttribute(0xf00d, DataElement(uint32_t(0xfeedbeef)));
  record.SetAttribute(0xf000, DataElement(uint32_t(0x01234567)));

  ServiceHandle handle = server()->RegisterService(std::move(record), kChannelParams, {});

  EXPECT_TRUE(handle);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kRequestAttr =
      CreateStaticByteBuffer(0x04,                        // SDP_ServiceAttritbuteRequest
                             0x10, 0x01,                  // Transaction ID (0x1001)
                             0x00, 0x11,                  // Parameter length (17 bytes)
                             UINT32_AS_BE_BYTES(handle),  // ServiceRecordHandle
                             0x00, 0x0A,  // MaximumAttributeByteCount (10 bytes max)
                             // AttributeIDList
                             0x35, 0x08,  // Sequence uint8 8 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x01,  // ServiceClassIDList
                             0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
                             0x30, 0x00,  // low end of range
                             0xf0, 0x00,  // high end of range
                             0x00         // Contunuation State: none
      );

  size_t received = 0;

  ServiceAttributeResponse rsp;

  auto send_cb = [this, handle, &rsp, &received](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x05, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    EXPECT_LE(len, 0x11);  // 10 + 2 (byte count) + 5 (cont state)
    packet.Resize(len);
    Status st = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_FALSE(st);
      EXPECT_EQ(HostError::kInProgress, st.error());
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (!st && (st.error() != HostError::kInProgress)) {
      // This isn't a valid packet and we shouldn't try to get
      // a continuation.
      return;
    }
    if (!rsp.complete()) {
      // Repeat the request with the continuation state if it was returned.
      auto continuation = rsp.ContinuationState();
      uint8_t cont_size = continuation.size();
      EXPECT_NE(0u, cont_size);
      // Make another request with the continutation data.
      size_t param_size = 17 + cont_size;
      auto kContinuedRequestAttrStart =
          CreateStaticByteBuffer(0x04,        // SDP_ServiceAttributeRequest
                                 0x10, 0x01,  // Transaction ID (reused)
                                 UpperBits(param_size), LowerBits(param_size),  // Parameter length
                                 UINT32_AS_BE_BYTES(handle),  // ServiceRecordHandle
                                 0x00, 0x0A,  // MaximumAttributeByteCount (10 bytes max)
                                 // AttributeIDList
                                 0x35, 0x08,  // Sequence uint8 8 bytes
                                 0x09,        // uint16_t, single attribute
                                 0x00, 0x01,  // ServiceClassIDList
                                 0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
                                 0x30, 0x00,  // low end of range
                                 0xf0, 0x00   // high end of range
          );
      DynamicByteBuffer req(kContinuedRequestAttrStart.size() + sizeof(uint8_t) + cont_size);

      kContinuedRequestAttrStart.Copy(&req);
      req.Write(&cont_size, sizeof(uint8_t), kContinuedRequestAttrStart.size());
      req.Write(continuation, kContinuedRequestAttrStart.size() + sizeof(uint8_t));

      fake_chan()->Receive(req);
    }
  };

  fake_chan()->SetSendCallback(send_cb, dispatcher());
  fake_chan()->Receive(kRequestAttr);
  RunLoopUntilIdle();

  EXPECT_GE(received, 1u);
  const auto& attrs = rsp.attributes();
  EXPECT_EQ(2u, attrs.size());
  EXPECT_NE(attrs.end(), attrs.find(kServiceClassIdList));
  EXPECT_NE(attrs.end(), attrs.find(0xf000));

  const auto kInvalidRangeOrder =
      CreateStaticByteBuffer(0x04,                        // SDP_ServiceAttritbuteRequest
                             0xE0, 0x01,                  // Transaction ID (0xE001)
                             0x00, 0x11,                  // Parameter length (17 bytes)
                             UINT32_AS_BE_BYTES(handle),  // ServiceRecordHandle
                             0x00, 0x0A,  // MaximumAttributeByteCount (10 bytes max)
                             // AttributeIDList
                             0x35, 0x08,  // Sequence uint8 8 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x01,  // ServiceClassIDList
                             0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
                             0xf0, 0x00,  // low end of range
                             0x30, 0x00,  // high end of range
                             0x00         // Contunuation State: none
      );

  const auto kRspErrSyntax = SDP_ERROR_RSP(0xE001, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidRangeOrder, kRspErrSyntax));

  const auto kInvalidMaxBytes =
      CreateStaticByteBuffer(0x04,                        // SDP_ServiceAttritbuteRequest
                             0xE0, 0x02,                  // Transaction ID (0xE001)
                             0x00, 0x0C,                  // Parameter length (12 bytes)
                             UINT32_AS_BE_BYTES(handle),  // ServiceRecordHandle
                             0x00, 0x05,                  // MaximumAttributeByteCount (5 bytes max)
                             // AttributeIDList
                             0x35, 0x03,  // Sequence uint8 3 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x01,  // ServiceClassIDList
                             0x00         // Contunuation State: none
      );

  const auto kRspErrSyntax2 = SDP_ERROR_RSP(0xE002, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidMaxBytes, kRspErrSyntax2));
}

// Test:
//  - Answers ServiceSearchAttributeRequest correctly
//  - Continuation state is generated correctly re:
//    MaximumAttributeListsByteCount
//  - Valid Continuation state continues response
TEST_F(SDP_ServerTest, SearchAttributeRequest) {
  ServiceRecord record1;
  record1.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record1.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                DataElement(uint16_t(500)));
  record1.SetAttribute(0xf00d, DataElement(uint32_t(0xfeedbeef)));
  record1.SetAttribute(0xf000, DataElement(uint32_t(0x01234567)));

  ServiceHandle handle1 =
      server()->RegisterService(std::move(record1), kChannelParams, NopConnectCallback);

  EXPECT_TRUE(handle1);

  ServiceRecord record2;
  record2.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                DataElement(uint16_t(501)));
  ServiceHandle handle2 =
      server()->RegisterService(std::move(record2), kChannelParams, NopConnectCallback);

  EXPECT_TRUE(handle2);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kRequestAttr =
      CreateStaticByteBuffer(0x06,        // SDP_ServiceAttritbuteRequest
                             0x10, 0x01,  // Transaction ID (0x1001)
                             0x00, 0x12,  // Parameter length (18 bytes)
                             // ServiceSearchPattern
                             0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0x00, 0x0A,        // MaximumAttributeByteCount (10 bytes max)
                             // AttributeIDList
                             0x35, 0x08,  // Sequence uint8 8 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x00,  // ServiceRecordHandle
                             0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
                             0x30, 0x00,  // low end of range
                             0xf0, 0x00,  // high end of range
                             0x00         // Contunuation State: none
      );

  size_t received = 0;

  ServiceSearchAttributeResponse rsp;

  auto send_cb = [this, &rsp, &received](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x07, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    EXPECT_LE(len, 0x11);  // 2 (byte count) + 10 (max len) + 5 (cont state)
    packet.Resize(len);
    Status st = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_FALSE(st);
      EXPECT_EQ(HostError::kInProgress, st.error());
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (!st && (st.error() != HostError::kInProgress)) {
      // This isn't a valid packet and we shouldn't try to get
      // a continuation.
      return;
    }
    if (!rsp.complete()) {
      // Repeat the request with the continuation state if it was returned.
      auto continuation = rsp.ContinuationState();
      uint8_t cont_size = continuation.size();
      EXPECT_NE(0u, cont_size);
      // Make another request with the continutation data.
      size_t param_size = 18 + cont_size;
      auto kContinuedRequestAttrStart =
          CreateStaticByteBuffer(0x06,  // SDP_ServiceAttributeRequest
                                 0x10, static_cast<uint8_t>(received + 1),      // Transaction ID
                                 UpperBits(param_size), LowerBits(param_size),  // Parameter length
                                 0x35, 0x03,        // Sequence uint8 3 bytes
                                 0x19, 0x01, 0x00,  // SearchPattern: L2CAP
                                 0x00, 0x0A,        // MaximumAttributeByteCount (10 bytes max)
                                 // AttributeIDList
                                 0x35, 0x08,  // Sequence uint8 8 bytes
                                 0x09,        // uint16_t, single attribute
                                 0x00, 0x00,  // ServiceRecordHandle
                                 0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
                                 0x30, 0x00,  // low end of range
                                 0xf0, 0x00   // high end of range
          );
      DynamicByteBuffer req(kContinuedRequestAttrStart.size() + sizeof(uint8_t) + cont_size);

      kContinuedRequestAttrStart.Copy(&req);
      req.Write(&cont_size, sizeof(uint8_t), kContinuedRequestAttrStart.size());
      req.Write(continuation, kContinuedRequestAttrStart.size() + sizeof(uint8_t));

      fake_chan()->Receive(req);
    }
  };

  fake_chan()->SetSendCallback(send_cb, dispatcher());
  fake_chan()->Receive(kRequestAttr);
  RunLoopUntilIdle();

  EXPECT_GE(received, 1u);
  // We should receive both of our entered records.
  EXPECT_EQ(2u, rsp.num_attribute_lists());
  for (size_t i = 0; i < rsp.num_attribute_lists(); i++) {
    const auto& attrs = rsp.attributes(i);
    // Every service has a record handle
    auto handle_it = attrs.find(kServiceRecordHandle);
    EXPECT_NE(attrs.end(), handle_it);
    ServiceHandle received_handle = *handle_it->second.Get<uint32_t>();
    if (received_handle == handle1) {
      // The first service also has another attribute we should find.
      EXPECT_EQ(2u, attrs.size());
      EXPECT_NE(attrs.end(), attrs.find(0xf000));
    }
  }

  const auto kInvalidRangeOrder =
      CreateStaticByteBuffer(0x06,                          // SDP_ServiceAttritbuteRequest
                             0xE0, 0x01,                    // Transaction ID (0xE001)
                             0x00, 0x12,                    // Parameter length (18 bytes)
                             0x35, 0x03, 0x19, 0x01, 0x00,  // SearchPattern: L2CAP
                             0x00, 0x0A,  // MaximumAttributeByteCount (10 bytes max)
                             // AttributeIDList
                             0x35, 0x08,  // Sequence uint8 8 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x01,  // ServiceClassIDList
                             0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
                             0xf0, 0x00,  // low end of range
                             0x30, 0x00,  // high end of range
                             0x00         // Contunuation State: none
      );

  const auto kRspErrSyntax = SDP_ERROR_RSP(0xE001, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidRangeOrder, kRspErrSyntax));

  const auto kInvalidMaxBytes =
      CreateStaticByteBuffer(0x04,                          // SDP_ServiceAttritbuteRequest
                             0xE0, 0x02,                    // Transaction ID (0xE002)
                             0x00, 0x0D,                    // Parameter length (13 bytes)
                             0x35, 0x03, 0x19, 0x01, 0x00,  // SearchPattern: L2CAP
                             0x00, 0x05,  // MaximumAttributeByteCount (5 bytes max)
                             // AttributeIDList
                             0x35, 0x03,  // Sequence uint8 3 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x01,  // ServiceClassIDList
                             0x00         // Contunuation State: none
      );

  const auto kRspErrSyntax2 = SDP_ERROR_RSP(0xE002, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidMaxBytes, kRspErrSyntax2));
}

TEST_F(SDP_ServerTest, ConnectionCallbacks) {
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  std::vector<zx::socket> socks;
  hci::ConnectionHandle latest_handle;

  // Register a service
  AddA2DPSink([&socks, &latest_handle](auto chan_sock, auto handle, const auto& protocol) {
    bt_log(SPEW, "test", "Got socket for the a2dp sink");
    socks.emplace_back(std::move(chan_sock.socket));
    latest_handle = handle;
  });

  // Connect to the service
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kAVDTP, kSdpChannel + 1, 0x0b00));
  RunLoopUntilIdle();

  // It should get a callback with a socket
  EXPECT_EQ(1u, socks.size());
  EXPECT_EQ(kTestHandle1, latest_handle);

  // Connect to the same service again with the same PSM (on a different
  // connection, it should still work)
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(kTestHandle2, l2cap::kAVDTP, kSdpChannel + 2, 0x0b01));
  RunLoopUntilIdle();

  ASSERT_EQ(2u, socks.size());
  EXPECT_EQ(kTestHandle2, latest_handle);
  EXPECT_NE(socks.front(), socks.back());
}

// Browse Group gets set correctly
TEST_F(SDP_ServerTest, BrowseGroup) {
  AddA2DPSink();

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kRequestAttr =
      CreateStaticByteBuffer(0x06,        // SDP_ServiceAttritbuteRequest
                             0x10, 0x01,  // Transaction ID (0x1001)
                             0x00, 0x0D,  // Parameter length (12 bytes)
                             // ServiceSearchPattern
                             0x35, 0x03,        // Sequence uint8 3 bytes
                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                             0xFF, 0xFF,        // MaximumAttributeByteCount (no max)
                             // AttributeIDList
                             0x35, 0x03,  // Sequence uint8 3 bytes
                             0x09,        // uint16_t, single attribute
                             0x00, 0x05,  // BrowseGroupList
                             0x00         // Contunuation State: none
      );

  ServiceSearchAttributeResponse rsp;
  auto send_cb = [&rsp](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x07, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    packet.Resize(len);
    auto status = rsp.Parse(packet.payload_data());
    EXPECT_TRUE(status);
  };

  fake_chan()->SetSendCallback(send_cb, dispatcher());
  fake_chan()->Receive(kRequestAttr);
  RunLoopUntilIdle();

  EXPECT_EQ(1u, rsp.num_attribute_lists());
  auto& attributes = rsp.attributes(0);
  auto group_attr_it = attributes.find(kBrowseGroupList);
  ASSERT_EQ(DataElement::Type::kSequence, group_attr_it->second.type());
  ASSERT_EQ(DataElement::Type::kUuid, group_attr_it->second.At(0)->type());
  EXPECT_NE(attributes.end(), group_attr_it);
  EXPECT_EQ(kPublicBrowseRootUuid, *group_attr_it->second.At(0)->Get<UUID>());
}

// Channels created for a service registered with channel parameters should be configured with that
// service's channel parameters.
TEST_F(SDP_ServerTest, RegisterServiceWithChannelParameters) {
  l2cap::PSM kPSM = l2cap::kAVDTP;

  l2cap::ChannelParameters preferred_params;
  preferred_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  preferred_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  std::optional<l2cap::ChannelInfo> params;
  size_t chan_cb_count = 0;
  ASSERT_TRUE(AddL2capService(kPSM, preferred_params,
                              [&](auto chan_sock, auto /*handle*/, auto& /*protocol*/) {
                                chan_cb_count++;
                                params = chan_sock.params;
                              }));

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, kPSM, 0x40, 0x41));
  RunLoopUntilIdle();
  EXPECT_EQ(1u, chan_cb_count);
  ASSERT_TRUE(params);
  EXPECT_EQ(*preferred_params.mode, params->mode);
  EXPECT_EQ(*preferred_params.max_rx_sdu_size, params->max_rx_sdu_size);
}

// Test:
// - Creation of ServiceDiscoveryService is valid.
TEST_F(SDP_ServerTest, MakeServiceDiscoveryServiceIsValid) {
  auto sdp_def = server()->MakeServiceDiscoveryService();

  const DataElement& version_number_list = sdp_def.GetAttribute(kSDP_VersionNumberList);
  EXPECT_EQ(DataElement::Type::kSequence, version_number_list.type());

  auto* it = version_number_list.At(0);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(0x0100u, it->Get<uint16_t>());
}

#undef SDP_ERROR_RSP
#undef UINT32_AS_LE_BYTES

}  // namespace
}  // namespace sdp
}  // namespace bt
