// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_command_handler.h"

#include <lib/async/cpp/task.h>
#include <memory>
#include <unordered_map>
#include <vector>

#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_signaling_channel.h"
#include "lib/gtest/test_loop_fixture.h"

namespace btlib {
namespace l2cap {
namespace internal {
namespace {

using common::BufferView;
using common::ByteBuffer;
using common::CreateStaticByteBuffer;
using common::LowerBits;
using common::UpperBits;

constexpr uint16_t kPsm = 0x0001;
constexpr ChannelId kLocalCId = 0x0040;
constexpr ChannelId kRemoteCId = 0x60a3;

class L2CAP_BrEdrCommandHandlerTest : public ::gtest::TestLoopFixture {
 public:
  L2CAP_BrEdrCommandHandlerTest() = default;
  ~L2CAP_BrEdrCommandHandlerTest() override = default;
  FXL_DISALLOW_COPY_AND_ASSIGN(L2CAP_BrEdrCommandHandlerTest);

 protected:
  // TestLoopFixture overrides
  void SetUp() override {
    signaling_channel_ =
        std::make_unique<testing::FakeSignalingChannel>(dispatcher());
    command_handler_ = std::make_unique<BrEdrCommandHandler>(fake_sig());
  }

  void TearDown() override {
    signaling_channel_ = nullptr;
    command_handler_ = nullptr;
  }

  testing::FakeSignalingChannel* fake_sig() const {
    return signaling_channel_.get();
  }
  BrEdrCommandHandler* cmd_handler() const { return command_handler_.get(); }

  std::unique_ptr<testing::FakeSignalingChannel> signaling_channel_;
  std::unique_ptr<BrEdrCommandHandler> command_handler_;
};

TEST_F(L2CAP_BrEdrCommandHandlerTest, OutboundConnReqRej) {
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
  auto on_conn_rsp = [&cb_called, kBadLocalCId](
                         const BrEdrCommandHandler::ConnectionResponse& rsp) {
    cb_called = true;
    EXPECT_EQ(BrEdrCommandHandler::Status::kReject, rsp.status());
    EXPECT_EQ(kInvalidChannelId, rsp.remote_cid());
    EXPECT_EQ(kBadLocalCId, rsp.local_cid());
    return false;
  };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kBadLocalCId,
                                                   std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, OutboundConnReqRspOk) {
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
  auto on_conn_rsp =
      [&cb_called](const BrEdrCommandHandler::ConnectionResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(BrEdrCommandHandler::Status::kSuccess, rsp.status());
        EXPECT_EQ(kRemoteCId, rsp.remote_cid());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(ConnectionResult::kSuccess, rsp.result());
        EXPECT_EQ(ConnectionStatus::kNoInfoAvailable, rsp.conn_status());
        return false;
      };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kLocalCId,
                                                   std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, OutboundConnReqRspPendingAuthThenOk) {
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
  EXPECT_OUTBOUND_REQ(
      *fake_sig(), kConnectionRequest, expected_conn_req.view(),
      {SignalingChannel::Status::kSuccess, pend_conn_rsp.view()},
      {SignalingChannel::Status::kSuccess, ok_conn_rsp.view()});

  int cb_count = 0;
  auto on_conn_rsp =
      [&cb_count](const BrEdrCommandHandler::ConnectionResponse& rsp) {
        cb_count++;
        EXPECT_EQ(kRemoteCId, rsp.remote_cid());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        if (cb_count == 1) {
          EXPECT_EQ(BrEdrCommandHandler::Status::kSuccess, rsp.status());
          EXPECT_EQ(ConnectionResult::kPending, rsp.result());
          EXPECT_EQ(ConnectionStatus::kAuthorizationPending, rsp.conn_status());
          return true;
        } else if (cb_count == 2) {
          EXPECT_EQ(BrEdrCommandHandler::Status::kSuccess, rsp.status());
          EXPECT_EQ(ConnectionResult::kSuccess, rsp.result());
          EXPECT_EQ(ConnectionStatus::kNoInfoAvailable, rsp.conn_status());
        }
        return false;
      };

  EXPECT_TRUE(cmd_handler()->SendConnectionRequest(kPsm, kLocalCId,
                                                   std::move(on_conn_rsp)));
  RunLoopUntilIdle();
  EXPECT_EQ(2, cb_count);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundInfoReqRspNotSupported) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type,
                                                          auto responder) {
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

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundInfoReqRspConnlessMtu) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type,
                                                          auto responder) {
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

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundInfoReqRspExtendedFeatures) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type,
                                                          auto responder) {
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

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundInfoReqRspFixedChannels) {
  BrEdrCommandHandler::InformationRequestCallback cb = [](InformationType type,
                                                          auto responder) {
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

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kInformationRequest, info_req, expected_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundConfigReqEmptyRspOkEmpty) {
  BrEdrCommandHandler::ConfigurationRequestCallback cb =
      [](ChannelId local_cid, uint16_t flags, auto& data, auto responder) {
        EXPECT_EQ(kLocalCId, local_cid);
        EXPECT_EQ(0x6006, flags);
        EXPECT_EQ(0UL, data.size());
        responder->Send(kRemoteCId, 0x0001, ConfigurationResult::kPending,
                        BufferView());
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

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kConfigurationRequest, config_req,
                                            expected_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, OutboundConfigReqRspPendingEmpty) {
  // Configuration Request payload
  auto expected_config_req = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Flags (non-zero to test encoding)
      0x01, 0x00,

      // Data
      // TODO(NET-1084): Replace with real configuration options
      't', 'e', 's', 't');
  const BufferView& req_options = expected_config_req.view(4, 4);

  // Configuration Response payload
  auto pending_config_req = CreateStaticByteBuffer(
      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Flags (non-zero to test encoding)
      0x04, 0x00,

      // Result = Pending
      0x04, 0x00,

      // Data
      // TODO(NET-1084): Replace with real configuration options
      'l', 'o', 'l', 'z');
  const BufferView& rsp_options = pending_config_req.view(6, 4);

  EXPECT_OUTBOUND_REQ(
      *fake_sig(), kConfigurationRequest, expected_config_req.view(),
      {SignalingChannel::Status::kSuccess, pending_config_req.view()});

  bool cb_called = false;
  BrEdrCommandHandler::ConfigurationResponseCallback on_config_rsp =
      [&cb_called,
       &rsp_options](const BrEdrCommandHandler::ConfigurationResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(0x0004, rsp.flags());
        EXPECT_EQ(ConfigurationResult::kPending, rsp.result());
        EXPECT_TRUE(common::ContainersEqual(rsp_options, rsp.options()));
        return false;
      };

  EXPECT_TRUE(cmd_handler()->SendConfigurationRequest(
      kRemoteCId, 0x0001, req_options, std::move(on_config_rsp)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, OutboundDisconReqRspOk) {
  // Disconnect Request payload
  auto expected_discon_req = CreateStaticByteBuffer(
      // Destination CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Disconnect Response payload
  // Channel endpoint roles (source, destination) are relative to requester so
  // the response's payload should be the same as the request's
  const ByteBuffer& ok_discon_rsp = expected_discon_req;

  EXPECT_OUTBOUND_REQ(
      *fake_sig(), kDisconnectionRequest, expected_discon_req.view(),
      {SignalingChannel::Status::kSuccess, ok_discon_rsp.view()});

  bool cb_called = false;
  BrEdrCommandHandler::DisconnectionResponseCallback on_discon_req =
      [&cb_called](const BrEdrCommandHandler::DisconnectionResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kSuccess, rsp.status());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(kRemoteCId, rsp.remote_cid());
        return false;
      };

  EXPECT_TRUE(cmd_handler()->SendDisconnectionRequest(
      kRemoteCId, kLocalCId, std::move(on_discon_req)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, OutboundDisconReqRej) {
  // Disconnect Request payload
  auto expected_discon_req = CreateStaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Source CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  // Command Reject payload
  auto rej_cid = CreateStaticByteBuffer(
      // Reject Reason (invalid channel ID)
      LowerBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),
      UpperBits(static_cast<uint16_t>(RejectReason::kInvalidCID)),

      // Source CID (relative to rejecter)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Destination CID (relative to rejecter)
      LowerBits(kLocalCId), UpperBits(kLocalCId));

  EXPECT_OUTBOUND_REQ(*fake_sig(), kDisconnectionRequest,
                      expected_discon_req.view(),
                      {SignalingChannel::Status::kReject, rej_cid.view()});

  bool cb_called = false;
  BrEdrCommandHandler::DisconnectionResponseCallback on_discon_cb =
      [&cb_called](const BrEdrCommandHandler::DisconnectionResponse& rsp) {
        cb_called = true;
        EXPECT_EQ(SignalingChannel::Status::kReject, rsp.status());
        EXPECT_EQ(RejectReason::kInvalidCID, rsp.reject_reason());
        EXPECT_EQ(kLocalCId, rsp.local_cid());
        EXPECT_EQ(kRemoteCId, rsp.remote_cid());
        return false;
      };

  EXPECT_TRUE(cmd_handler()->SendDisconnectionRequest(kRemoteCId, kLocalCId,
                                                      std::move(on_discon_cb)));
  RunLoopUntilIdle();
  EXPECT_TRUE(cb_called);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundDisconReqRspOk) {
  BrEdrCommandHandler::DisconnectionRequestCallback cb =
      [](ChannelId local_cid, ChannelId remote_cid, auto responder) {
        EXPECT_EQ(kLocalCId, local_cid);
        EXPECT_EQ(kRemoteCId, remote_cid);
        responder->Send();
      };
  cmd_handler()->ServeDisconnectionRequest(std::move(cb));

  // Disconnection Request payload
  auto discon_req = CreateStaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  // Disconnection Response payload
  auto expected_rsp = discon_req;

  RETURN_IF_FATAL(fake_sig()->ReceiveExpect(kDisconnectionRequest, discon_req,
                                            expected_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundDisconReqRej) {
  BrEdrCommandHandler::DisconnectionRequestCallback cb =
      [](ChannelId local_cid, ChannelId remote_cid, auto responder) {
        EXPECT_EQ(kLocalCId, local_cid);
        EXPECT_EQ(kRemoteCId, remote_cid);
        responder->RejectInvalidChannelId();
      };
  cmd_handler()->ServeDisconnectionRequest(std::move(cb));

  // Disconnection Request payload
  auto discon_req = CreateStaticByteBuffer(
      // Destination CID (relative to requester)
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Source CID (relative to requester)
      LowerBits(kRemoteCId), UpperBits(kRemoteCId));

  // Disconnection Response payload
  auto expected_rsp = discon_req;

  RETURN_IF_FATAL(fake_sig()->ReceiveExpectRejectInvalidChannelId(
      kDisconnectionRequest, discon_req, kLocalCId, kRemoteCId));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundConnReqRspPending) {
  BrEdrCommandHandler::ConnectionRequestCallback cb =
      [](PSM psm, ChannelId remote_cid, auto responder) {
        EXPECT_EQ(kPsm, psm);
        EXPECT_EQ(kRemoteCId, remote_cid);
        responder->Send(kLocalCId, ConnectionResult::kPending,
                        ConnectionStatus::kAuthorizationPending);
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
      UpperBits(
          static_cast<uint16_t>(ConnectionStatus::kAuthorizationPending)));

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kConnectionRequest, conn_req, conn_rsp));
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundConnReqBadPsm) {
  constexpr uint16_t kBadPsm = 0x0002;

  // Request callback shouldn't even be called for an invalid PSM.
  bool req_cb_called = false;
  BrEdrCommandHandler::ConnectionRequestCallback cb =
      [&req_cb_called](PSM psm, ChannelId remote_cid, auto responder) {
        req_cb_called = true;
      };
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

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kConnectionRequest, conn_req, conn_rsp));
  EXPECT_FALSE(req_cb_called);
}

TEST_F(L2CAP_BrEdrCommandHandlerTest, InboundConnReqNonDynamicSrcCId) {
  // Request callback shouldn't even be called for an invalid Source Channel ID.
  bool req_cb_called = false;
  BrEdrCommandHandler::ConnectionRequestCallback cb =
      [&req_cb_called](PSM psm, ChannelId remote_cid, auto responder) {
        req_cb_called = true;
      };
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

  RETURN_IF_FATAL(
      fake_sig()->ReceiveExpect(kConnectionRequest, conn_req, conn_rsp));
  EXPECT_FALSE(req_cb_called);
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace btlib
