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

using common::LowerBits;
using common::UpperBits;

using bt::testing::FakeController;

using TestingBase = bt::testing::FakeControllerTest<FakeController>;

constexpr hci::ConnectionHandle kTestHandle1 = 1;
constexpr hci::ConnectionHandle kTestHandle2 = 2;

void NopConnectCallback(zx::socket, hci::ConnectionHandle, const DataElement&) {
}

class SDP_ServerTest : public TestingBase {
 public:
  SDP_ServerTest() = default;
  ~SDP_ServerTest() = default;

 protected:
  void SetUp() override {
    l2cap_ = data::testing::FakeDomain::Create();
    l2cap_->set_channel_callback(
        [this](auto fake_chan) { channel_ = std::move(fake_chan); });
    l2cap_->Initialize();
    l2cap_->AddACLConnection(kTestHandle1, hci::Connection::Role::kSlave,
                             nullptr, nullptr, nullptr);
    l2cap_->AddACLConnection(kTestHandle2, hci::Connection::Role::kSlave,
                             nullptr, nullptr, nullptr);
    server_ = std::make_unique<Server>(l2cap_);
  }

  void TearDown() override {
    channel_ = nullptr;
    server_ = nullptr;
    l2cap_ = nullptr;
  }

  Server* server() const { return server_.get(); }

  fbl::RefPtr<data::testing::FakeDomain> l2cap() const { return l2cap_; }

  fbl::RefPtr<l2cap::testing::FakeChannel> fake_chan() const {
    return channel_;
  }

  bool Expect(const common::ByteBuffer& expected) {
    if (!fake_chan()) {
      bt_log(ERROR, "unittest", "no channel, failing!");
      return false;
    }

    bool success = false;
    auto cb = [&expected, &success, this](auto cb_packet) {
      success = common::ContainersEqual(expected, *cb_packet);
    };

    fake_chan()->SetSendCallback(cb, dispatcher());
    RunLoopUntilIdle();

    return success;
  }

  bool ReceiveAndExpect(const common::ByteBuffer& packet,
                        const common::ByteBuffer& expected_response) {
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
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                 protocol::kL2CAP, DataElement());
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                 protocol::kRFCOMM, DataElement(uint8_t(0)));
    record.AddProfile(profile::kSerialPort, 1, 2);
    record.AddInfo("en", "FAKE", "", "");
    ServiceHandle handle =
        server()->RegisterService(std::move(record), std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

  ServiceHandle AddA2DPSink(
      sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;
    record.SetServiceClassUUIDs({profile::kAudioSink});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                 protocol::kL2CAP, DataElement(l2cap::kAVDTP));
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                 protocol::kAVDTP,
                                 DataElement(uint16_t(0x0103)));  // Version
    record.AddProfile(profile::kAdvancedAudioDistribution, 1, 3);
    record.SetAttribute(kA2DP_SupportedFeatures,
                        DataElement(uint16_t(0x0001)));  // Headphones
    ServiceHandle handle =
        server()->RegisterService(std::move(record), std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

 private:
  fbl::RefPtr<l2cap::testing::FakeChannel> channel_;
  fbl::RefPtr<data::testing::FakeDomain> l2cap_;
  std::unique_ptr<Server> server_;
};

constexpr l2cap::ChannelId kSdpChannel = 0x0041;

#define SDP_ERROR_RSP(t_id, code)                                              \
  common::CreateStaticByteBuffer(0x01, UpperBits(t_id), LowerBits(t_id), 0x00, \
                                 0x02, UpperBits(uint16_t(code)),              \
                                 LowerBits(uint16_t(code)));

// Test:
//  - Accepts channels and holds channel open correctly.
//  - Packets that are the wrong length are responded to with kInvalidSize
//  - Answers with the same TransactionID as sent
TEST_F(SDP_ServerTest, BasicError) {
  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();
  ASSERT_TRUE(fake_chan());
  EXPECT_TRUE(fake_chan()->activated());

  const auto kRspErrSize = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);

  const auto kTooSmall =
      common::CreateStaticByteBuffer(0x01,        // SDP_ServiceSearchRequest
                                     0x10, 0x01,  // Transaction ID (0x1001)
                                     0x00, 0x09   // Parameter length (9 bytes)
      );

  const auto kRspTooSmall = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);

  const auto kTooBig =
      common::CreateStaticByteBuffer(0x01,        // SDP_ServiceSearchRequest
                                     0x20, 0x10,  // Transaction ID (0x2010)
                                     0x00, 0x02,  // Parameter length (2 bytes)
                                     0x01, 0x02, 0x03  // 3 bytes of parameters
      );

  const auto kRspTooBig = SDP_ERROR_RSP(0x2010, ErrorCode::kInvalidSize);

  EXPECT_TRUE(ReceiveAndExpect(kTooSmall, kRspTooSmall));
  EXPECT_TRUE(ReceiveAndExpect(kTooBig, kRspTooBig));

  const auto kRspInvalidSyntax =
      SDP_ERROR_RSP(0x2010, ErrorCode::kInvalidRequestSyntax);

  // Responses aren't valid requests
  EXPECT_TRUE(ReceiveAndExpect(kRspTooBig, kRspInvalidSyntax));
}

// Test:
//  - Passes an initialized ServiceRecord that has a matching SericeHandle
//  - Doesn't add a service that doesn't contain a ServiceClassIDList
//  - Adds a service that is valid.
//  - Services can be Unregistered.
TEST_F(SDP_ServerTest, RegisterService) {
  EXPECT_FALSE(server()->RegisterService(ServiceRecord(), {}));

  ServiceRecord record;
  record.SetAttribute(kServiceClassIdList, DataElement(uint16_t(42)));
  EXPECT_FALSE(server()->RegisterService(std::move(record), {}));

  ServiceRecord has_handle;
  has_handle.SetHandle(42);
  EXPECT_FALSE(server()->RegisterService(std::move(has_handle), {}));

  ServiceRecord valid;
  valid.SetServiceClassUUIDs({profile::kAVRemoteControl});
  ServiceHandle handle = server()->RegisterService(std::move(valid), {});

  EXPECT_TRUE(handle);

  EXPECT_TRUE(server()->UnregisterService(handle));
  EXPECT_FALSE(server()->UnregisterService(handle));
}

TEST_F(SDP_ServerTest, PSMVerification) {
  ServiceRecord no_psm;
  no_psm.SetServiceClassUUIDs({profile::kAVRemoteControl});
  no_psm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                               protocol::kL2CAP, DataElement());

  EXPECT_FALSE(server()->RegisterService(std::move(no_psm), {}));

  ServiceRecord psm_wrong_argtype;
  psm_wrong_argtype.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_wrong_argtype.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                          protocol::kL2CAP,
                                          DataElement(bool(true)));

  EXPECT_FALSE(server()->RegisterService(std::move(psm_wrong_argtype), {}));

  ServiceRecord psm_rfcomm;
  psm_rfcomm.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_rfcomm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                   protocol::kL2CAP, DataElement());
  // Don't need an argument for RFCOMM, it will be auto-assigned.
  psm_rfcomm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                   protocol::kRFCOMM, DataElement());

  EXPECT_TRUE(
      server()->RegisterService(std::move(psm_rfcomm), NopConnectCallback));

  // Another RFCOMM is also fine.
  ServiceRecord psm_rfcomm2;
  psm_rfcomm2.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_rfcomm2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                    protocol::kL2CAP, DataElement());
  // Don't need an argument for RFCOMM, it will be auto-assigned.
  psm_rfcomm2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                    protocol::kRFCOMM, DataElement());

  EXPECT_TRUE(
      server()->RegisterService(std::move(psm_rfcomm2), NopConnectCallback));

  ServiceRecord psm_ok;
  psm_ok.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_ok.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                               protocol::kL2CAP, DataElement(uint16_t(500)));

  auto handle =
      server()->RegisterService(std::move(psm_ok), NopConnectCallback);
  EXPECT_TRUE(handle);

  ServiceRecord psm_duplicate;
  psm_duplicate.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_duplicate.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                      protocol::kL2CAP,
                                      DataElement(uint16_t(500)));

  EXPECT_FALSE(
      server()->RegisterService(std::move(psm_duplicate), NopConnectCallback));

  // Unregistering allows us to re-register with PSM.
  server()->UnregisterService(handle);
  ServiceRecord psm_readd;
  psm_readd.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_readd.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                  protocol::kL2CAP, DataElement(uint16_t(500)));

  EXPECT_TRUE(
      server()->RegisterService(std::move(psm_readd), NopConnectCallback));

  // TODO(NET-1417): test that new connections to the PSM get delivered once
  // they are deliverable
}

#define UINT32_AS_BE_BYTES(x)                                    \
  UpperBits(x >> 16), LowerBits(x >> 16), UpperBits(x & 0xFFFF), \
      LowerBits(x & 0xFFFF)

// Test ServiceSearchRequest:
//  - returns services with the UUID included
//  - doesn't return services that don't have the UUID
//  - fails when there are no items or too many items in the search
//  - doesn't return more than the max requested
TEST_F(SDP_ServerTest, ServiceSearchRequest) {
  ServiceHandle spp_handle = AddSPP();
  ServiceHandle a2dp_handle = AddA2DPSink();

  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();

  const auto kL2capSearch = common::CreateStaticByteBuffer(
      0x02,        // SDP_ServiceSearchRequest
      0x10, 0x01,  // Transaction ID (0x1001)
      0x00, 0x08,  // Parameter length (8 bytes)
      // ServiceSearchPattern
      0x35, 0x03,        // Sequence uint8 3 bytes
      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
      0xFF, 0xFF,        // MaximumServiceRecordCount: (none)
      0x00               // Contunuation State: none
  );

  ServiceSearchRequest search_req;
  EXPECT_FALSE(search_req.valid());
  EXPECT_EQ(nullptr, search_req.GetPDU(0x1001));

  search_req.set_search_pattern({protocol::kL2CAP});

  auto pdu = search_req.GetPDU(0x1001);
  EXPECT_NE(nullptr, pdu);

  EXPECT_TRUE(ContainersEqual(kL2capSearch, *pdu));

  const auto kL2capSearchResponse = common::CreateStaticByteBuffer(
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
    common::PacketView<Header> packet(cb_packet.get());
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
  EXPECT_NE(handles.end(),
            std::find(handles.begin(), handles.end(), spp_handle));
  EXPECT_NE(handles.end(),
            std::find(handles.begin(), handles.end(), a2dp_handle));

  const auto kInvalidNoItems = common::CreateStaticByteBuffer(
      0x02,        // SDP_ServiceSearchRequest
      0x10, 0xA1,  // Transaction ID (0x10A1)
      0x00, 0x05,  // Parameter length (5 bytes)
      // ServiceSearchPattern
      0x35, 0x00,  // Sequence uint8 0 bytes
      0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
      0x00         // Contunuation State: none
  );

  const auto kRspErrSyntax =
      SDP_ERROR_RSP(0x10A1, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidNoItems, kRspErrSyntax));

  const auto kInvalidTooManyItems = common::CreateStaticByteBuffer(
      0x02,        // SDP_ServiceSearchRequest
      0x10, 0xA1,  // Transaction ID (0x10B1)
      0x00, 0x2C,  // Parameter length (44 bytes)
      // ServiceSearchPattern
      0x35, 0x27,        // Sequence uint8 27 bytes
      0x19, 0x30, 0x01,  // 13 UUIDs in the search
      0x19, 0x30, 0x02, 0x19, 0x30, 0x03, 0x19, 0x30, 0x04, 0x19, 0x30, 0x05,
      0x19, 0x30, 0x06, 0x19, 0x30, 0x07, 0x19, 0x30, 0x08, 0x19, 0x30, 0x09,
      0x19, 0x30, 0x10, 0x19, 0x30, 0x11, 0x19, 0x30, 0x12, 0x19, 0x30, 0x13,
      0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
      0x00         // Contunuation State: none
  );

  EXPECT_TRUE(ReceiveAndExpect(kInvalidTooManyItems, kRspErrSyntax));
}

// Test ServiceSearchRequest:
//  - doesn't return more than the max requested
TEST_F(SDP_ServerTest, ServiceSearchRequestOneOfMany) {
  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();

  ServiceHandle spp_handle = AddSPP();
  ServiceHandle a2dp_handle = AddA2DPSink();

  bool recv = false;
  std::vector<ServiceHandle> handles;
  TransactionId tid;
  auto cb = [&recv, &handles, &tid](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    common::PacketView<Header> packet(cb_packet.get());
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

  const auto kL2capSearchOne = common::CreateStaticByteBuffer(
      0x02,        // SDP_ServiceSearchRequest
      0x10, 0xC1,  // Transaction ID (0x10C1)
      0x00, 0x08,  // Parameter length (8 bytes)
      // ServiceSearchPattern
      0x35, 0x03,        // Sequence uint8 3 bytes
      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
      0x00, 0x01,        // MaximumServiceRecordCount: 1
      0x00               // Contunuation State: none
  );

  handles.clear();
  recv = false;

  fake_chan()->SetSendCallback(cb, dispatcher());
  fake_chan()->Receive(kL2capSearchOne);
  RunLoopUntilIdle();

  EXPECT_TRUE(recv);
  EXPECT_EQ(0x10C1, tid);
  EXPECT_EQ(1u, handles.size());
  bool found_spp =
      std::find(handles.begin(), handles.end(), spp_handle) != handles.end();
  bool found_a2dp =
      std::find(handles.begin(), handles.end(), a2dp_handle) != handles.end();
  EXPECT_TRUE(found_spp || found_a2dp);
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

  ServiceHandle handle = server()->RegisterService(std::move(record), {});

  EXPECT_TRUE(handle);

  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();

  const auto kRequestAttr = common::CreateStaticByteBuffer(
      0x04,                        // SDP_ServiceAttritbuteRequest
      0x10, 0x01,                  // Transaction ID (0x1001)
      0x00, 0x11,                  // Parameter length (17 bytes)
      UINT32_AS_BE_BYTES(handle),  // ServiceRecordHandle
      0x00, 0x0A,                  // MaximumAttributeByteCount (10 bytes max)
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
    common::PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x05, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    EXPECT_LE(len, 0x11);  // 10 + 2 (byte count) + 5 (cont state)
    packet.Resize(len);
    Status st = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_FALSE(st);
      EXPECT_EQ(common::HostError::kInProgress, st.error());
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (!st && (st.error() != common::HostError::kInProgress)) {
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
      auto kContinuedRequestAttrStart = common::CreateStaticByteBuffer(
          0x04,  // SDP_ServiceAttributeRequest
          0x10, static_cast<uint8_t>(received + 1), UpperBits(param_size),
          LowerBits(param_size),       // Parameter length
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
      common::DynamicByteBuffer req(kContinuedRequestAttrStart.size() +
                                    sizeof(uint8_t) + cont_size);

      kContinuedRequestAttrStart.Copy(&req);
      req.Write(&cont_size, sizeof(uint8_t), kContinuedRequestAttrStart.size());
      req.Write(continuation,
                kContinuedRequestAttrStart.size() + sizeof(uint8_t));

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

  const auto kInvalidRangeOrder = common::CreateStaticByteBuffer(
      0x04,                        // SDP_ServiceAttritbuteRequest
      0xE0, 0x01,                  // Transaction ID (0xE001)
      0x00, 0x11,                  // Parameter length (17 bytes)
      UINT32_AS_BE_BYTES(handle),  // ServiceRecordHandle
      0x00, 0x0A,                  // MaximumAttributeByteCount (10 bytes max)
      // AttributeIDList
      0x35, 0x08,  // Sequence uint8 8 bytes
      0x09,        // uint16_t, single attribute
      0x00, 0x01,  // ServiceClassIDList
      0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
      0xf0, 0x00,  // low end of range
      0x30, 0x00,  // high end of range
      0x00         // Contunuation State: none
  );

  const auto kRspErrSyntax =
      SDP_ERROR_RSP(0xE001, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidRangeOrder, kRspErrSyntax));

  const auto kInvalidMaxBytes = common::CreateStaticByteBuffer(
      0x04,                        // SDP_ServiceAttritbuteRequest
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

  const auto kRspErrSyntax2 =
      SDP_ERROR_RSP(0xE002, ErrorCode::kInvalidRequestSyntax);

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
  record1.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                protocol::kL2CAP, DataElement(uint16_t(500)));
  record1.SetAttribute(0xf00d, DataElement(uint32_t(0xfeedbeef)));
  record1.SetAttribute(0xf000, DataElement(uint32_t(0x01234567)));

  ServiceHandle handle1 =
      server()->RegisterService(std::move(record1), NopConnectCallback);

  EXPECT_TRUE(handle1);

  ServiceRecord record2;
  record2.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList,
                                protocol::kL2CAP, DataElement(uint16_t(501)));
  ServiceHandle handle2 =
      server()->RegisterService(std::move(record2), NopConnectCallback);

  EXPECT_TRUE(handle2);

  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();

  const auto kRequestAttr = common::CreateStaticByteBuffer(
      0x06,        // SDP_ServiceAttritbuteRequest
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
    common::PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x07, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    EXPECT_LE(len, 0x11);  // 2 (byte count) + 10 (max len) + 5 (cont state)
    packet.Resize(len);
    Status st = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_FALSE(st);
      EXPECT_EQ(common::HostError::kInProgress, st.error());
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (!st && (st.error() != common::HostError::kInProgress)) {
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
      auto kContinuedRequestAttrStart = common::CreateStaticByteBuffer(
          0x06,  // SDP_ServiceAttributeRequest
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
      common::DynamicByteBuffer req(kContinuedRequestAttrStart.size() +
                                    sizeof(uint8_t) + cont_size);

      kContinuedRequestAttrStart.Copy(&req);
      req.Write(&cont_size, sizeof(uint8_t), kContinuedRequestAttrStart.size());
      req.Write(continuation,
                kContinuedRequestAttrStart.size() + sizeof(uint8_t));

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

  const auto kInvalidRangeOrder = common::CreateStaticByteBuffer(
      0x06,                          // SDP_ServiceAttritbuteRequest
      0xE0, 0x01,                    // Transaction ID (0xE001)
      0x00, 0x12,                    // Parameter length (18 bytes)
      0x35, 0x03, 0x19, 0x01, 0x00,  // SearchPattern: L2CAP
      0x00, 0x0A,                    // MaximumAttributeByteCount (10 bytes max)
      // AttributeIDList
      0x35, 0x08,  // Sequence uint8 8 bytes
      0x09,        // uint16_t, single attribute
      0x00, 0x01,  // ServiceClassIDList
      0x0A,        // uint32_t, which is a range (0x3000 - 0xf000)
      0xf0, 0x00,  // low end of range
      0x30, 0x00,  // high end of range
      0x00         // Contunuation State: none
  );

  const auto kRspErrSyntax =
      SDP_ERROR_RSP(0xE001, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidRangeOrder, kRspErrSyntax));

  const auto kInvalidMaxBytes = common::CreateStaticByteBuffer(
      0x04,                          // SDP_ServiceAttritbuteRequest
      0xE0, 0x02,                    // Transaction ID (0xE002)
      0x00, 0x0D,                    // Parameter length (13 bytes)
      0x35, 0x03, 0x19, 0x01, 0x00,  // SearchPattern: L2CAP
      0x00, 0x05,                    // MaximumAttributeByteCount (5 bytes max)
      // AttributeIDList
      0x35, 0x03,  // Sequence uint8 3 bytes
      0x09,        // uint16_t, single attribute
      0x00, 0x01,  // ServiceClassIDList
      0x00         // Contunuation State: none
  );

  const auto kRspErrSyntax2 =
      SDP_ERROR_RSP(0xE002, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidMaxBytes, kRspErrSyntax2));
}

TEST_F(SDP_ServerTest, ConnectionCallbacks) {
  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();

  std::vector<zx::socket> socks;
  hci::ConnectionHandle latest_handle;

  // Register a service
  AddA2DPSink([&socks, &latest_handle](zx::socket incoming_sock, auto handle,
                                       const auto& protocol) {
    bt_log(SPEW, "test", "Got socket for the a2dp sink");
    socks.emplace_back(std::move(incoming_sock));
    latest_handle = handle;
  });

  // Connect to the service
  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kAVDTP,
                                      kSdpChannel + 1, 0x0b00);
  RunLoopUntilIdle();

  // It should get a callback with a socket
  EXPECT_EQ(1u, socks.size());
  EXPECT_EQ(kTestHandle1, latest_handle);

  // Connect to the same service again with the same PSM (on a different
  // connection, it should still work)
  l2cap()->TriggerInboundL2capChannel(kTestHandle2, l2cap::kAVDTP,
                                      kSdpChannel + 2, 0x0b01);
  RunLoopUntilIdle();

  ASSERT_EQ(2u, socks.size());
  EXPECT_EQ(kTestHandle2, latest_handle);
  EXPECT_NE(socks.front(), socks.back());
}

// Browse Group gets set correctly
TEST_F(SDP_ServerTest, BrowseGroup) {
  AddA2DPSink();

  l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel,
                                      0x0bad);
  RunLoopUntilIdle();

  const auto kRequestAttr = common::CreateStaticByteBuffer(
      0x06,        // SDP_ServiceAttritbuteRequest
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
  auto send_cb = [this, &rsp](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    common::PacketView<sdp::Header> packet(cb_packet.get());
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
  EXPECT_EQ(kPublicBrowseRootUuid,
            *group_attr_it->second.At(0)->Get<common::UUID>());
}

#undef SDP_ERROR_RSP
#undef UINT32_AS_LE_BYTES

}  // namespace
}  // namespace sdp
}  // namespace bt
