// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_command_handler.h"

#include <lib/async/cpp/task.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_configuration.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"

namespace bt::l2cap::internal {
namespace {

using TestBase = ::gtest::TestLoopFixture;

constexpr uint16_t kPsm = 0x0001;
constexpr ChannelId kLocalCId = 0x0040;
constexpr ChannelId kRemoteCId = 0x60a3;

class BrEdrCommandHandlerTest : public TestBase {
 public:
  BrEdrCommandHandlerTest() = default;
  ~BrEdrCommandHandlerTest() override = default;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrCommandHandlerTest);

 protected:
  // TestLoopFixture overrides
  void SetUp() override {
    TestBase::SetUp();
    signaling_channel_ = std::make_unique<testing::FakeSignalingChannel>(dispatcher());
    command_handler_ = std::make_unique<BrEdrCommandHandler>(
        fake_sig(), fit::bind_member<&BrEdrCommandHandlerTest::OnRequestFail>(this));
    request_fail_callback_ = nullptr;
    failed_requests_ = 0;
  }

  void TearDown() override {
    request_fail_callback_ = nullptr;
    signaling_channel_ = nullptr;
    command_handler_ = nullptr;
    TestBase::TearDown();
  }

  testing::FakeSignalingChannel* fake_sig() const { return signaling_channel_.get(); }
  BrEdrCommandHandler* cmd_handler() const { return command_handler_.get(); }
  size_t failed_requests() const { return failed_requests_; }

  void set_request_fail_callback(fit::closure request_fail_callback) {
    ZX_ASSERT(!request_fail_callback_);
    request_fail_callback_ = std::move(request_fail_callback);
  }

 private:
  void OnRequestFail() {
    failed_requests_++;
    if (request_fail_callback_) {
      request_fail_callback_();
    }
  }

  std::unique_ptr<testing::FakeSignalingChannel> signaling_channel_;
  std::unique_ptr<BrEdrCommandHandler> command_handler_;
  fit::closure request_fail_callback_;
  size_t failed_requests_;
};

TEST_F(BrEdrCommandHandlerTest, OutboundConnReqRej) {
  constexpr ChannelId kBadLocalCId = 0x0005;  // Not a dynamic channel

  // Connection Request payload
  auto expected_conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID
      LowerBits(kBadLocalCId), UpperBits(kBadLocalCId));

  // Command Reject payload
  auto rej_rsp = CreateStaticByteBuffer(
      // Reject Reason (invalid channel ID)
      LowerBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),
      UpperBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),

      // Local (relative to rejecter) CID
      LowerBits(kInvalidChannelId), UpperBits(kInvalidChannelId),

      // Remote (relative to rejecter) CID
      LowerBits(kBadLocalCId), UpperBits(kBadLocalCId));
  EXPECT_OUTBOUND_REQ(*fake_sig(), kConnectionRequest, expected_conn_req.view(),
                      {SignalingChannel::Status::kReject, rej_rsp.view()});

  bool cb_called = false;
  auto on_conn_rsp = [&cb_called,
                      kBadLocalCId](const BrEdrCommandHandler::ConnectionResponse& rsp) {
    cb_called = true;
    EXPECT_EQ(BrEdrCommandHandler::Status::kReject, rsp.status());
    EXPECT_EQ(kInvalidChannelId, rsp.remote_cid());
    EXPECT_EQ(kBadLocalCId, rsp.local_cid());
    return BrEdrCommandHandler::ResponseHandlerAction::kCompleteOutboundTransaction;
  };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kBadLocalCId, std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundConnReqRejNotEnoughBytesInRejection) {
  constexpr ChannelId kBadLocalCId = 0x0005;  // Not a dynamic channel

  // Connection Request payload
  auto expected_conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID
      LowerBits(kBadLocalCId), UpperBits(kBadLocalCId));

  // Command Reject payload (the invalid channel IDs are missing)
  auto rej_rsp = CreateStaticByteBuffer(
      // Reject Reason (invalid channel ID)
      LowerBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),
      UpperBits(static_cast<uint16_t>(RejectReason::kInvalidCID)));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kConnectionRequest, expected_conn_req.view(),
                      {SignalingChannel::Status::kReject, rej_rsp.view()});

  bool cb_called = false;
  auto on_conn_rsp = [&cb_called](const BrEdrCommandHandler::ConnectionResponse& rsp) {
    cb_called = true;
    return BrEdrCommandHandler::ResponseHandlerAction::kCompleteOutboundTransaction;
  };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kBadLocalCId, std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundConnReqRspOk) {
  // Connection Request payload
  auto expected_conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Connection Response payload
  auto ok_conn_rsp = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Result (Successful)
      0x00, 0x00,

      // Status (No further information available)
      0x00, 0x00);
  EXPECT_OUTBOUND_REQ(*fake_sig(), kConnectionRequest, expected_conn_req.view(),
                      {SignalingChannel::Status::kSuccess, ok_conn_rsp.view()});

  bool cb_called = false;
  auto on_conn_rsp = [&cb_called](const BrEdrCommandHandler::ConnectionResponse& rsp) {
    cb_called = true;
    EXPECT_EQ(BrEdrCommandHandler::Status::kSuccess, rsp.status());
    EXPECT_EQ(kRemoteCId, rsp.remote_cid());
    EXPECT_EQ(kLocalCId, rsp.local_cid());
    EXPECT_EQ(ConnectionResult::kSuccess, rsp.result());
    EXPECT_EQ(ConnectionStatus::kNoInfoAvailable, rsp.conn_status());
    return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
  };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kLocalCId, std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundConnReqRspPendingAuthThenOk) {
  // Connection Request payload
  auto expected_conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Connection Response payload
  auto pend_conn_rsp = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Result (Pending)
      0x01, 0x00,

      // Status (Authorization pending)
      0x02, 0x00);

  auto ok_conn_rsp = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Result (Successful)
      0x00, 0x00,

      // Status (No further information available)
      0x00, 0x00);
  EXPECT_OUTBOUND_REQ(*fake_sig(), kConnectionRequest, expected_conn_req.view(),
                      {SignalingChannel::Status::kSuccess, pend_conn_rsp.view()},
                      {SignalingChannel::Status::kSuccess, ok_conn_rsp.view()});

  int cb_count = 0;
  auto on_conn_rsp = [&cb_count](const BrEdrCommandHandler::ConnectionResponse& rsp) {
    cb_count++;
    EXPECT_EQ(kRemoteCId, rsp.remote_cid());
    EXPECT_EQ(kLocalCId, rsp.local_cid());
    if (cb_count == 1) {
      EXPECT_EQ(BrEdrCommandHandler::Status::kSuccess, rsp.status());
      EXPECT_EQ(ConnectionResult::kPending, rsp.result());
      EXPECT_EQ(ConnectionStatus::kAuthorizationPending, rsp.conn_status());
      return SignalingChannel::ResponseHandlerAction::kExpectAdditionalResponse;
    } else if (cb_count == 2) {
      EXPECT_EQ(BrEdrCommandHandler::Status::kSuccess, rsp.status());
      EXPECT_EQ(ConnectionResult::kSuccess, rsp.result());
      EXPECT_EQ(ConnectionStatus::kNoInfoAvailable, rsp.conn_status());
    }
    return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
  };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kLocalCId, std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_EQ(2, cb_count);
}

TEST_F(BrEdrCommandHandlerTest, OutboundConnReqRspTimeOut) {
  // Connection Request payload
  auto expected_conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kConnectionRequest, expected_conn_req.view(),
                      {SignalingChannel::Status::kTimeOut, {}});

  ASSERT_EQ(0u, failed_requests());

  auto on_conn_rsp = [](auto&) {
    ADD_FAILURE();
    return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
  };
  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kLocalCId, std::move(on_conn_rsp)));
  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_EQ(1u, failed_requests());
}

TEST_F(BrEdrCommandHandlerTest, InboundInfoReqRspNotSupported) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type, auto responder) {
    EXPECT_EQ(InformationType::kConnectionlessMTU, type);
    responder->SendNotSupported();
  };
  cmd_handler()->ServeInformationRequest(std::move(cb));

  // Information Request payload
  auto info_req = CreateStaticByteBuffer(
      // Type = Connectionless MTU
      0x01, 0x00);

  // Information Response payload
  auto expected_rsp = CreateStaticByteBuffer(
      // Type = Connectionless MTU
      0x01, 0x00,

      // Result = Not supported
      0x01, 0x00);

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(BrEdrCommandHandlerTest, InboundInfoReqRspConnlessMtu) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type, auto responder) {
    EXPECT_EQ(InformationType::kConnectionlessMTU, type);
    responder->SendConnectionlessMtu(0x02dc);
  };
  cmd_handler()->ServeInformationRequest(std::move(cb));

  // Information Request payload
  auto info_req = CreateStaticByteBuffer(
      // Type = Connectionless MTU
      0x01, 0x00);

  // Information Response payload
  auto expected_rsp = CreateStaticByteBuffer(
      // Type = Connectionless MTU
      0x01, 0x00,

      // Result = Success
      0x00, 0x00,

      // Data (MTU)
      0xdc, 0x02);

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(BrEdrCommandHandlerTest, InboundInfoReqRspExtendedFeatures) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type, auto responder) {
    EXPECT_EQ(InformationType::kExtendedFeaturesSupported, type);
    responder->SendExtendedFeaturesSupported(0xfaceb00c);
  };
  cmd_handler()->ServeInformationRequest(std::move(cb));

  // Information Request payload
  auto info_req = CreateStaticByteBuffer(
      // Type = Features Mask
      0x02, 0x00);

  // Information Response payload
  auto expected_rsp = CreateStaticByteBuffer(
      // Type = Features Mask
      0x02, 0x00,

      // Result = Success
      0x00, 0x00,

      // Data (Mask)
      0x0c, 0xb0, 0xce, 0xfa);

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(BrEdrCommandHandlerTest, InboundInfoReqRspFixedChannels) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type, auto responder) {
    EXPECT_EQ(InformationType::kFixedChannelsSupported, type);
    responder->SendFixedChannelsSupported(0xcafef00d4badc0deUL);
  };
  cmd_handler()->ServeInformationRequest(std::move(cb));

  // Information Request payload
  auto info_req = CreateStaticByteBuffer(
      // Type = Fixed Channels
      0x03, 0x00);

  // Configuration Response payload
  auto expected_rsp = CreateStaticByteBuffer(
      // Type = Fixed Channels
      0x03, 0x00,

      // Result = Success
      0x00, 0x00,

      // Data (Mask)
      0xde, 0xc0, 0xad, 0x4b, 0x0d, 0xf0, 0xfe, 0xca);

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(BrEdrCommandHandlerTest, InboundConfigReqEmptyRspOkEmpty) {
  BrEdrCommandHandler::ConfigurationRequestCallback cb =
      [](ChannelId local_cid, uint16_t flags, ChannelConfiguration config, auto responder) {
        EXPECT_EQ(kLocalCId, local_cid);
        EXPECT_EQ(0x6006, flags);
        EXPECT_FALSE(config.mtu_option().has_value());
        EXPECT_FALSE(config.retransmission_flow_control_option().has_value());
        EXPECT_EQ(0u, config.unknown_options().size());

        responder->Send(kRemoteCId, 0x0001, ConfigurationResult::kPending,
                        ChannelConfiguration::ConfigurationOptions());
      };
  cmd_handler()->ServeConfigurationRequest(std::move(cb));

  // Configuration Request payload
  auto config_req = CreateStaticByteBuffer(
      // Destination Channel ID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Flags
      0x06, 0x60);

  // Configuration Response payload
  auto expected_rsp = CreateStaticByteBuffer(
      // Destination Channel ID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Flags
      0x01, 0x00,

      // Result = Pending
      0x04, 0x00);

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kConfigurationRequest, config_req, expected_rsp));
}

TEST_F(BrEdrCommandHandlerTest, OutboundConfigReqRspPendingEmpty) {
  // Configuration Request payload
  auto expected_config_req = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Flags (non-zero to test encoding)
      0x01, 0x00,

      // Data (Config Options)
      0x01,       // Type = MTU
      0x02,       // Length = 2
      0x30, 0x00  // MTU = 48
  );

  // Configuration Response payload
  auto pending_config_req = CreateStaticByteBuffer(
      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Flags (non-zero to test encoding)
      0x04, 0x00,

      // Result = Pending
      0x04, 0x00,

      // Data (Config Options)
      0x01,       // Type = MTU
      0x02,       // Length = 2
      0x60, 0x00  // MTU = 96
  );

  EXPECT_OUTBOUND_REQ(*fake_sig(), kConfigurationRequest, expected_config_req.view(),
                      {SignalingChannel::Status::kSuccess, pending_config_req.view()});

  bool cb_called = false;
  BrEdrCommandHandler::ConfigurationResponseCallback on_config_rsp =
      [&cb_called](const BrEdrCommandHandler::ConfigurationResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(0x0004, rsp.flags());
        EXPECT_EQ(ConfigurationResult::kPending, rsp.result());
        EXPECT_TRUE(rsp.config().mtu_option().has_value());
        EXPECT_EQ(96u, rsp.config().mtu_option()->mtu());
        return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
      };

  ChannelConfiguration config;
  config.set_mtu_option(ChannelConfiguration::MtuOption(48));

  EXPECT_TRUE(cmd_handler()->SendConfigurationRequest(kRemoteCId, 0x0001, config.Options(),
                                                      std::move(on_config_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundConfigReqRspTimeOut) {
  // Configuration Request payload
  auto expected_config_req = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Flags (non-zero to test encoding)
      0x01, 0xf0,

      // Data (Config Options)
      0x01,       // Type = MTU
      0x02,       // Length = 2
      0x30, 0x00  // MTU = 48
  );

  // Disconnect Request payload
  auto expected_discon_req = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kConfigurationRequest, expected_config_req.view(),
                      {SignalingChannel::Status::kTimeOut, {}});
  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest, expected_discon_req.view());

  set_request_fail_callback([this]() {
    // Should still be allowed to send requests even after one failed
    auto on_discon_rsp = [](auto&) {};
    EXPECT_TRUE(
        cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId, std::move(on_discon_rsp)));
  });

  ASSERT_EQ(0u, failed_requests());

  auto on_config_rsp = [](auto&) {
    ADD_FAILURE();
    return SignalingChannel::ResponseHandlerAction::kCompleteOutboundTransaction;
  };

  ChannelConfiguration config;
  config.set_mtu_option(ChannelConfiguration::MtuOption(48));

  EXPECT_TRUE(cmd_handler()->SendConfigurationRequest(kRemoteCId, 0xf001, config.Options(),
                                                      std::move(on_config_rsp)));
  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_EQ(1u, failed_requests());
}
TEST_F(BrEdrCommandHandlerTest, OutboundInfoReqRspOk) {
  // Information Request payload
  auto expected_info_req = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)));

  // Information Response payload
  auto ok_info_rsp = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),

      // Information Result
      LowerBits(static_cast<uint16_t>(InformationResult::kSuccess)),
      UpperBits(static_cast<uint16_t>(InformationResult::kSuccess)),

      // Data (extended features mask, 4 bytes)
      LowerBits(kExtendedFeaturesBitFixedChannels), 0, 0, 0);

  EXPECT_OUTBOUND_REQ(*fake_sig(), kInformationRequest, expected_info_req.view(),
                      {SignalingChannel::Status::kSuccess, ok_info_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::InformationResponseCallback on_info_cb =
      [&cb_called](const BrEdrCommandHandler::InformationResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(InformationResult::kSuccess, rsp.result());
        EXPECT_EQ(InformationType::kExtendedFeaturesSupported, rsp.type());
        EXPECT_EQ(kExtendedFeaturesBitFixedChannels, rsp.extended_features());
      };

  EXPECT_TRUE(cmd_handler()->SendInformationRequest(InformationType::kExtendedFeaturesSupported,
                                                    std::move(on_info_cb)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundInfoReqRspNotSupported) {
  // Information Request payload
  auto expected_info_req = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kConnectionlessMTU)),
      UpperBits(static_cast<uint16_t>(InformationType::kConnectionlessMTU)));

  // Information Response payload
  auto error_info_rsp = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kConnectionlessMTU)),
      UpperBits(static_cast<uint16_t>(InformationType::kConnectionlessMTU)),

      // Information Result
      LowerBits(static_cast<uint16_t>(InformationResult::kNotSupported)),
      UpperBits(static_cast<uint16_t>(InformationResult::kNotSupported)));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kInformationRequest, expected_info_req.view(),
                      {SignalingChannel::Status::kSuccess, error_info_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::InformationResponseCallback on_info_cb =
      [&cb_called](const BrEdrCommandHandler::InformationResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(InformationResult::kNotSupported, rsp.result());
        EXPECT_EQ(InformationType::kConnectionlessMTU, rsp.type());
      };

  EXPECT_TRUE(cmd_handler()->SendInformationRequest(InformationType::kConnectionlessMTU,
                                                    std::move(on_info_cb)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundInfoReqRspHeaderNotEnoughBytes) {
  // Information Request payload
  auto expected_info_req = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)));

  // Information Response payload
  auto malformed_info_rsp = CreateStaticByteBuffer(
      // 1 of 4 bytes expected of an Information Response just to be able to parse it.
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kInformationRequest, expected_info_req.view(),
                      {SignalingChannel::Status::kSuccess, malformed_info_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::InformationResponseCallback on_info_cb =
      [&cb_called](const BrEdrCommandHandler::InformationResponse& rsp) { cb_called = true; };

  EXPECT_TRUE(cmd_handler()->SendInformationRequest(InformationType::kExtendedFeaturesSupported,
                                                    std::move(on_info_cb)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, OutboundInfoReqRspPayloadNotEnoughBytes) {
  // Information Request payload
  auto expected_info_req = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)));

  // Information Response payload
  auto malformed_info_rsp = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),

      // Information Result
      LowerBits(static_cast<uint16_t>(InformationResult::kSuccess)),
      UpperBits(static_cast<uint16_t>(InformationResult::kSuccess)),

      // Data (2 of 4 bytes expected of an extended features mask)
      0, 0);

  EXPECT_OUTBOUND_REQ(*fake_sig(), kInformationRequest, expected_info_req.view(),
                      {SignalingChannel::Status::kSuccess, malformed_info_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::InformationResponseCallback on_info_cb =
      [&cb_called](const BrEdrCommandHandler::InformationResponse& rsp) { cb_called = true; };

  EXPECT_TRUE(cmd_handler()->SendInformationRequest(InformationType::kExtendedFeaturesSupported,
                                                    std::move(on_info_cb)));
  RunLoopUntilIdle();
  EXPECT_FALSE(cb_called);
}

// Accept and pass a valid Information Response even if it doesn't have the type
// requested.
TEST_F(BrEdrCommandHandlerTest, OutboundInfoReqRspWrongType) {
  // Information Request payload
  auto expected_info_req = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)),
      UpperBits(static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported)));

  // Information Response payload
  auto mismatch_info_rsp = CreateStaticByteBuffer(
      // Information Type
      LowerBits(static_cast<uint16_t>(InformationType::kConnectionlessMTU)),
      UpperBits(static_cast<uint16_t>(InformationType::kConnectionlessMTU)),

      // Information Result
      LowerBits(static_cast<uint16_t>(InformationResult::kSuccess)),
      UpperBits(static_cast<uint16_t>(InformationResult::kSuccess)),

      // Data (connectionless broadcast MTU, 2 bytes)
      0x40, 0);

  EXPECT_OUTBOUND_REQ(*fake_sig(), kInformationRequest, expected_info_req.view(),
                      {SignalingChannel::Status::kSuccess, mismatch_info_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::InformationResponseCallback on_info_cb =
      [&cb_called](const BrEdrCommandHandler::InformationResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(InformationResult::kSuccess, rsp.result());
        EXPECT_EQ(InformationType::kConnectionlessMTU, rsp.type());
        EXPECT_EQ(0x40, rsp.connectionless_mtu());
      };

  EXPECT_TRUE(cmd_handler()->SendInformationRequest(InformationType::kExtendedFeaturesSupported,
                                                    std::move(on_info_cb)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

// Allow types of information besides those known to BrEdrCommandHandler.
TEST_F(BrEdrCommandHandlerTest, OutboundInfoReqUnknownType) {
  // Information Request payload
  auto expected_info_req = CreateStaticByteBuffer(
      // Information Type
      0x04, 0);

  // Information Response payload
  auto ok_info_rsp = CreateStaticByteBuffer(
      // Information Type
      0x04, 0,

      // Information Result
      LowerBits(static_cast<uint16_t>(InformationResult::kSuccess)),
      UpperBits(static_cast<uint16_t>(InformationResult::kSuccess)),

      // Data (some payload)
      't', 'e', 's', 't');

  EXPECT_OUTBOUND_REQ(*fake_sig(), kInformationRequest, expected_info_req.view(),
                      {SignalingChannel::Status::kSuccess, ok_info_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::InformationResponseCallback on_info_cb =
      [&cb_called](const BrEdrCommandHandler::InformationResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(InformationResult::kSuccess, rsp.result());
        EXPECT_EQ(static_cast<InformationType>(0x04), rsp.type());
      };

  EXPECT_TRUE(cmd_handler()->SendInformationRequest(static_cast<InformationType>(0x04),
                                                    std::move(on_info_cb)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(BrEdrCommandHandlerTest, InboundConnReqRspPending) {
  BrEdrCommandHandler::ConnectionRequestCallback cb = [](PSM psm, ChannelId remote_cid,
                                                         auto responder) {
    EXPECT_EQ(kPsm, psm);
    EXPECT_EQ(kRemoteCId, remote_cid);
    responder->Send(kLocalCId, ConnectionResult::kPending, ConnectionStatus::kAuthorizationPending);
  };
  cmd_handler()->ServeConnectionRequest(std::move(cb));

  // Connection Request payload
  auto conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  // Connection Response payload
  auto conn_rsp = CreateStaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Connection Result
      LowerBits(static_cast<uint16_t>(ConnectionResult::kPending)),
      UpperBits(static_cast<uint16_t>(ConnectionResult::kPending)),

      // Connection Status
      LowerBits(static_cast<uint16_t>(ConnectionStatus::kAuthorizationPending)),
      UpperBits(static_cast<uint16_t>(ConnectionStatus::kAuthorizationPending)));

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kConnectionRequest, conn_req, conn_rsp));
}

TEST_F(BrEdrCommandHandlerTest, InboundConnReqBadPsm) {
  constexpr uint16_t kBadPsm = 0x0002;

  // Request callback shouldn't even be called for an invalid PSM.
  bool req_cb_called = false;
  BrEdrCommandHandler::ConnectionRequestCallback cb =
      [&req_cb_called](PSM psm, ChannelId remote_cid, auto responder) { req_cb_called = true; };
  cmd_handler()->ServeConnectionRequest(std::move(cb));

  // Connection Request payload
  auto conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kBadPsm), UpperBits(kBadPsm),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  // Connection Response payload
  auto conn_rsp = CreateStaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kInvalidChannelId), UpperBits(kInvalidChannelId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Connection Result
      LowerBits(static_cast<uint16_t>(ConnectionResult::kPSMNotSupported)),
      UpperBits(static_cast<uint16_t>(ConnectionResult::kPSMNotSupported)),

      // Connection Status
      LowerBits(static_cast<uint16_t>(ConnectionStatus::kNoInfoAvailable)),
      UpperBits(static_cast<uint16_t>(ConnectionStatus::kNoInfoAvailable)));

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kConnectionRequest, conn_req, conn_rsp));
  EXPECT_FALSE(req_cb_called);
}

TEST_F(BrEdrCommandHandlerTest, InboundConnReqNonDynamicSrcCId) {
  // Request callback shouldn't even be called for an invalid Source Channel ID.
  bool req_cb_called = false;
  BrEdrCommandHandler::ConnectionRequestCallback cb =
      [&req_cb_called](PSM psm, ChannelId remote_cid, auto responder) { req_cb_called = true; };
  cmd_handler()->ServeConnectionRequest(std::move(cb));

  // Connection Request payload
  auto conn_req = CreateStaticByteBuffer(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID: fixed channel for Security Manager (relative to requester)
      LowerBits(kSMPChannelId), UpperBits(kSMPChannelId));

  // Connection Response payload
  auto conn_rsp = CreateStaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kInvalidChannelId), UpperBits(kInvalidChannelId),

      // Source CID (relative to requester)
      LowerBits(kSMPChannelId), UpperBits(kSMPChannelId),

      // Connection Result
      LowerBits(static_cast<uint16_t>(ConnectionResult::kInvalidSourceCID)),
      UpperBits(static_cast<uint16_t>(ConnectionResult::kInvalidSourceCID)),

      // Connection Status
      LowerBits(static_cast<uint16_t>(ConnectionStatus::kNoInfoAvailable)),
      UpperBits(static_cast<uint16_t>(ConnectionStatus::kNoInfoAvailable)));

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kConnectionRequest, conn_req, conn_rsp));
  EXPECT_FALSE(req_cb_called);
}

}  // namespace
}  // namespace bt::l2cap::internal
