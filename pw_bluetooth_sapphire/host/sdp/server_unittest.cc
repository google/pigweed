// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/sdp/server.h"

#include <lib/inspect/testing/cpp/inspect.h>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel_test.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/pdu.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"

namespace bt::sdp {

using RegistrationHandle = Server::RegistrationHandle;

namespace {

using namespace inspect::testing;

using TestingBase = l2cap::testing::FakeChannelTest;

constexpr hci_spec::ConnectionHandle kTestHandle1 = 1;
constexpr hci_spec::ConnectionHandle kTestHandle2 = 2;

void NopConnectCallback(fbl::RefPtr<l2cap::Channel>, const DataElement&) {}

constexpr l2cap::ChannelParameters kChannelParams;

class ServerTest : public TestingBase {
 public:
  ServerTest() = default;
  ~ServerTest() = default;

 protected:
  void SetUp() override {
    l2cap_ = std::make_unique<l2cap::testing::FakeL2cap>();
    l2cap_->set_channel_callback([this](auto fake_chan) {
      channel_ = std::move(fake_chan);
      set_fake_chan(channel_->AsWeakPtr());
    });
    l2cap_->AddACLConnection(kTestHandle1, hci_spec::ConnectionRole::kPeripheral, nullptr, nullptr);
    l2cap_->AddACLConnection(kTestHandle2, hci_spec::ConnectionRole::kPeripheral, nullptr, nullptr);
    server_ = std::make_unique<Server>(l2cap_.get());
  }

  void TearDown() override {
    channel_ = nullptr;
    server_ = nullptr;
    l2cap_ = nullptr;
  }

  Server* server() const { return server_.get(); }

  l2cap::testing::FakeL2cap* l2cap() const { return l2cap_.get(); }

  RegistrationHandle AddSPP(sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;

    record.SetServiceClassUUIDs({profile::kSerialPort});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement());
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kRFCOMM,
                                 DataElement(uint8_t{0}));
    record.AddProfile(profile::kSerialPort, 1, 2);
    record.AddInfo("en", "FAKE", "", "");
    std::vector<ServiceRecord> records;
    records.emplace_back(std::move(record));
    RegistrationHandle handle =
        server()->RegisterService(std::move(records), kChannelParams, std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

  RegistrationHandle AddA2DPSink(sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;
    record.SetServiceClassUUIDs({profile::kAudioSink});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement(l2cap::kAVDTP));
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kAVDTP,
                                 DataElement(uint16_t{0x0103}));  // Version
    record.AddProfile(profile::kAdvancedAudioDistribution, 1, 3);
    record.SetAttribute(kA2DP_SupportedFeatures, DataElement(uint16_t{0x0001}));  // Headphones
    std::vector<ServiceRecord> records;
    records.emplace_back(std::move(record));
    RegistrationHandle handle =
        server()->RegisterService(std::move(records), kChannelParams, std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

  RegistrationHandle AddL2capService(l2cap::PSM channel,
                                     l2cap::ChannelParameters chan_params = kChannelParams,
                                     sdp::Server::ConnectCallback cb = NopConnectCallback) {
    ServiceRecord record;
    record.SetServiceClassUUIDs({profile::kAudioSink});
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement(channel));
    record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kAVDTP,
                                 DataElement(uint16_t{0x0103}));  // Version
    record.AddProfile(profile::kAdvancedAudioDistribution, 1, 3);
    record.SetAttribute(kA2DP_SupportedFeatures, DataElement(uint16_t{0x0001}));  // Headphones
    std::vector<ServiceRecord> records;
    records.emplace_back(std::move(record));
    RegistrationHandle handle =
        server()->RegisterService(std::move(records), chan_params, std::move(cb));
    EXPECT_TRUE(handle);
    return handle;
  }

 private:
  fbl::RefPtr<l2cap::testing::FakeChannel> channel_;
  std::unique_ptr<l2cap::testing::FakeL2cap> l2cap_;
  std::unique_ptr<Server> server_;
};

constexpr l2cap::ChannelId kSdpChannel = 0x0041;

#define SDP_ERROR_RSP(t_id, code)                                                                 \
  StaticByteBuffer(0x01, UpperBits(t_id), LowerBits(t_id), 0x00, 0x02, UpperBits(uint16_t(code)), \
                   LowerBits(uint16_t(code)));

// Test:
//  - Accepts channels and holds channel open correctly.
//  - More than one channel from the same peer can be open at once.
//  - Packets that are the wrong length are responded to with kInvalidSize
//  - Answers with the same TransactionID as sent
TEST_F(ServerTest, BasicError) {
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();
  ASSERT_TRUE(fake_chan());
  EXPECT_TRUE(fake_chan()->activated());

  const auto kRspErrSize = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);

  const StaticByteBuffer kTooSmall(0x01,        // SDP_ServiceSearchRequest
                                   0x10, 0x01,  // Transaction ID (0x1001)
                                   0x00, 0x09   // Parameter length (9 bytes)
  );

  const auto kRspTooSmall = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);

  const StaticByteBuffer kTooBig(0x01,             // SDP_ServiceSearchRequest
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

  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel + 1, 0x0bad));
  RunLoopUntilIdle();
  ASSERT_TRUE(fake_chan());
  EXPECT_TRUE(fake_chan()->activated());

  EXPECT_TRUE(ReceiveAndExpect(kTooSmall, kRspTooSmall));
}

// Test:
//  - Passes an initialized ServiceRecord that has a matching ServiceHandle
//  - Doesn't add a service that doesn't contain a ServiceClassIDList
//  - Adds a service that is valid.
//  - Services can be Unregistered.
TEST_F(ServerTest, RegisterService) {
  std::vector<ServiceRecord> records;
  EXPECT_FALSE(server()->RegisterService(std::move(records), kChannelParams, {}));

  ServiceRecord record = ServiceRecord();
  std::vector<ServiceRecord> records0;
  records0.emplace_back(std::move(record));
  EXPECT_FALSE(server()->RegisterService(std::move(records0), kChannelParams, {}));

  ServiceRecord record1;
  record1.SetAttribute(kServiceClassIdList, DataElement(uint16_t{42}));
  std::vector<ServiceRecord> records1;
  records1.emplace_back(std::move(record1));
  EXPECT_FALSE(server()->RegisterService(std::move(records1), kChannelParams, {}));

  ServiceRecord has_handle;
  has_handle.SetHandle(42);
  std::vector<ServiceRecord> records2;
  records2.emplace_back(std::move(has_handle));
  EXPECT_FALSE(server()->RegisterService(std::move(records2), kChannelParams, {}));

  ServiceRecord valid;
  valid.SetServiceClassUUIDs({profile::kAVRemoteControl});
  std::vector<ServiceRecord> records3;
  records3.emplace_back(std::move(valid));
  RegistrationHandle handle = server()->RegisterService(std::move(records3), kChannelParams, {});

  EXPECT_TRUE(handle);

  EXPECT_TRUE(server()->UnregisterService(handle));
  EXPECT_FALSE(server()->UnregisterService(handle));
}

// Test:
// - Adds a primary protocol to the service defintion.
// - Adds multiple additional protocols to the service definition.
// - Tests registration and removal are successful.
// - Tests callback correctness when inbound l2cap channels are connected.
TEST_F(ServerTest, RegisterServiceWithAdditionalProtocol) {
  std::vector<l2cap::PSM> psms{500, 27, 29};

  ServiceRecord psm_additional;
  psm_additional.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_additional.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                       DataElement(uint16_t(psms[0])));
  psm_additional.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t(psms[1])));
  psm_additional.AddProtocolDescriptor(2, protocol::kL2CAP, DataElement(uint16_t(psms[2])));

  std::vector<uint16_t> protocols_discovered;
  auto cb = [&](auto /*channel*/, auto& protocol_list) {
    EXPECT_EQ(DataElement::Type::kSequence, protocol_list.type());
    auto* psm = protocol_list.At(0);
    EXPECT_EQ(DataElement::Type::kSequence, psm->type());
    psm = psm->At(1);
    EXPECT_EQ(DataElement::Type::kUnsignedInt, psm->type());
    protocols_discovered.emplace_back(*psm->template Get<uint16_t>());
  };

  std::vector<ServiceRecord> records;
  records.emplace_back(std::move(psm_additional));
  auto handle = server()->RegisterService(std::move(records), kChannelParams, std::move(cb));

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
TEST_F(ServerTest, RegisterServiceWithIncompleteAdditionalProtocol) {
  ServiceRecord psm_additional;
  psm_additional.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_additional.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                       DataElement(uint16_t{500}));
  psm_additional.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t{27}));
  psm_additional.AddProtocolDescriptor(2, protocol::kL2CAP, DataElement());

  size_t cb_count = 0;
  auto cb = [&](auto /*channel*/, auto& /* protocol_list */) { cb_count++; };

  std::vector<ServiceRecord> records;
  records.emplace_back(std::move(psm_additional));
  RegistrationHandle handle =
      server()->RegisterService(std::move(records), kChannelParams, std::move(cb));

  EXPECT_FALSE(handle);
  EXPECT_FALSE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 500, 0x40, 0x41));
  RunLoopUntilIdle();

  // Despite an incoming L2CAP connection, the callback should never be triggered since
  // no services should be registered.
  EXPECT_EQ(0u, cb_count);

  EXPECT_FALSE(server()->UnregisterService(handle));
}

TEST_F(ServerTest, PSMVerification) {
  ServiceRecord no_psm;
  no_psm.SetServiceClassUUIDs({profile::kAVRemoteControl});
  no_psm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                               DataElement());

  std::vector<ServiceRecord> records;
  records.emplace_back(std::move(no_psm));
  EXPECT_FALSE(server()->RegisterService(std::move(records), kChannelParams, {}));

  ServiceRecord psm_wrong_argtype;
  psm_wrong_argtype.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_wrong_argtype.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                          DataElement(bool(true)));

  std::vector<ServiceRecord> records2;
  records2.emplace_back(std::move(psm_wrong_argtype));
  EXPECT_FALSE(server()->RegisterService(std::move(records2), kChannelParams, {}));

  ServiceRecord psm_wrong_intsize;
  psm_wrong_intsize.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_wrong_intsize.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                          DataElement(uint8_t{5}));

  std::vector<ServiceRecord> records_wrong_intsize;
  records_wrong_intsize.emplace_back(std::move(psm_wrong_intsize));
  EXPECT_FALSE(server()->RegisterService(std::move(records_wrong_intsize), kChannelParams, {}));

  ServiceRecord psm_rfcomm;
  psm_rfcomm.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_rfcomm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                   DataElement());
  psm_rfcomm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kRFCOMM,
                                   DataElement(uint16_t{5}));

  std::vector<ServiceRecord> records3;
  records3.emplace_back(std::move(psm_rfcomm));
  EXPECT_TRUE(server()->RegisterService(std::move(records3), kChannelParams, NopConnectCallback));

  // Another RFCOMM should fail, even with a different channel.
  ServiceRecord psm_rfcomm2;
  psm_rfcomm2.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_rfcomm2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                    DataElement());
  psm_rfcomm2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kRFCOMM,
                                    DataElement(uint16_t{7}));

  std::vector<ServiceRecord> records4;
  records4.emplace_back(std::move(psm_rfcomm2));
  EXPECT_FALSE(server()->RegisterService(std::move(records4), kChannelParams, NopConnectCallback));

  ServiceRecord psm_ok;
  psm_ok.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_ok.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                               DataElement(uint16_t{500}));

  std::vector<ServiceRecord> records5;
  records5.emplace_back(std::move(psm_ok));
  auto handle = server()->RegisterService(std::move(records5), kChannelParams, NopConnectCallback);
  EXPECT_TRUE(handle);

  ServiceRecord psm_same;
  psm_same.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_same.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                 DataElement(uint16_t{500}));

  std::vector<ServiceRecord> records6;
  records6.emplace_back(std::move(psm_same));
  EXPECT_FALSE(server()->RegisterService(std::move(records6), kChannelParams, NopConnectCallback));

  // Unregistering allows us to re-register with PSM.
  server()->UnregisterService(handle);
  ServiceRecord psm_readd;
  psm_readd.SetServiceClassUUIDs({profile::kAVRemoteControl});
  psm_readd.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                  DataElement(uint16_t{500}));

  std::vector<ServiceRecord> records7;
  records7.emplace_back(std::move(psm_readd));
  EXPECT_TRUE(server()->RegisterService(std::move(records7), kChannelParams, NopConnectCallback));
}

// Test:
// - Registering multiple ServiceRecords from the same client is successful.
// - Inbound L2CAP connections on the registered PSMs trigger the callback.
TEST_F(ServerTest, RegisterServiceMultipleRecordsSuccess) {
  ServiceRecord record1;
  record1.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record1.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                DataElement(uint16_t{7}));
  record1.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t{8}));

  ServiceRecord record2;
  record2.SetServiceClassUUIDs({profile::kAudioSink});
  record2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                DataElement(uint16_t{9}));
  record2.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t{10}));

  std::vector<ServiceRecord> records;
  records.emplace_back(std::move(record1));
  records.emplace_back(std::move(record2));

  size_t cb_count = 0;
  auto cb = [&](auto /*channel*/, auto& protocol_list) { cb_count++; };

  auto handle = server()->RegisterService(std::move(records), kChannelParams, std::move(cb));
  EXPECT_TRUE(handle);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 7, 0x40, 0x41));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 8, 0x42, 0x43));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 9, 0x44, 0x44));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 10, 0x45, 0x46));
  RunLoopUntilIdle();

  EXPECT_EQ(4u, cb_count);

  EXPECT_TRUE(server()->UnregisterService(handle));
}

// Test:
// - Registering multiple ServiceRecords with the same PSM from the same client
// is successful.
// - Inbound L2CAP connections on the registered PSMs trigger the same callback.
// - Attempting to register a record with an already taken PSM will fail, and not
// register any of the other records in the set of records.
TEST_F(ServerTest, RegisterServiceMultipleRecordsSamePSM) {
  ServiceRecord target_browse_record;
  target_browse_record.SetServiceClassUUIDs({profile::kAVRemoteControlTarget});
  target_browse_record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                             DataElement(uint16_t{25}));
  target_browse_record.AddProtocolDescriptor(1, protocol::kL2CAP, DataElement(uint16_t{27}));

  ServiceRecord controller_record;
  controller_record.SetServiceClassUUIDs({profile::kAVRemoteControlController});
  controller_record.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                          DataElement(uint16_t{25}));

  std::vector<ServiceRecord> records;
  records.emplace_back(std::move(target_browse_record));
  records.emplace_back(std::move(controller_record));

  std::vector<uint16_t> protocols_discovered;
  auto cb = [&](auto /*channel*/, auto& protocol_list) {
    EXPECT_EQ(DataElement::Type::kSequence, protocol_list.type());
    auto* psm = protocol_list.At(0);
    EXPECT_EQ(DataElement::Type::kSequence, psm->type());
    psm = psm->At(1);
    EXPECT_EQ(DataElement::Type::kUnsignedInt, psm->type());
    protocols_discovered.emplace_back(*psm->template Get<uint16_t>());
  };

  auto handle = server()->RegisterService(std::move(records), kChannelParams, std::move(cb));
  EXPECT_TRUE(handle);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 25, 0x40, 0x41));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 27, 0x44, 0x44));
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, 25, 0x42, 0x43));
  RunLoopUntilIdle();

  // We expect two calls to the callback for psm=25, and one for psm=27.
  ASSERT_EQ(2u, std::count(protocols_discovered.begin(), protocols_discovered.end(), 25));
  ASSERT_EQ(1u, std::count(protocols_discovered.begin(), protocols_discovered.end(), 27));

  // Attempt to register existing PSM.
  ServiceRecord duplicate_psm;
  duplicate_psm.SetServiceClassUUIDs({profile::kAVRemoteControlTarget});
  duplicate_psm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                      DataElement(uint16_t{25}));

  ServiceRecord valid_psm;
  valid_psm.SetServiceClassUUIDs({profile::kAudioSource});
  valid_psm.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                  DataElement(uint16_t{31}));

  std::vector<ServiceRecord> invalid_records;
  invalid_records.emplace_back(std::move(duplicate_psm));
  invalid_records.emplace_back(std::move(valid_psm));
  EXPECT_FALSE(
      server()->RegisterService(std::move(invalid_records), kChannelParams, NopConnectCallback));

  EXPECT_TRUE(server()->UnregisterService(handle));
}

#define UINT32_AS_BE_BYTES(x) \
  UpperBits(x >> 16), LowerBits(x >> 16), UpperBits(x & 0xFFFF), LowerBits(x & 0xFFFF)

// Test ServiceSearchRequest:
//  - returns services with the UUID included
//  - doesn't return services that don't have the UUID
//  - fails when there are no items or too many items in the search
//  - doesn't return more than the max requested
TEST_F(ServerTest, ServiceSearchRequest) {
  RegistrationHandle spp_handle = AddSPP();
  RegistrationHandle a2dp_handle = AddA2DPSink();

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const StaticByteBuffer kL2capSearch(0x02,        // SDP_ServiceSearchRequest
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

  const StaticByteBuffer kL2capSearchResponse(
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
    bt_log(TRACE, "unittest", "resize packet to %d", len);
    packet.Resize(len);
    ServiceSearchResponse resp;
    auto status = resp.Parse(packet.payload_data());
    EXPECT_EQ(fitx::ok(), status);
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

  const StaticByteBuffer kInvalidNoItems(0x02,        // SDP_ServiceSearchRequest
                                         0x10, 0xA1,  // Transaction ID (0x10A1)
                                         0x00, 0x05,  // Parameter length (5 bytes)
                                         // ServiceSearchPattern
                                         0x35, 0x00,  // Sequence uint8 0 bytes
                                         0xFF, 0xFF,  // MaximumServiceRecordCount: (none)
                                         0x00         // Contunuation State: none
  );

  const auto kRspErrSyntax = SDP_ERROR_RSP(0x10A1, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidNoItems, kRspErrSyntax));

  const StaticByteBuffer kInvalidTooManyItems(
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
TEST_F(ServerTest, ServiceSearchRequestOneOfMany) {
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  RegistrationHandle spp_handle = AddSPP();
  RegistrationHandle a2dp_handle = AddA2DPSink();

  bool recv = false;
  std::vector<ServiceHandle> handles;
  TransactionId tid;
  auto cb = [&recv, &handles, &tid](auto cb_packet) {
    EXPECT_LE(sizeof(Header), cb_packet->size());
    PacketView<Header> packet(cb_packet.get());
    EXPECT_EQ(kServiceSearchResponse, packet.header().pdu_id);
    tid = betoh16(packet.header().tid);
    uint16_t len = betoh16(packet.header().param_length);
    bt_log(TRACE, "unittests", "resizing packet to %d", len);
    packet.Resize(len);
    ServiceSearchResponse resp;
    auto status = resp.Parse(packet.payload_data());
    EXPECT_EQ(fitx::ok(), status);
    handles = resp.service_record_handle_list();
    recv = true;
  };

  const StaticByteBuffer kL2capSearchOne(0x02,        // SDP_ServiceSearchRequest
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
  bool found_spp = std::find(handles.begin(), handles.end(), spp_handle) != handles.end();
  bool found_a2dp = std::find(handles.begin(), handles.end(), a2dp_handle) != handles.end();
  EXPECT_TRUE(found_spp || found_a2dp);
}

// Test ServiceSearchRequest:
//  - returns continuation state if too many services match
//  - continuation state in request works correctly
TEST_F(ServerTest, ServiceSearchContinuationState) {
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
    fitx::result<Error<>> result = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_EQ(ToResult(HostError::kInProgress), result);
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (result.is_error() && !result.error_value().is(HostError::kInProgress)) {
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
      StaticByteBuffer kContinuedRequestStart(0x02,        // SDP_ServiceSearchRequest
                                              0x10, 0xC1,  // Transaction ID (0x10C1)
                                              UpperBits(param_size),
                                              LowerBits(param_size),  // Parameter length
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

  const StaticByteBuffer kL2capSearch(0x02,        // SDP_ServiceSearchRequest
                                      0x10, 0xC1,  // Transaction ID (0x10C1)
                                      0x00, 0x08,  // Parameter length (8 bytes)
                                      // ServiceSearchPattern
                                      0x35, 0x03,        // Sequence uint8 3 bytes
                                      0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                      0x00, 0xFF,        // MaximumServiceRecordCount: 256
                                      0x00               // Contunuation State: none
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
TEST_F(ServerTest, ServiceAttributeRequest) {
  ServiceRecord record;
  record.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record.SetAttribute(0xf00d, DataElement(uint32_t{0xfeedbeef}));
  record.SetAttribute(0xf000, DataElement(uint32_t{0x01234567}));

  std::vector<ServiceRecord> records;
  records.emplace_back(std::move(record));
  RegistrationHandle handle = server()->RegisterService(std::move(records), kChannelParams, {});

  EXPECT_TRUE(handle);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kRequestAttr =
      StaticByteBuffer(0x04,                        // SDP_ServiceAttritbuteRequest
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
    PacketView<sdp::Header> packet(cb_packet.get());
    ASSERT_EQ(0x05, packet.header().pdu_id);
    uint16_t len = betoh16(packet.header().param_length);
    EXPECT_LE(len, 0x11);  // 10 + 2 (byte count) + 5 (cont state)
    packet.Resize(len);
    fitx::result<Error<>> result = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_EQ(ToResult(HostError::kInProgress), result);
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (result.is_error() && !result.error_value().is(HostError::kInProgress)) {
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
          StaticByteBuffer(0x04,        // SDP_ServiceAttributeRequest
                           0x10, 0x01,  // Transaction ID (reused)
                           UpperBits(param_size), LowerBits(param_size),  // Parameter length
                           UINT32_AS_BE_BYTES(handle),                    // ServiceRecordHandle
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
      StaticByteBuffer(0x04,                        // SDP_ServiceAttritbuteRequest
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

  const auto kRspErrSyntax = SDP_ERROR_RSP(0xE001, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidRangeOrder, kRspErrSyntax));

  const auto kInvalidMaxBytes =
      StaticByteBuffer(0x04,                        // SDP_ServiceAttritbuteRequest
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
TEST_F(ServerTest, SearchAttributeRequest) {
  ServiceRecord record1;
  record1.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record1.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                DataElement(uint16_t{500}));
  record1.SetAttribute(0xf00d, DataElement(uint32_t{0xfeedbeef}));
  record1.SetAttribute(0xf000, DataElement(uint32_t{0x01234567}));

  std::vector<ServiceRecord> records1;
  records1.emplace_back(std::move(record1));
  RegistrationHandle handle1 =
      server()->RegisterService(std::move(records1), kChannelParams, NopConnectCallback);

  EXPECT_TRUE(handle1);

  ServiceRecord record2;
  record2.SetServiceClassUUIDs({profile::kAVRemoteControl});
  record2.AddProtocolDescriptor(ServiceRecord::kPrimaryProtocolList, protocol::kL2CAP,
                                DataElement(uint16_t{501}));

  std::vector<ServiceRecord> records2;
  records2.emplace_back(std::move(record2));
  RegistrationHandle handle2 =
      server()->RegisterService(std::move(records2), kChannelParams, NopConnectCallback);

  EXPECT_TRUE(handle2);

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kRequestAttr =
      StaticByteBuffer(0x06,        // SDP_ServiceAttritbuteRequest
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
    fitx::result<Error<>> result = rsp.Parse(packet.payload_data());
    if (received == 0) {
      // Server should have split this into more than one response.
      EXPECT_EQ(ToResult(HostError::kInProgress), result);
      EXPECT_FALSE(rsp.complete());
    }
    received++;
    if (result.is_error() && !result.error_value().is(HostError::kInProgress)) {
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
          StaticByteBuffer(0x06,                                      // SDP_ServiceAttributeRequest
                           0x10, static_cast<uint8_t>(received + 1),  // Transaction ID
                           UpperBits(param_size), LowerBits(param_size),  // Parameter length
                           0x35, 0x03,                                    // Sequence uint8 3 bytes
                           0x19, 0x01, 0x00,                              // SearchPattern: L2CAP
                           0x00, 0x0A,  // MaximumAttributeByteCount (10 bytes max)
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
      StaticByteBuffer(0x06,                          // SDP_ServiceAttritbuteRequest
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

  const auto kRspErrSyntax = SDP_ERROR_RSP(0xE001, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidRangeOrder, kRspErrSyntax));

  const auto kInvalidMaxBytes =
      StaticByteBuffer(0x04,                          // SDP_ServiceAttritbuteRequest
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

  const auto kRspErrSyntax2 = SDP_ERROR_RSP(0xE002, ErrorCode::kInvalidRequestSyntax);

  EXPECT_TRUE(ReceiveAndExpect(kInvalidMaxBytes, kRspErrSyntax2));
}

TEST_F(ServerTest, ConnectionCallbacks) {
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  std::vector<fbl::RefPtr<l2cap::Channel>> channels;
  hci_spec::ConnectionHandle latest_handle;

  // Register a service
  AddA2DPSink([&channels, &latest_handle](auto chan, const auto& protocol) {
    bt_log(TRACE, "test", "Got channel for the a2dp sink");
    latest_handle = chan->link_handle();
    channels.emplace_back(std::move(chan));
  });

  // Connect to the service
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kAVDTP, kSdpChannel + 1, 0x0b00));
  RunLoopUntilIdle();

  // It should get a callback with a channel
  EXPECT_EQ(1u, channels.size());
  EXPECT_EQ(kTestHandle1, latest_handle);

  // Connect to the same service again with the same PSM (on a different
  // connection, it should still work)
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(kTestHandle2, l2cap::kAVDTP, kSdpChannel + 2, 0x0b01));
  RunLoopUntilIdle();

  ASSERT_EQ(2u, channels.size());
  EXPECT_EQ(kTestHandle2, latest_handle);
  EXPECT_NE(channels.front(), channels.back());

  // Connect to the service again, on the first connection.
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kAVDTP, kSdpChannel + 3, 0x0b00));
  RunLoopUntilIdle();

  // It should get a callback with a channel
  EXPECT_EQ(3u, channels.size());
  EXPECT_EQ(kTestHandle1, latest_handle);
}

// Browse Group gets set correctly
TEST_F(ServerTest, BrowseGroup) {
  AddA2DPSink();

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(kTestHandle1, l2cap::kSDP, kSdpChannel, 0x0bad));
  RunLoopUntilIdle();

  const auto kRequestAttr = StaticByteBuffer(0x06,        // SDP_ServiceAttritbuteRequest
                                             0x10, 0x01,  // Transaction ID (0x1001)
                                             0x00, 0x0D,  // Parameter length (12 bytes)
                                             // ServiceSearchPattern
                                             0x35, 0x03,        // Sequence uint8 3 bytes
                                             0x19, 0x01, 0x00,  // UUID: Protocol: L2CAP
                                             0xFF, 0xFF,  // MaximumAttributeByteCount (no max)
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
    EXPECT_EQ(fitx::ok(), status);
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
TEST_F(ServerTest, RegisterServiceWithChannelParameters) {
  l2cap::PSM kPSM = l2cap::kAVDTP;

  l2cap::ChannelParameters preferred_params;
  preferred_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  preferred_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  std::optional<l2cap::ChannelInfo> params;
  size_t chan_cb_count = 0;
  ASSERT_TRUE(AddL2capService(kPSM, preferred_params, [&](auto chan, auto& /*protocol*/) {
    chan_cb_count++;
    params = chan->info();
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
TEST_F(ServerTest, MakeServiceDiscoveryServiceIsValid) {
  auto sdp_def = server()->MakeServiceDiscoveryService();

  const DataElement& version_number_list = sdp_def.GetAttribute(kSDP_VersionNumberList);
  EXPECT_EQ(DataElement::Type::kSequence, version_number_list.type());

  auto* it = version_number_list.At(0);
  EXPECT_EQ(DataElement::Type::kUnsignedInt, it->type());
  EXPECT_EQ(0x0100u, it->Get<uint16_t>());
}

// Test:
// - The inspect hierarchy for the initial server is valid. It should
// only contain the registered PSM for SDP.
TEST_F(ServerTest, InspectHierarchy) {
  inspect::Inspector inspector;
  server()->AttachInspect(inspector.GetRoot());

  auto psm_matcher = AllOf(NodeMatches(
      AllOf(NameMatches("psm0x1"), PropertyList(UnorderedElementsAre(StringIs("psm", "SDP"))))));
  auto reg_psm_matcher = AllOf(NodeMatches(AllOf(NameMatches("registered_psms"))),
                               ChildrenMatch(UnorderedElementsAre(psm_matcher)));

  auto record_matcher = AllOf(
      NodeMatches(AllOf(
          NameMatches("record0x0"),
          PropertyList(UnorderedElementsAre(StringIs(
              "record",
              "Service Class Id List: Sequence { UUID(00001000-0000-1000-8000-00805f9b34fb) }"))))),
      ChildrenMatch(UnorderedElementsAre(reg_psm_matcher)));

  auto sdp_matcher = AllOf(NodeMatches(AllOf(NameMatches("sdp_server"))),
                           ChildrenMatch(UnorderedElementsAre(record_matcher)));

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy);
  EXPECT_THAT(hierarchy.take_value(), AllOf(NodeMatches(NameMatches("root")),
                                            ChildrenMatch(UnorderedElementsAre(sdp_matcher))));
}

// Test:
// - The inspect hierarchy is updated correctly after registering another service.
// - The inspect hierarchy is updated correctly after unregistering a service.
// Note: None of the matchers test the name of the node. This is because the ordering of
// the std::unordered_map of PSMs is not guaranteed. Asserting on the name of the node
// is not feasible due to the usage of inspect::UniqueName, which assigns a new name
// to a node in every call. The contents of the node are verified.
TEST_F(ServerTest, InspectHierarchyAfterUnregisterService) {
  inspect::Inspector inspector;
  server()->AttachInspect(inspector.GetRoot());

  auto handle = AddA2DPSink();

  auto sdp_psm_matcher =
      AllOf(NodeMatches(AllOf(PropertyList(UnorderedElementsAre(StringIs("psm", "SDP"))))));
  auto sdp_matcher = AllOf(NodeMatches(AllOf(NameMatches("registered_psms"))),
                           ChildrenMatch(UnorderedElementsAre(sdp_psm_matcher)));

  auto a2dp_psm_matcher =
      AllOf(NodeMatches(AllOf(PropertyList(UnorderedElementsAre(StringIs("psm", "AVDTP"))))));
  auto a2dp_matcher = AllOf(NodeMatches(AllOf(NameMatches("registered_psms"))),
                            ChildrenMatch(UnorderedElementsAre(a2dp_psm_matcher)));

  auto sdp_record_matcher = AllOf(
      NodeMatches(AllOf(PropertyList(UnorderedElementsAre(StringIs(
          "record",
          "Service Class Id List: Sequence { UUID(00001000-0000-1000-8000-00805f9b34fb) }"))))),
      ChildrenMatch(UnorderedElementsAre(sdp_matcher)));
  auto a2dp_record_matcher =
      AllOf(NodeMatches(AllOf(PropertyList(UnorderedElementsAre(StringIs(
                "record",
                "Profile Descriptor: Sequence { Sequence { "
                "UUID(0000110d-0000-1000-8000-00805f9b34fb) UnsignedInt:2(259) } }\nService Class "
                "Id List: Sequence { UUID(0000110b-0000-1000-8000-00805f9b34fb) }"))))),
            ChildrenMatch(UnorderedElementsAre(a2dp_matcher)));

  auto sdp_server_matcher =
      AllOf(NodeMatches(AllOf(NameMatches("sdp_server"))),
            ChildrenMatch(UnorderedElementsAre(sdp_record_matcher, a2dp_record_matcher)));

  // Hierarchy should contain ServiceRecords and PSMs for SDP and A2DP Sink.
  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy);
  EXPECT_THAT(hierarchy.take_value(),
              AllOf(NodeMatches(NameMatches("root")),
                    ChildrenMatch(UnorderedElementsAre(sdp_server_matcher))));

  // Unregister the A2DP Sink service.
  EXPECT_TRUE(server()->UnregisterService(handle));

  auto sdp_server_matcher2 = AllOf(NodeMatches(AllOf(NameMatches("sdp_server"))),
                                   ChildrenMatch(UnorderedElementsAre(sdp_record_matcher)));

  // The ServiceRecords and PSMs associated with A2DP Sink should be removed
  // after the service has been registered. Only SDP's data should still exist.
  auto hierarchy2 = inspect::ReadFromVmo(inspector.DuplicateVmo());
  ASSERT_TRUE(hierarchy2);
  EXPECT_THAT(hierarchy2.take_value(),
              AllOf(NodeMatches(NameMatches("root")),
                    ChildrenMatch(UnorderedElementsAre(sdp_server_matcher2))));
}

// Test:
// Server::HandleRequest() provides expected responses when called without
// a corresponding l2cap::channel for both successful requests and errors.
TEST_F(ServerTest, HandleRequestWithoutChannel) {
  const auto kRspErrSize = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);
  const StaticByteBuffer kTooSmall(0x01,        // SDP_ServiceSearchRequest
                                   0x10, 0x01,  // Transaction ID (0x1001)
                                   0x00, 0x09   // Parameter length (9 bytes)
  );
  const auto kRspTooSmall = SDP_ERROR_RSP(0x1001, ErrorCode::kInvalidSize);
  auto too_small_rsp = server()->HandleRequest(
      std::unique_ptr<ByteBuffer>(new StaticByteBuffer(kTooSmall)), l2cap::kDefaultMTU);
  EXPECT_TRUE(ContainersEqual(*too_small_rsp.value(), kRspTooSmall));

  RegistrationHandle spp_handle = AddSPP();
  RegistrationHandle a2dp_handle = AddA2DPSink();
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
  auto search_rsp = server()->HandleRequest(
      std::unique_ptr<ByteBuffer>(new StaticByteBuffer(kL2capSearch)), l2cap::kDefaultMTU);
  EXPECT_TRUE(ContainersEqual(*search_rsp.value(), kL2capSearchResponse));
}

#undef SDP_ERROR_RSP
#undef UINT32_AS_LE_BYTES

}  // namespace
}  // namespace bt::sdp
