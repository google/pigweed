// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/bredr_discovery_manager.h"

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace bt {
namespace gap {
namespace {

using bt::testing::CommandTransaction;

using TestingBase = bt::testing::FakeControllerTest<bt::testing::TestController>;

// clang-format off
#define COMMAND_COMPLETE_RSP(opcode)                                         \
  CreateStaticByteBuffer(hci::kCommandCompleteEventCode, 0x04, 0xF0, \
                                 LowerBits((opcode)), UpperBits((opcode)),   \
                                 hci::kSuccess)

#define COMMAND_STATUS_RSP(opcode, statuscode)                       \
  CreateStaticByteBuffer(hci::kCommandStatusEventCode, 0x04, \
                                 (statuscode), 0xF0,                 \
                                 LowerBits((opcode)), UpperBits((opcode)))


const auto kWriteInquiryActivity = CreateStaticByteBuffer(
    LowerBits(hci::kWriteInquiryScanActivity), UpperBits(hci::kWriteInquiryScanActivity),
    0x04, // Param total size
    LowerBits(kInquiryScanInterval), UpperBits(kInquiryScanInterval),
    LowerBits(kInquiryScanWindow), UpperBits(kInquiryScanWindow)
);

const auto kWriteInquiryActivityRsp = COMMAND_COMPLETE_RSP(hci::kWriteInquiryScanActivity);

const auto kWriteInquiryType = CreateStaticByteBuffer(
    LowerBits(hci::kWriteInquiryScanType), UpperBits(hci::kWriteInquiryScanType),
    0x01, // Param total size
    0x01 // Interlaced Inquiry Scan
);

const auto kWriteInquiryTypeRsp = COMMAND_COMPLETE_RSP(hci::kWriteInquiryScanType);
// clang-format on

class BrEdrDiscoveryManagerTest : public TestingBase {
 public:
  BrEdrDiscoveryManagerTest()
      : peer_cache_(inspector_.GetRoot().CreateChild(PeerCache::kInspectNodeName)) {}
  ~BrEdrDiscoveryManagerTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());

    NewDiscoveryManager(hci::InquiryMode::kStandard);
  }

  void TearDown() override {
    discovery_manager_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  void NewDiscoveryManager(hci::InquiryMode mode) {
    // We expect to set the Inquiry Scan and the Type when we start.
    test_device()->QueueCommandTransaction(
        CommandTransaction(kWriteInquiryActivity, {&kWriteInquiryActivityRsp}));
    test_device()->QueueCommandTransaction(
        CommandTransaction(kWriteInquiryType, {&kWriteInquiryTypeRsp}));

    discovery_manager_ =
        std::make_unique<BrEdrDiscoveryManager>(transport()->WeakPtr(), mode, &peer_cache_);

    RunLoopUntilIdle();
  }

  PeerCache* peer_cache() { return &peer_cache_; }

 protected:
  BrEdrDiscoveryManager* discovery_manager() const { return discovery_manager_.get(); }

 private:
  inspect::Inspector inspector_;
  PeerCache peer_cache_;
  std::unique_ptr<BrEdrDiscoveryManager> discovery_manager_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrDiscoveryManagerTest);
};

using GAP_BrEdrDiscoveryManagerTest = BrEdrDiscoveryManagerTest;

// clang-format off
const auto kInquiry = CreateStaticByteBuffer(
  LowerBits(hci::kInquiry), UpperBits(hci::kInquiry),
  0x05, // Paramreter total size
  0x33, 0x8B, 0x9E, // GIAC
  0x08, // kInquiryLengthDefault
  0x00 // Unlimited responses
);

const auto kWriteLocalNameRsp =
    COMMAND_STATUS_RSP(hci::kWriteLocalName, hci::StatusCode::kSuccess);

const auto kWriteLocalNameRspError =
    COMMAND_STATUS_RSP(hci::kWriteLocalName, hci::StatusCode::kHardwareFailure);

const auto kWriteExtendedInquiryResponseRsp =
    COMMAND_STATUS_RSP(hci::kWriteExtendedInquiryResponse, hci::StatusCode::kSuccess);

const auto kWriteExtendedInquiryResponseRspError =
    COMMAND_STATUS_RSP(hci::kWriteExtendedInquiryResponse, hci::StatusCode::kHardwareFailure);

const auto kInquiryRsp = COMMAND_STATUS_RSP(hci::kInquiry, hci::kSuccess);

const auto kInquiryRspError = COMMAND_STATUS_RSP(hci::kInquiry, hci::kHardwareFailure);

const auto kInquiryComplete = CreateStaticByteBuffer(
  hci::kInquiryCompleteEventCode,
  0x01, // parameter_total_size (1 bytes)
  hci::kSuccess
);

const auto kInquiryCompleteError = CreateStaticByteBuffer(
  hci::kInquiryCompleteEventCode,
  0x01, // parameter_total_size (1 bytes)
  hci::kHardwareFailure
);

#define BD_ADDR(addr1) addr1, 0x00, 0x00, 0x00, 0x00, 0x00

const DeviceAddress kDeviceAddress1(DeviceAddress::Type::kBREDR,
                                    {BD_ADDR(0x01)});
const DeviceAddress kLeAliasAddress1(DeviceAddress::Type::kLEPublic,
                                     kDeviceAddress1.value());
const DeviceAddress kDeviceAddress2(DeviceAddress::Type::kBREDR,
                                    {BD_ADDR(0x02)});
const DeviceAddress kLeAliasAddress2(DeviceAddress::Type::kLEPublic,
                                     kDeviceAddress2.value());
const DeviceAddress kDeviceAddress3(DeviceAddress::Type::kBREDR,
                                    {BD_ADDR(0x03)});

const auto kInquiryResult = CreateStaticByteBuffer(
  hci::kInquiryResultEventCode,
  0x0F, // parameter_total_size (15 bytes)
  0x01, // num_responses
  BD_ADDR(0x01), // bd_addr[0]
  0x00, // page_scan_repetition_mode[0] (R0)
  0x00, // unused / reserved
  0x00, // unused / reserved
  0x00, 0x1F, 0x00, // class_of_device[0] (unspecified)
  0x00, 0x00 // clock_offset[0]
);

const auto kRSSIInquiryResult = CreateStaticByteBuffer(
  hci::kInquiryResultWithRSSIEventCode,
  0x0F, // parameter_total_size (15 bytes)
  0x01, // num_responses
  BD_ADDR(0x02), // bd_addr[0]
  0x00, // page_scan_repetition_mode[0] (R0)
  0x00, // unused / reserved
  0x00, 0x1F, 0x00, // class_of_device[0] (unspecified)
  0x00, 0x00, // clock_offset[0]
  0xEC // RSSI (-20dBm)
);

#define REMOTE_NAME_REQUEST(addr1) CreateStaticByteBuffer( \
    LowerBits(hci::kRemoteNameRequest), UpperBits(hci::kRemoteNameRequest), \
    0x0a, /* parameter_total_size (10 bytes) */ \
    BD_ADDR(addr1),  /* BD_ADDR */ \
    0x00, 0x00, 0x00, 0x80 /* page_scan_repetition_mode, 0, clock_offset */ \
);

const auto kRemoteNameRequest1 = REMOTE_NAME_REQUEST(0x01);
const auto kRemoteNameRequest2 = REMOTE_NAME_REQUEST(0x02);
const auto kRemoteNameRequest3 = REMOTE_NAME_REQUEST(0x03);

#undef REMOTE_NAME_REQUEST

const auto kRemoteNameRequestRsp =
    COMMAND_STATUS_RSP(hci::kRemoteNameRequest, hci::StatusCode::kSuccess);

#undef COMMAND_STATUS_RSP

const auto kRemoteNameRequestComplete1 = CreateStaticByteBuffer(
    hci::kRemoteNameRequestCompleteEventCode,
    0x20,                                // parameter_total_size (32)
    hci::StatusCode::kSuccess,           // status
    BD_ADDR(0x01),                       // BD_ADDR (00:00:00:00:00:01)
    'F', 'u', 'c', 'h', 's', 'i', 'a', 0xF0, 0x9F, 0x92, 0x96, 0x00, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
    // remote name (Fuchsia 💖)
    // Everything after the 0x00 should be ignored.
);
const auto kRemoteNameRequestComplete2 = CreateStaticByteBuffer(
    hci::kRemoteNameRequestCompleteEventCode,
    0x10,                                // parameter_total_size (16)
    hci::StatusCode::kSuccess,           // status
    BD_ADDR(0x02),                       // BD_ADDR (00:00:00:00:00:02)
    'S', 'a', 'p', 'p', 'h', 'i', 'r', 'e', 0x00 // remote name (Sapphire)
);

const auto kExtendedInquiryResult = CreateStaticByteBuffer(
  hci::kExtendedInquiryResultEventCode,
  0xFF, // parameter_total_size (255 bytes)
  0x01, // num_responses
  BD_ADDR(0x03),  // bd_addr
  0x00, // page_scan_repetition_mode (R0)
  0x00, // unused / reserved
  0x00, 0x1F, 0x00, // class_of_device (unspecified)
  0x00, 0x00, // clock_offset
  0xEC, // RSSI (-20dBm)
  // Extended Inquiry Response (240 bytes total)
  // Complete Local Name (12 bytes): Fuchsia 💖
  0x0C, 0x09, 'F', 'u', 'c', 'h', 's', 'i', 'a', 0xF0, 0x9F, 0x92, 0x96,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
);

#undef BD_ADDR

const auto kInqCancel = CreateStaticByteBuffer(
  LowerBits(hci::kInquiryCancel), UpperBits(hci::kInquiryCancel),  // opcode
  0x00                                   // parameter_total_size
);

const auto kInqCancelRsp = COMMAND_COMPLETE_RSP(hci::kInquiryCancel);

const auto kSetExtendedMode = CreateStaticByteBuffer(
    LowerBits(hci::kWriteInquiryMode), UpperBits(hci::kWriteInquiryMode),
    0x01, // parameter_total_size
    0x02 // Extended Inquiry Result or Inquiry Result with RSSI
);

const auto kSetExtendedModeRsp = COMMAND_COMPLETE_RSP(hci::kWriteInquiryMode);

const auto kReadScanEnable = CreateStaticByteBuffer(
    LowerBits(hci::kReadScanEnable), UpperBits(hci::kReadScanEnable),
    0x00  // No parameters
);

const auto kWriteLocalName = CreateStaticByteBuffer(
  LowerBits(hci::kWriteLocalName), UpperBits(hci::kWriteLocalName),
  0xF8, // parameter_total_size (248 bytes)
  // Complete Local Name ()
  'A', 'B', 'C', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00
);

const auto kWriteExtendedInquiryResponse = CreateStaticByteBuffer(
  LowerBits(hci::kWriteExtendedInquiryResponse),
  UpperBits(hci::kWriteExtendedInquiryResponse),
  0xF1, // parameter_total_size (241 bytes)
  0x00, // fec_required
  0x04, // name_length + 1
  0x09, // DataType::kCompleteLocalName,
  // Complete Local Name (3 bytes + 1 byte null terminator + 234 bytes of zero padding)
  'A', 'B', 'C', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
);

#define READ_SCAN_ENABLE_RSP(scan_enable)                                    \
  CreateStaticByteBuffer(hci::kCommandCompleteEventCode, 0x05, 0xF0, \
                                 LowerBits(hci::kReadScanEnable),            \
                                 UpperBits(hci::kReadScanEnable),            \
                                 hci::kSuccess, (scan_enable))

const auto kReadScanEnableRspNone = READ_SCAN_ENABLE_RSP(0x00);
const auto kReadScanEnableRspInquiry = READ_SCAN_ENABLE_RSP(0x01);
const auto kReadScanEnableRspPage = READ_SCAN_ENABLE_RSP(0x02);
const auto kReadScanEnableRspBoth = READ_SCAN_ENABLE_RSP(0x03);

#undef READ_SCAN_ENABLE_RSP

#define WRITE_SCAN_ENABLE_CMD(scan_enable)                               \
  CreateStaticByteBuffer(LowerBits(hci::kWriteScanEnable),       \
                                 UpperBits(hci::kWriteScanEnable), 0x01, \
                                 (scan_enable))

const auto kWriteScanEnableNone = WRITE_SCAN_ENABLE_CMD(0x00);
const auto kWriteScanEnableInq = WRITE_SCAN_ENABLE_CMD(0x01);
const auto kWriteScanEnablePage = WRITE_SCAN_ENABLE_CMD(0x02);
const auto kWriteScanEnableBoth = WRITE_SCAN_ENABLE_CMD(0x03);

#undef WRITE_SCAN_ENABLE_CMD

const auto kWriteScanEnableRsp = COMMAND_COMPLETE_RSP(hci::kWriteScanEnable);

#undef COMMAND_COMPLETE_RSP
// clang-format on

// Test: discovering() answers correctly

// Test: requesting discovery should start inquiry
// Test: Inquiry Results that come in when there is discovery get reported up
// correctly to the sessions
// Test: Peers discovered are reported to the cache
// Test: Inquiry Results that come in when there's no discovery happening get
// discarded.
TEST_F(GAP_BrEdrDiscoveryManagerTest, RequestDiscoveryAndDrop) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest1, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete1}));

  std::unique_ptr<BrEdrDiscoverySession> session;
  size_t peers_found = 0u;

  discovery_manager()->RequestDiscovery([&session, &peers_found](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found](const auto&) { peers_found++; });
    session = std::move(cb_session);
  });

  EXPECT_FALSE(discovery_manager()->discovering());

  RunLoopUntilIdle();

  EXPECT_EQ(1u, peers_found);
  EXPECT_TRUE(discovery_manager()->discovering());

  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));

  test_device()->SendCommandChannelPacket(kInquiryComplete);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, peers_found);

  // TODO(jamuraa, NET-619): test InquiryCancel when it is implemented

  session = nullptr;
  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, peers_found);
  EXPECT_FALSE(discovery_manager()->discovering());

  test_device()->SendCommandChannelPacket(kInquiryComplete);
}

// Test: requesting a second discovery should start a session without sending
// any more HCI commands.
// Test: dropping the first discovery shouldn't stop inquiry
// Test: starting two sessions at once should only start inquiry once
TEST_F(GAP_BrEdrDiscoveryManagerTest, MultipleRequests) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest1, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete1}));

  std::unique_ptr<BrEdrDiscoverySession> session1;
  size_t peers_found1 = 0u;

  discovery_manager()->RequestDiscovery([&session1, &peers_found1](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found1](const auto&) { peers_found1++; });
    session1 = std::move(cb_session);
  });

  EXPECT_FALSE(discovery_manager()->discovering());

  RunLoopUntilIdle();

  EXPECT_TRUE(session1);
  EXPECT_EQ(1u, peers_found1);
  EXPECT_TRUE(discovery_manager()->discovering());

  std::unique_ptr<BrEdrDiscoverySession> session2;
  size_t peers_found2 = 0u;

  discovery_manager()->RequestDiscovery([&session2, &peers_found2](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found2](const auto&) { peers_found2++; });
    session2 = std::move(cb_session);
  });

  RunLoopUntilIdle();

  EXPECT_TRUE(session2);
  EXPECT_EQ(1u, peers_found1);
  EXPECT_EQ(0u, peers_found2);
  EXPECT_TRUE(discovery_manager()->discovering());

  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, peers_found1);
  EXPECT_EQ(1u, peers_found2);

  session1 = nullptr;

  RunLoopUntilIdle();

  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, peers_found1);
  EXPECT_EQ(2u, peers_found2);

  // TODO(jamuraa, NET-619): test InquiryCancel when it is implemented

  session2 = nullptr;

  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, peers_found1);
  EXPECT_EQ(2u, peers_found2);

  EXPECT_FALSE(discovery_manager()->discovering());

  test_device()->SendCommandChannelPacket(kInquiryComplete);

  RunLoopUntilIdle();
}

// Test: starting a session "while" the other one is stopping a session should
// still restart the Inquiry.
// Test: starting a session "while" the other one is stopping should return
// without needing an InquiryComplete first.
// Test: we should only request a peer's name if it's the first time we
// encounter it.
TEST_F(GAP_BrEdrDiscoveryManagerTest, RequestDiscoveryWhileStop) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest1, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete1}));

  std::unique_ptr<BrEdrDiscoverySession> session1;
  size_t peers_found1 = 0u;

  discovery_manager()->RequestDiscovery([&session1, &peers_found1](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found1](const auto&) { peers_found1++; });
    session1 = std::move(cb_session);
  });

  EXPECT_FALSE(discovery_manager()->discovering());

  RunLoopUntilIdle();

  EXPECT_TRUE(session1);
  EXPECT_EQ(1u, peers_found1);
  EXPECT_TRUE(discovery_manager()->discovering());

  // Drop the active session.
  session1 = nullptr;
  RunLoopUntilIdle();

  std::unique_ptr<BrEdrDiscoverySession> session2;
  size_t peers_found2 = 0u;
  discovery_manager()->RequestDiscovery([&session2, &peers_found2](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found2](const auto&) { peers_found2++; });
    session2 = std::move(cb_session);
  });

  // The new session should be started at this point, and inquiry results
  // returned.
  EXPECT_TRUE(session2);
  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, peers_found2);

  // Inquiry should be restarted when the Complete comes in because an active
  // session2 still exists.
  // TODO(jamuraa, NET-619): test InquiryCancel when it is implemented
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));
  test_device()->SendCommandChannelPacket(kInquiryComplete);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, peers_found1);
  EXPECT_EQ(2u, peers_found2);
  EXPECT_TRUE(discovery_manager()->discovering());

  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, peers_found1);
  EXPECT_EQ(3u, peers_found2);

  // TODO(jamuraa, NET-619): test InquiryCancel when it is implemented
  session2 = nullptr;

  // After the session is dropped, even if another result comes in, no results
  // are sent to the callback.
  test_device()->SendCommandChannelPacket(kInquiryResult);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, peers_found1);
  EXPECT_EQ(3u, peers_found2);
}

// Test: When Inquiry Fails to start, we report this back to the requester.
TEST_F(GAP_BrEdrDiscoveryManagerTest, RequestDiscoveryError) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRspError, &kInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest1, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete1}));

  std::unique_ptr<BrEdrDiscoverySession> session;

  discovery_manager()->RequestDiscovery([](auto status, auto cb_session) {
    EXPECT_FALSE(status);
    EXPECT_FALSE(cb_session);
    EXPECT_EQ(HostError::kProtocolError, status.error());
    EXPECT_EQ(hci::kHardwareFailure, status.protocol_error());
  });

  EXPECT_FALSE(discovery_manager()->discovering());

  RunLoopUntilIdle();

  EXPECT_FALSE(discovery_manager()->discovering());
}

// Test: When inquiry complete indicates failure, we signal to the current
// sessions.
TEST_F(GAP_BrEdrDiscoveryManagerTest, ContinuingDiscoveryError) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest1, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete1}));

  std::unique_ptr<BrEdrDiscoverySession> session;
  size_t peers_found = 0u;
  bool error_callback = false;

  discovery_manager()->RequestDiscovery(
      [&session, &peers_found, &error_callback](auto status, auto cb_session) {
        EXPECT_TRUE(status);
        cb_session->set_result_callback([&peers_found](const auto&) { peers_found++; });
        cb_session->set_error_callback([&error_callback]() { error_callback = true; });
        session = std::move(cb_session);
      });

  EXPECT_FALSE(discovery_manager()->discovering());

  RunLoopUntilIdle();

  EXPECT_EQ(1u, peers_found);
  EXPECT_TRUE(discovery_manager()->discovering());

  test_device()->SendCommandChannelPacket(kInquiryCompleteError);

  RunLoopUntilIdle();

  EXPECT_TRUE(error_callback);
  EXPECT_FALSE(discovery_manager()->discovering());

  session = nullptr;

  RunLoopUntilIdle();
}

// Test: UpdateLocalName successfully sends hci command, and further calls
// UpdateEIRResponseData (private). Ensures the name is updated at the very end.
TEST_F(GAP_BrEdrDiscoveryManagerTest, UpdateLocalNameSuccess) {
  test_device()->QueueCommandTransaction(CommandTransaction(kWriteLocalName, {}));

  // Set the status to be a dummy invalid status.
  hci::Status result = hci::Status(hci::kPairingNotAllowed);
  size_t callback_count = 0u;
  auto name_cb = [&result, &callback_count](const auto& status) {
    EXPECT_TRUE(status);
    callback_count++;
    result = status;
  };
  const std::string kNewName = "ABC";
  discovery_manager()->UpdateLocalName(kNewName, name_cb);

  RunLoopUntilIdle();

  // Local name should not be set, callback shouldn't be called yet.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(0u, callback_count);

  test_device()->SendCommandChannelPacket(kWriteLocalNameRsp);
  test_device()->QueueCommandTransaction(CommandTransaction(kWriteExtendedInquiryResponse, {}));

  RunLoopUntilIdle();

  // Still waiting on EIR response.
  // Local name should not be set, callback shouldn't be called yet.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(0u, callback_count);

  test_device()->SendCommandChannelPacket(kWriteExtendedInquiryResponseRsp);

  RunLoopUntilIdle();

  EXPECT_EQ(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(hci::Status(hci::kSuccess), result);
  EXPECT_EQ(1u, callback_count);
}

// Test: UpdateLocalName passes back error code through the callback and |local_name_|
// does not get updated.
TEST_F(GAP_BrEdrDiscoveryManagerTest, UpdateLocalNameError) {
  test_device()->QueueCommandTransaction(CommandTransaction(kWriteLocalName, {}));

  // Set the status to be a dummy invalid status.
  hci::Status result = hci::Status(hci::kUnsupportedRemoteFeature);
  size_t callback_count = 0u;
  auto name_cb = [&result, &callback_count](const auto& status) {
    EXPECT_FALSE(status);
    callback_count++;
    result = status;
  };
  const std::string kNewName = "ABC";
  discovery_manager()->UpdateLocalName(kNewName, name_cb);

  RunLoopUntilIdle();

  // Local name should not be set, callback shouldn't be called yet.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(0u, callback_count);

  // Send a response error.
  test_device()->SendCommandChannelPacket(kWriteLocalNameRspError);

  RunLoopUntilIdle();

  // |local_name_| should not be updated, return status should be error.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(hci::Status(hci::kHardwareFailure), result);
  EXPECT_EQ(1u, callback_count);
}

// Test: UpdateLocalName should succeed, but UpdateEIRResponseData should fail.
// Consequently, the |local_name_| should not be updated, and the callback should
// return the error.
TEST_F(GAP_BrEdrDiscoveryManagerTest, UpdateEIRResponseDataError) {
  test_device()->QueueCommandTransaction(CommandTransaction(kWriteLocalName, {}));

  // Set the status to be a dummy invalid status.
  hci::Status result = hci::Status(hci::kUnsupportedRemoteFeature);
  size_t callback_count = 0u;
  auto name_cb = [&result, &callback_count](const auto& status) {
    EXPECT_FALSE(status);
    callback_count++;
    result = status;
  };
  const std::string kNewName = "ABC";
  discovery_manager()->UpdateLocalName(kNewName, name_cb);

  RunLoopUntilIdle();

  // Local name should not be set, callback shouldn't be called yet.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(0u, callback_count);

  // kWriteLocalName should succeed.
  test_device()->SendCommandChannelPacket(kWriteLocalNameRsp);
  test_device()->QueueCommandTransaction(CommandTransaction(kWriteExtendedInquiryResponse, {}));

  RunLoopUntilIdle();

  // Still waiting on EIR response.
  // Local name should not be set, callback shouldn't be called yet.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(0u, callback_count);

  // kWriteExtendedInquiryResponse should fail.
  test_device()->SendCommandChannelPacket(kWriteExtendedInquiryResponseRspError);

  RunLoopUntilIdle();

  // |local_name_| should not be updated, return status should be error.
  EXPECT_NE(kNewName, discovery_manager()->local_name());
  EXPECT_EQ(hci::Status(hci::kHardwareFailure), result);
  EXPECT_EQ(1u, callback_count);
}

// Test: requesting discoverable works
// Test: requesting discoverable while discoverable is pending doesn't send
// any more HCI commands
TEST_F(GAP_BrEdrDiscoveryManagerTest, DiscoverableSet) {
  test_device()->QueueCommandTransaction(CommandTransaction(kReadScanEnable, {}));

  std::vector<std::unique_ptr<BrEdrDiscoverableSession>> sessions;
  auto session_cb = [&sessions](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    sessions.emplace_back(std::move(cb_session));
  };

  discovery_manager()->RequestDiscoverable(session_cb);

  RunLoopUntilIdle();

  EXPECT_EQ(0u, sessions.size());
  EXPECT_FALSE(discovery_manager()->discoverable());

  test_device()->QueueCommandTransaction(CommandTransaction(kWriteScanEnableInq, {}));

  test_device()->SendCommandChannelPacket(kReadScanEnableRspNone);

  RunLoopUntilIdle();

  // Request another session while the first is pending.
  discovery_manager()->RequestDiscoverable(session_cb);

  test_device()->SendCommandChannelPacket(kWriteScanEnableRsp);

  RunLoopUntilIdle();

  EXPECT_EQ(2u, sessions.size());
  EXPECT_TRUE(discovery_manager()->discoverable());

  discovery_manager()->RequestDiscoverable(session_cb);

  EXPECT_EQ(3u, sessions.size());
  EXPECT_TRUE(discovery_manager()->discoverable());

  test_device()->QueueCommandTransaction(
      CommandTransaction(kReadScanEnable, {&kReadScanEnableRspInquiry}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(kWriteScanEnableNone, {&kWriteScanEnableRsp}));

  sessions.clear();

  RunLoopUntilIdle();

  EXPECT_FALSE(discovery_manager()->discoverable());
}

// Test: requesting discoverable while discovery is disabling leaves
// the discoverable enabled and reports success
// Test: enable/disable while page scan is enabled works.
TEST_F(GAP_BrEdrDiscoveryManagerTest, DiscoverableRequestWhileStopping) {
  test_device()->QueueCommandTransaction(
      CommandTransaction(kReadScanEnable, {&kReadScanEnableRspPage}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(kWriteScanEnableBoth, {&kWriteScanEnableRsp}));

  std::vector<std::unique_ptr<BrEdrDiscoverableSession>> sessions;
  auto session_cb = [&sessions](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    sessions.emplace_back(std::move(cb_session));
  };

  discovery_manager()->RequestDiscoverable(session_cb);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, sessions.size());
  EXPECT_TRUE(discovery_manager()->discoverable());

  test_device()->QueueCommandTransaction(CommandTransaction(kReadScanEnable, {}));

  sessions.clear();

  RunLoopUntilIdle();

  // Request a new discovery before the procedure finishes.
  // This will queue another ReadScanEnable just in case the disable write is
  // in progress.
  test_device()->QueueCommandTransaction(CommandTransaction(kReadScanEnable, {}));
  discovery_manager()->RequestDiscoverable(session_cb);

  test_device()->SendCommandChannelPacket(kReadScanEnableRspBoth);

  // This shouldn't send any WriteScanEnable because we're already in the right
  // mode (TestController will assert if we do as it's not expecting)
  RunLoopUntilIdle();

  EXPECT_EQ(1u, sessions.size());
  EXPECT_TRUE(discovery_manager()->discoverable());

  // If somehow the scan got turned off, we will still turn it back on.
  test_device()->QueueCommandTransaction(
      CommandTransaction(kWriteScanEnableBoth, {&kWriteScanEnableRsp}));
  test_device()->SendCommandChannelPacket(kReadScanEnableRspPage);

  RunLoopUntilIdle();

  EXPECT_EQ(1u, sessions.size());
  EXPECT_TRUE(discovery_manager()->discoverable());

  test_device()->QueueCommandTransaction(
      CommandTransaction(kReadScanEnable, {&kReadScanEnableRspBoth}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(kWriteScanEnablePage, {&kWriteScanEnableRsp}));

  sessions.clear();

  RunLoopUntilIdle();

  EXPECT_FALSE(discovery_manager()->discoverable());
}

// Test: non-standard inquiry modes mean before the first discovery, the
// inquiry mode is set.
// Test: extended inquiry is stored in the remote peer
TEST_F(GAP_BrEdrDiscoveryManagerTest, ExtendedInquiry) {
  NewDiscoveryManager(hci::InquiryMode::kExtended);

  test_device()->QueueCommandTransaction(
      CommandTransaction(kSetExtendedMode, {&kSetExtendedModeRsp}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kExtendedInquiryResult, &kRSSIInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest2, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete2}));

  std::unique_ptr<BrEdrDiscoverySession> session1;
  size_t peers_found1 = 0u;

  discovery_manager()->RequestDiscovery([&session1, &peers_found1](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found1](const auto&) { peers_found1++; });
    session1 = std::move(cb_session);
  });

  EXPECT_FALSE(discovery_manager()->discovering());

  RunLoopUntilIdle();

  EXPECT_TRUE(session1);
  EXPECT_EQ(2u, peers_found1);
  EXPECT_TRUE(discovery_manager()->discovering());
  session1 = nullptr;

  Peer* peer1 = peer_cache()->FindByAddress(kDeviceAddress2);
  ASSERT_TRUE(peer1);
  EXPECT_EQ(-20, peer1->rssi());

  Peer* peer2 = peer_cache()->FindByAddress(kDeviceAddress3);
  ASSERT_TRUE(peer2);
  ASSERT_TRUE(peer2->name());
  EXPECT_EQ("Fuchsia💖", *peer2->name());

  test_device()->SendCommandChannelPacket(kInquiryComplete);

  RunLoopUntilIdle();

  EXPECT_FALSE(discovery_manager()->discovering());
}

TEST_F(GAP_BrEdrDiscoveryManagerTest, InquiryResultUpgradesKnownLowEnergyPeerToDualMode) {
  Peer* peer = peer_cache()->NewPeer(kLeAliasAddress1, true);
  ASSERT_TRUE(peer);
  ASSERT_EQ(TechnologyType::kLowEnergy, peer->technology());

  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest1, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete1}));

  std::unique_ptr<BrEdrDiscoverySession> session;
  size_t peers_found = 0u;

  discovery_manager()->RequestDiscovery([&session, &peers_found](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found](auto&) { peers_found++; });
    session = std::move(cb_session);
  });
  RunLoopUntilIdle();
  session = nullptr;

  EXPECT_EQ(1u, peers_found);
  ASSERT_EQ(peer, peer_cache()->FindByAddress(kDeviceAddress1));
  EXPECT_EQ(TechnologyType::kDualMode, peer->technology());

  test_device()->SendCommandChannelPacket(kInquiryComplete);

  RunLoopUntilIdle();
}

TEST_F(GAP_BrEdrDiscoveryManagerTest, ExtendedInquiryResultUpgradesKnownLowEnergyPeerToDualMode) {
  Peer* peer = peer_cache()->NewPeer(kLeAliasAddress2, true);
  ASSERT_TRUE(peer);
  ASSERT_EQ(TechnologyType::kLowEnergy, peer->technology());

  NewDiscoveryManager(hci::InquiryMode::kExtended);

  test_device()->QueueCommandTransaction(
      CommandTransaction(kSetExtendedMode, {&kSetExtendedModeRsp}));
  test_device()->QueueCommandTransaction(
      CommandTransaction(kInquiry, {&kInquiryRsp, &kExtendedInquiryResult, &kRSSIInquiryResult}));
  test_device()->QueueCommandTransaction(CommandTransaction(
      kRemoteNameRequest2, {&kRemoteNameRequestRsp, &kRemoteNameRequestComplete2}));

  std::unique_ptr<BrEdrDiscoverySession> session;
  size_t peers_found = 0u;

  discovery_manager()->RequestDiscovery([&session, &peers_found](auto status, auto cb_session) {
    EXPECT_TRUE(status);
    cb_session->set_result_callback([&peers_found](auto&) { peers_found++; });
    session = std::move(cb_session);
  });
  RunLoopUntilIdle();
  session = nullptr;

  EXPECT_EQ(2u, peers_found);
  ASSERT_EQ(peer, peer_cache()->FindByAddress(kDeviceAddress2));
  EXPECT_EQ(TechnologyType::kDualMode, peer->technology());

  test_device()->SendCommandChannelPacket(kInquiryComplete);

  RunLoopUntilIdle();
}

}  // namespace
}  // namespace gap
}  // namespace bt
