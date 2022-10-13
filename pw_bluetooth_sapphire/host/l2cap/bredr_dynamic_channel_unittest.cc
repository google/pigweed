// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/bredr_dynamic_channel.h"

#include <lib/async/cpp/task.h>

#include <vector>

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/protocol.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_signaling_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bt::l2cap::internal {
namespace {

// TODO(fxbug.dev/1049): Add integration test with FakeChannelTest and
// BrEdrSignalingChannel using snooped connection data to verify signaling
// channel traffic.

constexpr uint16_t kPsm = 0x0001;
constexpr uint16_t kInvalidPsm = 0x0002;  // Valid PSMs are odd.
constexpr ChannelId kLocalCId = 0x0040;
constexpr ChannelId kLocalCId2 = 0x0041;
constexpr ChannelId kRemoteCId = 0x60a3;
constexpr ChannelId kBadCId = 0x003f;  // Not a dynamic channel.

constexpr ChannelParameters kChannelParams;
constexpr ChannelParameters kERTMChannelParams{ChannelMode::kEnhancedRetransmission, std::nullopt,
                                               std::nullopt};

// Commands Reject

const StaticByteBuffer kRejNotUnderstood(
    // Reject Reason (Not Understood)
    0x00, 0x00);

// Connection Requests

const StaticByteBuffer kConnReq(
    // PSM
    LowerBits(kPsm), UpperBits(kPsm),

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId));

auto MakeConnectionRequest(ChannelId src_id, PSM psm) {
  return StaticByteBuffer(
      // PSM
      LowerBits(psm), UpperBits(psm),

      // Source CID
      LowerBits(src_id), UpperBits(src_id));
}

const StaticByteBuffer kInboundConnReq(
    // PSM
    LowerBits(kPsm), UpperBits(kPsm),

    // Source CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId));

const StaticByteBuffer kInboundInvalidPsmConnReq(
    // PSM
    LowerBits(kInvalidPsm), UpperBits(kInvalidPsm),

    // Source CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId));

const StaticByteBuffer kInboundBadCIdConnReq(
    // PSM
    LowerBits(kPsm), UpperBits(kPsm),

    // Source CID
    LowerBits(kBadCId), UpperBits(kBadCId));

// Connection Responses

const StaticByteBuffer kPendingConnRsp(
    // Destination CID
    0x00, 0x00,

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Result (Pending)
    0x01, 0x00,

    // Status (Authorization Pending)
    0x02, 0x00);

const StaticByteBuffer kPendingConnRspWithId(
    // Destination CID (Wrong endianness but valid)
    UpperBits(kRemoteCId), LowerBits(kRemoteCId),

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Result (Pending)
    0x01, 0x00,

    // Status (Authorization Pending)
    0x02, 0x00);

auto MakeConnectionResponseWithResultPending(ChannelId src_id, ChannelId dst_id) {
  return StaticByteBuffer(
      // Destination CID
      LowerBits(dst_id), UpperBits(dst_id),

      // Source CID
      LowerBits(src_id), UpperBits(src_id),

      // Result (Pending)
      0x01, 0x00,

      // Status (Authorization Pending)
      0x02, 0x00);
}

const StaticByteBuffer kOkConnRsp(
    // Destination CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId),

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Result (Successful)
    0x00, 0x00,

    // Status (No further information available)
    0x00, 0x00);

auto MakeConnectionResponse(ChannelId src_id, ChannelId dst_id) {
  return StaticByteBuffer(
      // Destination CID
      LowerBits(dst_id), UpperBits(dst_id),

      // Source CID
      LowerBits(src_id), UpperBits(src_id),

      // Result (Successful)
      0x00, 0x00,

      // Status (No further information available)
      0x00, 0x00);
}

const StaticByteBuffer kInvalidConnRsp(
    // Destination CID (Not a dynamic channel ID)
    LowerBits(kBadCId), UpperBits(kBadCId),

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Result (Successful)
    0x00, 0x00,

    // Status (No further information available)
    0x00, 0x00);

const StaticByteBuffer kRejectConnRsp(
    // Destination CID (Invalid)
    LowerBits(kInvalidChannelId), UpperBits(kInvalidChannelId),

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Result (No resources)
    0x04, 0x00,

    // Status (No further information available)
    0x00, 0x00);

const StaticByteBuffer kInboundOkConnRsp(
    // Destination CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Source CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId),

    // Result (Successful)
    0x00, 0x00,

    // Status (No further information available)
    0x00, 0x00);

const StaticByteBuffer kOutboundSourceCIdAlreadyAllocatedConnRsp(
    // Destination CID (Invalid)
    0x00, 0x00,

    // Source CID (Invalid)
    LowerBits(kRemoteCId), UpperBits(kRemoteCId),

    // Result (Connection refused - source CID already allocated)
    0x07, 0x00,

    // Status (No further information available)
    0x00, 0x00);

const StaticByteBuffer kInboundBadPsmConnRsp(
    // Destination CID (Invalid)
    0x00, 0x00,

    // Source CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId),

    // Result (PSM Not Supported)
    0x02, 0x00,

    // Status (No further information available)
    0x00, 0x00);

const StaticByteBuffer kInboundBadCIdConnRsp(
    // Destination CID (Invalid)
    0x00, 0x00,

    // Source CID
    LowerBits(kBadCId), UpperBits(kBadCId),

    // Result (Invalid Source CID)
    0x06, 0x00,

    // Status (No further information available)
    0x00, 0x00);

// Disconnection Requests

const StaticByteBuffer kDisconReq(
    // Destination CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId),

    // Source CID
    LowerBits(kLocalCId), UpperBits(kLocalCId));

const StaticByteBuffer kInboundDisconReq(
    // Destination CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Source CID
    LowerBits(kRemoteCId), UpperBits(kRemoteCId));

// Disconnection Responses

const ByteBuffer& kInboundDisconRsp = kInboundDisconReq;

const ByteBuffer& kDisconRsp = kDisconReq;

// Configuration Requests

auto MakeConfigReqWithMtuAndRfc(ChannelId dest_cid, uint16_t mtu, ChannelMode mode,
                                uint8_t tx_window, uint8_t max_transmit,
                                uint16_t retransmission_timeout, uint16_t monitor_timeout,
                                uint16_t mps) {
  return StaticByteBuffer(
      // Destination CID
      LowerBits(dest_cid), UpperBits(dest_cid),

      // Flags
      0x00, 0x00,

      // MTU option (Type, Length, MTU value)
      0x01, 0x02, LowerBits(mtu), UpperBits(mtu),

      // Retransmission & Flow Control option (Type, Length = 9, mode, unused fields)
      0x04, 0x09, static_cast<uint8_t>(mode), tx_window, max_transmit,
      LowerBits(retransmission_timeout), UpperBits(retransmission_timeout),
      LowerBits(monitor_timeout), UpperBits(monitor_timeout), LowerBits(mps), UpperBits(mps));
}

auto MakeConfigReqWithMtu(ChannelId dest_cid, uint16_t mtu = kMaxMTU, uint16_t flags = 0x0000) {
  return StaticByteBuffer(
      // Destination CID
      LowerBits(dest_cid), UpperBits(dest_cid),

      // Flags
      LowerBits(flags), UpperBits(flags),

      // MTU option (Type, Length, MTU value)
      0x01, 0x02, LowerBits(mtu), UpperBits(mtu));
}

const ByteBuffer& kOutboundConfigReq = MakeConfigReqWithMtu(kRemoteCId);

const ByteBuffer& kOutboundConfigReqWithErtm = MakeConfigReqWithMtuAndRfc(
    kRemoteCId, kMaxInboundPduPayloadSize, ChannelMode::kEnhancedRetransmission,
    kErtmMaxUnackedInboundFrames, kErtmMaxInboundRetransmissions, 0, 0, kMaxInboundPduPayloadSize);

const StaticByteBuffer kInboundConfigReq(
    // Destination CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Flags
    0x00, 0x00);

const StaticByteBuffer kInboundConfigReq2(
    // Destination CID
    LowerBits(kLocalCId2), UpperBits(kLocalCId2),

    // Flags
    0x00, 0x00);

// Use plausible ERTM parameters that do not necessarily match values in production. See Core Spec
// v5.0 Vol 3, Part A, Sec 5.4 for meanings.
constexpr uint8_t kErtmNFramesInTxWindow = 32;
constexpr uint8_t kErtmMaxTransmissions = 8;
constexpr uint16_t kMaxTxPduPayloadSize = 1024;

const StaticByteBuffer kInboundConfigReqWithERTM(
    // Destination CID
    LowerBits(kLocalCId), UpperBits(kLocalCId),

    // Flags
    0x00, 0x00,

    // Retransmission & Flow Control option (Type, Length = 9, mode = ERTM, dummy parameters)
    0x04, 0x09, 0x03, kErtmNFramesInTxWindow, kErtmMaxTransmissions, 0x00, 0x00, 0x00, 0x00,
    LowerBits(kMaxTxPduPayloadSize), UpperBits(kMaxTxPduPayloadSize));

// Configuration Responses

auto MakeEmptyConfigRsp(ChannelId src_id,
                        ConfigurationResult result = ConfigurationResult::kSuccess,
                        uint16_t flags = 0x0000) {
  return StaticByteBuffer(
      // Source CID
      LowerBits(src_id), UpperBits(src_id),

      // Flags
      LowerBits(flags), UpperBits(flags),

      // Result
      LowerBits(static_cast<uint16_t>(result)), UpperBits(static_cast<uint16_t>(result)));
}

const ByteBuffer& kOutboundEmptyContinuationConfigRsp =
    MakeEmptyConfigRsp(kRemoteCId, ConfigurationResult::kSuccess, kConfigurationContinuation);

const ByteBuffer& kInboundEmptyConfigRsp = MakeEmptyConfigRsp(kLocalCId);

const ByteBuffer& kUnknownIdConfigRsp = MakeEmptyConfigRsp(kBadCId);

const ByteBuffer& kOutboundEmptyPendingConfigRsp =
    MakeEmptyConfigRsp(kRemoteCId, ConfigurationResult::kPending);

const ByteBuffer& kInboundEmptyPendingConfigRsp =
    MakeEmptyConfigRsp(kLocalCId, ConfigurationResult::kPending);

auto MakeConfigRspWithMtu(ChannelId source_cid, uint16_t mtu,
                          ConfigurationResult result = ConfigurationResult::kSuccess,
                          uint16_t flags = 0x0000) {
  return StaticByteBuffer(
      // Source CID
      LowerBits(source_cid), UpperBits(source_cid),

      // Flags
      LowerBits(flags), UpperBits(flags),

      // Result
      LowerBits(static_cast<uint16_t>(result)), UpperBits(static_cast<uint16_t>(result)),

      // MTU option (Type, Length, MTU value)
      0x01, 0x02, LowerBits(mtu), UpperBits(mtu));
}

const ByteBuffer& kOutboundOkConfigRsp = MakeConfigRspWithMtu(kRemoteCId, kDefaultMTU);

auto MakeConfigRspWithRfc(ChannelId source_cid, ConfigurationResult result, ChannelMode mode,
                          uint8_t tx_window, uint8_t max_transmit, uint16_t retransmission_timeout,
                          uint16_t monitor_timeout, uint16_t mps) {
  return StaticByteBuffer(
      // Source CID
      LowerBits(source_cid), UpperBits(source_cid),

      // Flags
      0x00, 0x00,

      // Result
      LowerBits(static_cast<uint16_t>(result)), UpperBits(static_cast<uint16_t>(result)),

      // Retransmission & Flow Control option (Type, Length: 9, mode, unused parameters)
      0x04, 0x09, static_cast<uint8_t>(mode), tx_window, max_transmit,
      LowerBits(retransmission_timeout), UpperBits(retransmission_timeout),
      LowerBits(monitor_timeout), UpperBits(monitor_timeout), LowerBits(mps), UpperBits(mps));
}

const ByteBuffer& kInboundUnacceptableParamsWithRfcBasicConfigRsp = MakeConfigRspWithRfc(
    kLocalCId, ConfigurationResult::kUnacceptableParameters, ChannelMode::kBasic, 0, 0, 0, 0, 0);

const ByteBuffer& kOutboundUnacceptableParamsWithRfcBasicConfigRsp = MakeConfigRspWithRfc(
    kRemoteCId, ConfigurationResult::kUnacceptableParameters, ChannelMode::kBasic, 0, 0, 0, 0, 0);

const ByteBuffer& kOutboundUnacceptableParamsWithRfcERTMConfigRsp =
    MakeConfigRspWithRfc(kRemoteCId, ConfigurationResult::kUnacceptableParameters,
                         ChannelMode::kEnhancedRetransmission, 0, 0, 0, 0, 0);

auto MakeConfigRspWithMtuAndRfc(ChannelId source_cid, ConfigurationResult result, ChannelMode mode,
                                uint16_t mtu, uint8_t tx_window, uint8_t max_transmit,
                                uint16_t retransmission_timeout, uint16_t monitor_timeout,
                                uint16_t mps) {
  return StaticByteBuffer(
      // Source CID
      LowerBits(source_cid), UpperBits(source_cid),

      // Flags
      0x00, 0x00,

      // Result
      LowerBits(static_cast<uint16_t>(result)), UpperBits(static_cast<uint16_t>(result)),

      // MTU option (Type, Length, MTU value)
      0x01, 0x02, LowerBits(mtu), UpperBits(mtu),

      // Retransmission & Flow Control option (Type, Length = 9, mode, ERTM fields)
      0x04, 0x09, static_cast<uint8_t>(mode), tx_window, max_transmit,
      LowerBits(retransmission_timeout), UpperBits(retransmission_timeout),
      LowerBits(monitor_timeout), UpperBits(monitor_timeout), LowerBits(mps), UpperBits(mps));
}

// Corresponds to kInboundConfigReqWithERTM
const ByteBuffer& kOutboundOkConfigRspWithErtm = MakeConfigRspWithMtuAndRfc(
    kRemoteCId, ConfigurationResult::kSuccess, ChannelMode::kEnhancedRetransmission, kDefaultMTU,
    kErtmNFramesInTxWindow, kErtmMaxTransmissions, 2000, 12000, kMaxTxPduPayloadSize);

// Information Requests

auto MakeInfoReq(InformationType info_type) {
  const auto type = static_cast<uint16_t>(info_type);
  return StaticByteBuffer(LowerBits(type), UpperBits(type));
}

const ByteBuffer& kExtendedFeaturesInfoReq =
    MakeInfoReq(InformationType::kExtendedFeaturesSupported);

// Information Responses

auto MakeExtendedFeaturesInfoRsp(InformationResult result = InformationResult::kSuccess,
                                 ExtendedFeatures features = 0u) {
  const auto type = static_cast<uint16_t>(InformationType::kExtendedFeaturesSupported);
  const auto res = static_cast<uint16_t>(result);
  const auto features_bytes = ToBytes(features);
  return StaticByteBuffer(
      // Type
      LowerBits(type), UpperBits(type),

      // Result
      LowerBits(res), UpperBits(res),

      // Data
      features_bytes[0], features_bytes[1], features_bytes[2], features_bytes[3]);
}

const ByteBuffer& kExtendedFeaturesInfoRsp =
    MakeExtendedFeaturesInfoRsp(InformationResult::kSuccess);
const ByteBuffer& kExtendedFeaturesInfoRspWithERTM = MakeExtendedFeaturesInfoRsp(
    InformationResult::kSuccess, kExtendedFeaturesBitEnhancedRetransmission);

class BrEdrDynamicChannelTest : public ::gtest::TestLoopFixture {
 public:
  BrEdrDynamicChannelTest() = default;
  ~BrEdrDynamicChannelTest() override = default;

 protected:
  // Import types for brevity.
  using DynamicChannelCallback = DynamicChannelRegistry::DynamicChannelCallback;
  using ServiceRequestCallback = DynamicChannelRegistry::ServiceRequestCallback;

  // TestLoopFixture overrides
  void SetUp() override {
    TestLoopFixture::SetUp();
    channel_close_cb_ = nullptr;
    service_request_cb_ = nullptr;
    signaling_channel_ = std::make_unique<testing::FakeSignalingChannel>(dispatcher());

    ext_info_transaction_id_ =
        EXPECT_OUTBOUND_REQ(*sig(), kInformationRequest, kExtendedFeaturesInfoReq.view());
    // TODO(63074): Make these tests not rely on strict ordering of channel IDs.
    registry_ = std::make_unique<BrEdrDynamicChannelRegistry>(
        sig(), fit::bind_member<&BrEdrDynamicChannelTest::OnChannelClose>(this),
        fit::bind_member<&BrEdrDynamicChannelTest::OnServiceRequest>(this),
        /*random_channel_ids=*/false);
  }

  void TearDown() override {
    RunLoopUntilIdle();
    registry_ = nullptr;
    signaling_channel_ = nullptr;
    service_request_cb_ = nullptr;
    channel_close_cb_ = nullptr;
    TestLoopFixture::TearDown();
  }

  testing::FakeSignalingChannel* sig() const { return signaling_channel_.get(); }

  BrEdrDynamicChannelRegistry* registry() const { return registry_.get(); }

  void set_channel_close_cb(DynamicChannelCallback close_cb) {
    channel_close_cb_ = std::move(close_cb);
  }

  void set_service_request_cb(ServiceRequestCallback service_request_cb) {
    service_request_cb_ = std::move(service_request_cb);
  }

  testing::FakeSignalingChannel::TransactionId ext_info_transaction_id() {
    return ext_info_transaction_id_;
  }

 private:
  void OnChannelClose(const DynamicChannel* channel) {
    if (channel_close_cb_) {
      channel_close_cb_(channel);
    }
  }

  // Default to rejecting all service requests if no test callback is set.
  std::optional<DynamicChannelRegistry::ServiceInfo> OnServiceRequest(PSM psm) {
    if (service_request_cb_) {
      return service_request_cb_(psm);
    }
    return std::nullopt;
  }

  DynamicChannelCallback channel_close_cb_;
  ServiceRequestCallback service_request_cb_;
  std::unique_ptr<testing::FakeSignalingChannel> signaling_channel_;
  std::unique_ptr<BrEdrDynamicChannelRegistry> registry_;
  testing::FakeSignalingChannel::TransactionId ext_info_transaction_id_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(BrEdrDynamicChannelTest);
};

TEST_F(BrEdrDynamicChannelTest,
       InboundConnectionResponseReusingChannelIdCausesInboundChannelFailure) {
  // make successful connection
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
      EXPECT_TRUE(chan->IsConnected());
      EXPECT_EQ(kLocalCId, chan->local_cid());
      EXPECT_EQ(kRemoteCId, chan->remote_cid());
    }
    open_cb_count++;
  };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) {
    EXPECT_TRUE(chan);
    close_cb_count++;
  });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);

  // simulate inbound request to make new connection using already allocated remote CId
  sig()->ReceiveExpect(kConnectionRequest, kInboundConnReq,
                       kOutboundSourceCIdAlreadyAllocatedConnRsp);
}

TEST_F(BrEdrDynamicChannelTest,
       PeerConnectionResponseReusingChannelIdCausesOutboundChannelFailure) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  // make successful connection
  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
      EXPECT_TRUE(chan->IsConnected());
      EXPECT_EQ(kLocalCId, chan->local_cid());
      EXPECT_EQ(kRemoteCId, chan->remote_cid());
    }
    open_cb_count++;
  };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) {
    EXPECT_TRUE(chan);
    close_cb_count++;
  });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);

  // peer responds with already allocated remote CID
  const auto kConnReq2 = MakeConnectionRequest(kLocalCId2, kPsm);
  const auto kOkConnRspSamePeerCId = MakeConnectionResponse(kLocalCId2, kRemoteCId);

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq2.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRspSamePeerCId.view()});

  auto channel =
      BrEdrDynamicChannel::MakeOutbound(registry(), sig(), kPsm, kLocalCId2, kChannelParams, false);
  EXPECT_FALSE(channel->IsConnected());
  EXPECT_FALSE(channel->IsOpen());

  int close_cb_count2 = 0;
  set_channel_close_cb([&close_cb_count2](auto) { close_cb_count2++; });

  int open_cb_count2 = 0;
  channel->Open([&open_cb_count2] { open_cb_count2++; });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_FALSE(channel->IsConnected());
  EXPECT_FALSE(channel->IsOpen());
  EXPECT_EQ(open_cb_count2, 1);
  EXPECT_EQ(close_cb_count2, 0);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

TEST_F(BrEdrDynamicChannelTest,
       PeerPendingConnectionResponseReusingChannelIdCausesOutboundChannelFailure) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  // make successful connection
  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
      EXPECT_TRUE(chan->IsConnected());
      EXPECT_EQ(kLocalCId, chan->local_cid());
      EXPECT_EQ(kRemoteCId, chan->remote_cid());
    }
    open_cb_count++;
  };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) {
    EXPECT_TRUE(chan);
    close_cb_count++;
  });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);

  // peer responds with already allocated remote CID
  const auto kConnReq2 = MakeConnectionRequest(kLocalCId2, kPsm);
  const auto kOkConnRspWithResultPendingSamePeerCId =
      MakeConnectionResponseWithResultPending(kLocalCId2, kRemoteCId);
  EXPECT_OUTBOUND_REQ(
      *sig(), kConnectionRequest, kConnReq2.view(),
      {SignalingChannel::Status::kSuccess, kOkConnRspWithResultPendingSamePeerCId.view()});

  int open_cb_count2 = 0;
  int close_cb_count2 = 0;
  set_channel_close_cb([&close_cb_count2](auto) { close_cb_count2++; });

  registry()->OpenOutbound(kPsm, kChannelParams, [&open_cb_count2](auto) { open_cb_count2++; });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(open_cb_count2, 1);
  // A failed-to-open channel should not invoke the close callback.
  EXPECT_EQ(close_cb_count2, 0);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

TEST_F(BrEdrDynamicChannelTest,
       PeerConnectionResponseWithSameRemoteChannelIdAsPeerPendingConnectionResponseSucceeds) {
  const auto kOkPendingConnRsp = MakeConnectionResponseWithResultPending(kLocalCId, kRemoteCId);
  auto conn_rsp_id =
      EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                          {SignalingChannel::Status::kSuccess, kOkPendingConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
      EXPECT_TRUE(chan->IsConnected());
      EXPECT_EQ(kLocalCId, chan->local_cid());
      EXPECT_EQ(kRemoteCId, chan->remote_cid());
    }
    open_cb_count++;
  };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto) { close_cb_count++; });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(sig()->ReceiveResponses(
      conn_rsp_id, {{SignalingChannel::Status::kSuccess, kOkConnRsp.view()}}));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(open_cb_count, 1);
  EXPECT_EQ(close_cb_count, 0);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

TEST_F(BrEdrDynamicChannelTest, ChannelDeletedBeforeConnectionResponse) {
  auto conn_id = EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view());

  // Build channel and operate it directly to be able to delete it.
  auto channel =
      BrEdrDynamicChannel::MakeOutbound(registry(), sig(), kPsm, kLocalCId, kChannelParams, false);
  ASSERT_TRUE(channel);

  int open_result_cb_count = 0;
  channel->Open([&open_result_cb_count] { open_result_cb_count++; });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  channel = nullptr;
  RETURN_IF_FATAL(sig()->ReceiveResponses(
      conn_id, {{SignalingChannel::Status::kSuccess, kOutboundEmptyPendingConfigRsp.view()}}));

  EXPECT_EQ(0, open_result_cb_count);

  // No disconnection transaction expected because the channel isn't actually owned by the registry.
}

TEST_F(BrEdrDynamicChannelTest, FailConnectChannel) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kRejectConnRsp.view()});

  // Build channel and operate it directly to be able to inspect it in the
  // connected but not open state.
  auto channel =
      BrEdrDynamicChannel::MakeOutbound(registry(), sig(), kPsm, kLocalCId, kChannelParams, false);
  EXPECT_FALSE(channel->IsConnected());
  EXPECT_FALSE(channel->IsOpen());
  EXPECT_EQ(kLocalCId, channel->local_cid());

  int open_result_cb_count = 0;
  auto open_result_cb = [&open_result_cb_count, &channel] {
    if (open_result_cb_count == 0) {
      EXPECT_FALSE(channel->IsConnected());
      EXPECT_FALSE(channel->IsOpen());
    }
    open_result_cb_count++;
  };
  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto) { close_cb_count++; });

  channel->Open(std::move(open_result_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_result_cb_count);
  EXPECT_FALSE(channel->IsConnected());
  EXPECT_FALSE(channel->IsOpen());
  EXPECT_EQ(kInvalidChannelId, channel->remote_cid());

  // A failed-to-open channel should not invoke the close callback.
  EXPECT_EQ(0, close_cb_count);

  // No disconnection transaction expected because the channel isn't actually
  // owned by the registry.
}

TEST_F(BrEdrDynamicChannelTest, ConnectChannelFailConfig) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kReject, kRejNotUnderstood.view()});

  // Build channel and operate it directly to be able to inspect it in the
  // connected but not open state.
  auto channel =
      BrEdrDynamicChannel::MakeOutbound(registry(), sig(), kPsm, kLocalCId, kChannelParams, false);
  EXPECT_FALSE(channel->IsConnected());
  EXPECT_FALSE(channel->IsOpen());
  EXPECT_EQ(kLocalCId, channel->local_cid());

  int open_result_cb_count = 0;
  auto open_result_cb = [&open_result_cb_count, &channel] {
    if (open_result_cb_count == 0) {
      EXPECT_TRUE(channel->IsConnected());
      EXPECT_FALSE(channel->IsOpen());
    }
    open_result_cb_count++;
  };
  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto) { close_cb_count++; });

  channel->Open(std::move(open_result_cb));
  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_TRUE(channel->IsConnected());

  // A connected channel should have a valid remote channel ID.
  EXPECT_EQ(kRemoteCId, channel->remote_cid());

  EXPECT_FALSE(channel->IsOpen());
  EXPECT_EQ(1, open_result_cb_count);

  // A failed-to-open channel should not invoke the close callback.
  EXPECT_EQ(0, close_cb_count);

  // No disconnection transaction expected because the channel isn't actually
  // owned by the registry.
}

TEST_F(BrEdrDynamicChannelTest, ConnectChannelFailInvalidResponse) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kInvalidConnRsp.view()});

  // Build channel and operate it directly to be able to inspect it in the
  // connected but not open state.
  auto channel =
      BrEdrDynamicChannel::MakeOutbound(registry(), sig(), kPsm, kLocalCId, kChannelParams, false);

  int open_result_cb_count = 0;
  auto open_result_cb = [&open_result_cb_count, &channel] {
    if (open_result_cb_count == 0) {
      EXPECT_FALSE(channel->IsConnected());
      EXPECT_FALSE(channel->IsOpen());
    }
    open_result_cb_count++;
  };
  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto) { close_cb_count++; });

  channel->Open(std::move(open_result_cb));
  RETURN_IF_FATAL(RunLoopUntilIdle());
  EXPECT_FALSE(channel->IsConnected());
  EXPECT_FALSE(channel->IsOpen());
  EXPECT_EQ(1, open_result_cb_count);
  EXPECT_EQ(0, close_cb_count);

  // No disconnection transaction expected because the channel isn't actually
  // owned by the registry.
}

TEST_F(BrEdrDynamicChannelTest, OutboundFailsAfterRtxExpiryForConnectionResponse) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kTimeOut, BufferView()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  // FakeSignalingChannel doesn't need to be clocked in order to simulate a timeout.
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OutboundFailsAfterErtxExpiryForConnectionResponse) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kPendingConnRsp.view()},
                      {SignalingChannel::Status::kTimeOut, BufferView()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  // FakeSignalingChannel doesn't need to be clocked in order to simulate a timeout.
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_cb_count);
}

// In L2CAP Test Spec v5.0.2, this is L2CAP/COS/CED/BV-08-C [Disconnect on Timeout].
TEST_F(BrEdrDynamicChannelTest, OutboundFailsAndDisconnectsAfterRtxExpiryForConfigurationResponse) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kTimeOut, BufferView()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kTimeOut, BufferView()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  // FakeSignalingChannel doesn't need to be clocked in order to simulate a timeout.
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_cb_count);
}

// Simulate a ERTX timer expiry after a Configuration Response with "Pending" status.
TEST_F(BrEdrDynamicChannelTest,
       OutboundFailsAndDisconnectsAfterErtxExpiryForConfigurationResponse) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyPendingConfigRsp.view()},
                      {SignalingChannel::Status::kTimeOut, BufferView()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kTimeOut, BufferView()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  // FakeSignalingChannel doesn't need to be clocked in order to simulate a timeout.
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OpenAndLocalCloseChannel) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
      EXPECT_TRUE(chan->IsConnected());
      EXPECT_EQ(kLocalCId, chan->local_cid());
      EXPECT_EQ(kRemoteCId, chan->remote_cid());
      EXPECT_EQ(ChannelMode::kBasic, chan->info().mode);
    }
    open_cb_count++;
  };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) {
    EXPECT_TRUE(chan);
    close_cb_count++;
  });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);

  bool channel_close_cb_called = false;
  registry()->CloseChannel(kLocalCId, [&] { channel_close_cb_called = true; });
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_cb_count);

  // Local channel closure shouldn't trigger the close callback.
  EXPECT_EQ(0, close_cb_count);

  // Callback passed to CloseChannel should be called nonetheless.
  EXPECT_TRUE(channel_close_cb_called);

  // Repeated closure of the same channel should not have any effect.
  channel_close_cb_called = false;
  registry()->CloseChannel(kLocalCId, [&] { channel_close_cb_called = true; });
  EXPECT_TRUE(channel_close_cb_called);
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OpenAndRemoteCloseChannel) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) { open_cb_count++; };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) {
    ASSERT_TRUE(chan);
    EXPECT_FALSE(chan->IsOpen());
    EXPECT_FALSE(chan->IsConnected());
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
    close_cb_count++;
  });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kDisconnectionRequest, kInboundDisconReq, kInboundDisconRsp));

  EXPECT_EQ(1, open_cb_count);

  // Remote channel closure should trigger the close callback.
  EXPECT_EQ(1, close_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OpenChannelWithPendingConn) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kPendingConnRsp.view()},
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  registry()->OpenOutbound(kPsm, kChannelParams, [&open_cb_count](auto chan) {
    open_cb_count++;
    ASSERT_TRUE(chan);
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
  });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OpenChannelMismatchConnRsp) {
  // The first Connection Response (pending) has a different ID than the final
  // Connection Response (success).
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kPendingConnRspWithId.view()},
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  registry()->OpenOutbound(kPsm, kChannelParams, [&open_cb_count](auto chan) {
    open_cb_count++;
    ASSERT_TRUE(chan);
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
  });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OpenChannelConfigPending) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kOutboundEmptyPendingConfigRsp.view()},
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  registry()->OpenOutbound(kPsm, kChannelParams, [&open_cb_count](auto chan) {
    open_cb_count++;
    ASSERT_TRUE(chan);
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
  });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, OpenChannelRemoteDisconnectWhileConfiguring) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  auto config_id = EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view());

  int open_cb_count = 0;
  registry()->OpenOutbound(kPsm, kChannelParams, [&open_cb_count](auto chan) {
    open_cb_count++;
    EXPECT_FALSE(chan);
  });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kDisconnectionRequest, kInboundDisconReq, kInboundDisconRsp));

  // Response handler should return false ("no more responses") when called, so
  // trigger single responses rather than a set of two.
  RETURN_IF_FATAL(sig()->ReceiveResponses(
      config_id, {{SignalingChannel::Status::kSuccess, kOutboundEmptyPendingConfigRsp.view()}}));
  RETURN_IF_FATAL(sig()->ReceiveResponses(
      config_id, {{SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()}}));

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, ChannelIdNotReusedUntilDisconnectionCompletes) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  auto disconn_id = EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view());

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) {
    ASSERT_TRUE(chan);
    open_cb_count++;
  };

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) {
    EXPECT_TRUE(chan);
    close_cb_count++;
  });

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Complete opening the channel.
  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, open_cb_count);
  EXPECT_EQ(0, close_cb_count);

  bool channel_close_cb_called = false;
  registry()->CloseChannel(kLocalCId, [&] { channel_close_cb_called = true; });
  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Disconnection Response hasn't been received yet so the second channel
  // should use a different channel ID.
  const StaticByteBuffer kSecondChannelConnReq(
      // PSM
      LowerBits(kPsm), UpperBits(kPsm),

      // Source CID
      LowerBits(kLocalCId + 1), UpperBits(kLocalCId + 1));

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kSecondChannelConnReq.view());
  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  // CloseChannel callback hasn't been called either.
  EXPECT_FALSE(channel_close_cb_called);

  // Complete the disconnection on the first channel.
  RETURN_IF_FATAL(sig()->ReceiveResponses(
      disconn_id, {{SignalingChannel::Status::kSuccess, kDisconRsp.view()}}));

  EXPECT_TRUE(channel_close_cb_called);

  // Now the first channel ID gets reused.
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view());
  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));
}

// DisconnectDoneCallback is load-bearing as a signal for the registry to delete a channel's state
// and recycle its channel ID, so test that it's called even when the peer doesn't actually send a
// Disconnect Response.
TEST_F(BrEdrDynamicChannelTest, DisconnectDoneCallbackCalledAfterDisconnectResponseTimeOut) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  // Build channel and operate it directly to be able to disconnect it.
  auto channel =
      BrEdrDynamicChannel::MakeOutbound(registry(), sig(), kPsm, kLocalCId, kChannelParams, false);
  ASSERT_TRUE(channel);
  channel->Open([] {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_TRUE(channel->IsConnected());

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kTimeOut, BufferView()});

  bool disconnect_done_cb_called = false;
  channel->Disconnect([&disconnect_done_cb_called] { disconnect_done_cb_called = true; });
  EXPECT_FALSE(channel->IsConnected());

  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_TRUE(disconnect_done_cb_called);
}

TEST_F(BrEdrDynamicChannelTest, OpenChannelConfigWrongId) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kUnknownIdConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  registry()->OpenOutbound(kPsm, kChannelParams, [&open_cb_count](auto chan) {
    open_cb_count++;
    EXPECT_FALSE(chan);
  });

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(sig()->ReceiveExpectRejectInvalidChannelId(
      kConfigurationRequest, kInboundConfigReq, kLocalCId, kInvalidChannelId));

  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, InboundConnectionOk) {
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  DynamicChannelCallback open_cb = [&open_cb_count](auto chan) {
    open_cb_count++;
    ASSERT_TRUE(chan);
    EXPECT_EQ(kPsm, chan->psm());
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
  };

  int service_request_cb_count = 0;
  auto service_request_cb =
      [&service_request_cb_count, open_cb = std::move(open_cb)](
          PSM psm) mutable -> std::optional<DynamicChannelRegistry::ServiceInfo> {
    service_request_cb_count++;
    EXPECT_EQ(kPsm, psm);
    if (psm == kPsm) {
      return DynamicChannelRegistry::ServiceInfo(kChannelParams, open_cb.share());
    }
    return std::nullopt;
  };

  set_service_request_cb(std::move(service_request_cb));

  int close_cb_count = 0;
  set_channel_close_cb([&close_cb_count](auto chan) { close_cb_count++; });

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConnectionRequest, kInboundConnReq, kInboundOkConnRsp));
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_EQ(1, service_request_cb_count);
  EXPECT_EQ(0, open_cb_count);

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1, service_request_cb_count);
  EXPECT_EQ(1, open_cb_count);

  registry()->CloseChannel(kLocalCId, [] {});
  EXPECT_EQ(0, close_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, InboundConnectionRemoteDisconnectWhileConfiguring) {
  auto config_id = EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view());

  int open_cb_count = 0;
  DynamicChannelCallback open_cb = [&open_cb_count](auto chan) {
    open_cb_count++;
    FAIL() << "Failed-to-open inbound channels shouldn't trip open callback";
  };

  int service_request_cb_count = 0;
  auto service_request_cb =
      [&service_request_cb_count, open_cb = std::move(open_cb)](
          PSM psm) mutable -> std::optional<DynamicChannelRegistry::ServiceInfo> {
    service_request_cb_count++;
    EXPECT_EQ(kPsm, psm);
    if (psm == kPsm) {
      return DynamicChannelRegistry::ServiceInfo(kChannelParams, open_cb.share());
    }
    return std::nullopt;
  };

  set_service_request_cb(std::move(service_request_cb));

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConnectionRequest, kInboundConnReq, kInboundOkConnRsp));
  RunLoopUntilIdle();

  EXPECT_EQ(1, service_request_cb_count);
  EXPECT_EQ(0, open_cb_count);

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));
  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kDisconnectionRequest, kInboundDisconReq, kInboundDisconRsp));

  // Drop response received after the channel is disconnected.
  RETURN_IF_FATAL(sig()->ReceiveResponses(
      config_id, {{SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()}}));

  EXPECT_EQ(1, service_request_cb_count);

  // Channel that failed to open shouldn't have triggered channel open callback.
  EXPECT_EQ(0, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, InboundConnectionInvalidPsm) {
  auto service_request_cb = [](PSM psm) -> std::optional<DynamicChannelRegistry::ServiceInfo> {
    // Write user code that accepts the invalid PSM, but control flow may not
    // reach here.
    EXPECT_EQ(kInvalidPsm, psm);
    if (psm == kInvalidPsm) {
      return DynamicChannelRegistry::ServiceInfo(
          kChannelParams, [](auto /*unused*/) { FAIL() << "Channel should fail to open for PSM"; });
    }
    return std::nullopt;
  };

  set_service_request_cb(std::move(service_request_cb));

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConnectionRequest, kInboundInvalidPsmConnReq, kInboundBadPsmConnRsp));
  RunLoopUntilIdle();
}

TEST_F(BrEdrDynamicChannelTest, InboundConnectionUnsupportedPsm) {
  int service_request_cb_count = 0;
  auto service_request_cb =
      [&service_request_cb_count](PSM psm) -> std::optional<DynamicChannelRegistry::ServiceInfo> {
    service_request_cb_count++;
    EXPECT_EQ(kPsm, psm);

    // Reject the service request.
    return std::nullopt;
  };

  set_service_request_cb(std::move(service_request_cb));

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConnectionRequest, kInboundConnReq, kInboundBadPsmConnRsp));
  RunLoopUntilIdle();

  EXPECT_EQ(1, service_request_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, InboundConnectionInvalidSrcCId) {
  auto service_request_cb = [](PSM psm) -> std::optional<DynamicChannelRegistry::ServiceInfo> {
    // Control flow may not reach here.
    EXPECT_EQ(kPsm, psm);
    if (psm == kPsm) {
      return DynamicChannelRegistry::ServiceInfo(kChannelParams, [](auto /*unused*/) {
        FAIL() << "Channel from src_cid should fail to open";
      });
    }
    return std::nullopt;
  };

  set_service_request_cb(std::move(service_request_cb));

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConnectionRequest, kInboundBadCIdConnReq, kInboundBadCIdConnRsp));
  RunLoopUntilIdle();
}

TEST_F(BrEdrDynamicChannelTest, RejectConfigReqWithUnknownOptions) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  size_t open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) { open_cb_count++; };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  const StaticByteBuffer kInboundConfigReqUnknownOption(
      // Destination CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),

      // Flags
      0x00, 0x00,

      // Unknown Option: Type, Length, Data
      0x70, 0x01, 0x02);

  const StaticByteBuffer kOutboundConfigRspUnknownOption(
      // Source CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),

      // Flags
      0x00, 0x00,

      // Result (Failure - unknown options)
      0x03, 0x00,

      // Unknown Option: Type, Length, Data
      0x70, 0x01, 0x02);

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqUnknownOption,
                                       kOutboundConfigRspUnknownOption));

  EXPECT_EQ(0u, open_cb_count);

  RunLoopUntilIdle();

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

TEST_F(BrEdrDynamicChannelTest, ClampErtmChannelInfoMaxTxSduSizeToMaxPduPayloadSize) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  const auto kPeerMps = 1024;
  const auto kPeerMtu = kPeerMps + 1;
  bool channel_opened = false;
  auto open_cb = [&](auto chan) {
    channel_opened = true;
    ASSERT_TRUE(chan);
    EXPECT_TRUE(chan->IsOpen());

    // Note that max SDU size is the peer's MPS because it's smaller than its MTU.
    EXPECT_EQ(kPeerMps, chan->info().max_tx_sdu_size);
  };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  const auto kInboundConfigReq =
      MakeConfigReqWithMtuAndRfc(kLocalCId, kPeerMtu, ChannelMode::kEnhancedRetransmission,
                                 kErtmNFramesInTxWindow, kErtmMaxTransmissions, 0, 0, kPeerMps);
  const auto kOutboundConfigRsp = MakeConfigRspWithMtuAndRfc(
      kRemoteCId, ConfigurationResult::kSuccess, ChannelMode::kEnhancedRetransmission, kPeerMtu,
      kErtmNFramesInTxWindow, kErtmMaxTransmissions, 2000, 12000, kPeerMps);

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundConfigRsp));

  EXPECT_TRUE(channel_opened);
}

struct ReceiveMtuTestParams {
  std::optional<uint16_t> request_mtu;
  uint16_t response_mtu;
  ConfigurationResult response_status;
};
class ReceivedMtuTest : public BrEdrDynamicChannelTest,
                        public ::testing::WithParamInterface<ReceiveMtuTestParams> {};

TEST_P(ReceivedMtuTest, ResponseMtuAndStatus) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  bool channel_opened = false;
  auto open_cb = [&](auto chan) {
    channel_opened = true;
    ASSERT_TRUE(chan);
    EXPECT_TRUE(chan->IsOpen());
    EXPECT_EQ(chan->info().max_tx_sdu_size, GetParam().response_mtu);
  };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  const auto kOutboundConfigRsp =
      MakeConfigRspWithMtu(kRemoteCId, GetParam().response_mtu, GetParam().response_status);

  if (GetParam().request_mtu) {
    RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest,
                                         MakeConfigReqWithMtu(kLocalCId, *GetParam().request_mtu),
                                         kOutboundConfigRsp));
  } else {
    RETURN_IF_FATAL(
        sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundConfigRsp));
  }

  EXPECT_EQ(GetParam().response_status == ConfigurationResult::kSuccess, channel_opened);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

INSTANTIATE_TEST_SUITE_P(
    BrEdrDynamicChannelTest, ReceivedMtuTest,
    ::testing::Values(
        ReceiveMtuTestParams{std::nullopt, kDefaultMTU, ConfigurationResult::kSuccess},
        ReceiveMtuTestParams{kMinACLMTU, kMinACLMTU, ConfigurationResult::kSuccess},
        ReceiveMtuTestParams{kMinACLMTU - 1, kMinACLMTU,
                             ConfigurationResult::kUnacceptableParameters},
        ReceiveMtuTestParams{kDefaultMTU + 1, kDefaultMTU + 1, ConfigurationResult::kSuccess}));

class ConfigRspWithMtuTest
    : public BrEdrDynamicChannelTest,
      public ::testing::WithParamInterface<std::optional<uint16_t> /*response mtu*/> {};

TEST_P(ConfigRspWithMtuTest, ConfiguredLocalMtu) {
  const auto kExpectedConfiguredLocalMtu = GetParam() ? *GetParam() : kMaxMTU;

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});

  const auto kInboundConfigRspWithParamMtu =
      MakeConfigRspWithMtu(kLocalCId, GetParam() ? *GetParam() : 0);
  if (GetParam()) {
    EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                        {SignalingChannel::Status::kSuccess, kInboundConfigRspWithParamMtu.view()});
  } else {
    EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                        {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  }

  size_t open_cb_count = 0;
  auto open_cb = [&](auto chan) {
    EXPECT_TRUE(chan->IsOpen());
    EXPECT_EQ(kExpectedConfiguredLocalMtu, chan->info().max_rx_sdu_size);
    open_cb_count++;
  };
  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1u, open_cb_count);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

TEST_P(ConfigRspWithMtuTest, ConfiguredLocalMtuWithPendingRsp) {
  const auto kExpectedConfiguredLocalMtu = GetParam() ? *GetParam() : kMaxMTU;

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});

  const auto kInboundPendingConfigRspWithMtu =
      MakeConfigRspWithMtu(kLocalCId, GetParam() ? *GetParam() : 0, ConfigurationResult::kPending);
  if (GetParam()) {
    EXPECT_OUTBOUND_REQ(
        *sig(), kConfigurationRequest, kOutboundConfigReq.view(),
        {SignalingChannel::Status::kSuccess, kInboundPendingConfigRspWithMtu.view()},
        {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  } else {
    EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                        {SignalingChannel::Status::kSuccess, kInboundEmptyPendingConfigRsp.view()},
                        {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  }

  size_t open_cb_count = 0;
  auto open_cb = [&](auto chan) {
    EXPECT_TRUE(chan->IsOpen());
    EXPECT_EQ(kExpectedConfiguredLocalMtu, chan->info().max_rx_sdu_size);
    open_cb_count++;
  };
  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  EXPECT_EQ(1u, open_cb_count);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

INSTANTIATE_TEST_SUITE_P(BrEdrDynamicChannelTest, ConfigRspWithMtuTest,
                         ::testing::Values(std::nullopt, kMinACLMTU));

TEST_F(BrEdrDynamicChannelTest, RespondsToInboundExtendedFeaturesRequest) {
  const auto kExpectedExtendedFeatures =
      kExtendedFeaturesBitFixedChannels | kExtendedFeaturesBitEnhancedRetransmission;
  const auto kExpectedExtendedFeaturesInfoRsp =
      MakeExtendedFeaturesInfoRsp(InformationResult::kSuccess, kExpectedExtendedFeatures);
  sig()->ReceiveExpect(kInformationRequest, kExtendedFeaturesInfoReq,
                       kExpectedExtendedFeaturesInfoRsp);
}

TEST_F(BrEdrDynamicChannelTest, ExtendedFeaturesResponseSaved) {
  const auto kExpectedExtendedFeatures =
      kExtendedFeaturesBitFixedChannels | kExtendedFeaturesBitEnhancedRetransmission;
  const auto kInfoRsp =
      MakeExtendedFeaturesInfoRsp(InformationResult::kSuccess, kExpectedExtendedFeatures);

  EXPECT_FALSE(registry()->extended_features());

  sig()->ReceiveResponses(ext_info_transaction_id(),
                          {{SignalingChannel::Status::kSuccess, kInfoRsp.view()}});
  EXPECT_TRUE(registry()->extended_features());
  EXPECT_EQ(kExpectedExtendedFeatures, *registry()->extended_features());
}

TEST_F(BrEdrDynamicChannelTest, ExtendedFeaturesResponseInvalidFailureResult) {
  constexpr auto kResult = static_cast<InformationResult>(0xFFFF);
  const auto kInfoRsp = MakeExtendedFeaturesInfoRsp(kResult);

  EXPECT_FALSE(registry()->extended_features());

  sig()->ReceiveResponses(ext_info_transaction_id(),
                          {{SignalingChannel::Status::kSuccess, kInfoRsp.view()}});
  EXPECT_FALSE(registry()->extended_features());
}

class InformationResultTest : public BrEdrDynamicChannelTest,
                              public ::testing::WithParamInterface<InformationResult> {};

TEST_P(InformationResultTest,
       ERTMChannelWaitsForExtendedFeaturesResultBeforeFallingBackToBasicModeAndStartingConfigFlow) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});

  size_t open_cb_count = 0;
  auto open_cb = [&open_cb_count](auto chan) { open_cb_count++; };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  // Config request should not be sent.
  RETURN_IF_FATAL(RunLoopUntilIdle());

  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});

  const auto kExtendedFeaturesInfoRsp = MakeExtendedFeaturesInfoRsp(GetParam());
  sig()->ReceiveResponses(ext_info_transaction_id(),
                          {{SignalingChannel::Status::kSuccess, kExtendedFeaturesInfoRsp.view()}});

  RunLoopUntilIdle();

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  // Config should have been sent, so channel should be open.
  EXPECT_EQ(1u, open_cb_count);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

INSTANTIATE_TEST_SUITE_P(BrEdrDynamicChannelTest, InformationResultTest,
                         ::testing::Values(InformationResult::kSuccess,
                                           InformationResult::kNotSupported,
                                           static_cast<InformationResult>(0xFFFF)));

TEST_F(BrEdrDynamicChannelTest, ERTChannelDoesNotSendConfigReqBeforeConnRspReceived) {
  auto conn_id = EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(), {});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Channel will be notified that extended features received.
  sig()->ReceiveResponses(ext_info_transaction_id(),
                          {{SignalingChannel::Status::kSuccess, kExtendedFeaturesInfoRsp.view()}});

  // Config request should not be sent before connection response received.
  RunLoopUntilIdle();

  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  sig()->ReceiveResponses(conn_id, {{SignalingChannel::Status::kSuccess, kOkConnRsp.view()}});
  RunLoopUntilIdle();

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

TEST_F(BrEdrDynamicChannelTest, SendAndReceiveERTMConfigReq) {
  constexpr uint16_t kPreferredMtu = kDefaultMTU + 1;
  const auto kExpectedOutboundConfigReq = MakeConfigReqWithMtuAndRfc(
      kRemoteCId, kPreferredMtu, ChannelMode::kEnhancedRetransmission, kErtmMaxUnackedInboundFrames,
      kErtmMaxInboundRetransmissions, 0, 0, kMaxInboundPduPayloadSize);

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kExpectedOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;

  auto open_cb = [kPreferredMtu, &open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());

      // Check values of ChannelInfo fields.
      EXPECT_EQ(ChannelMode::kEnhancedRetransmission, chan->info().mode);

      // Receive capability even under ERTM is based on MTU option, not on the MPS in R&FC option.
      EXPECT_EQ(kPreferredMtu, chan->info().max_rx_sdu_size);

      // Inbound request has no MTU option, so the peer's receive capability is the default.
      EXPECT_EQ(kDefaultMTU, chan->info().max_tx_sdu_size);

      // These values should match the contents of kInboundConfigReqWithERTM.
      EXPECT_EQ(kErtmNFramesInTxWindow, chan->info().n_frames_in_tx_window);
      EXPECT_EQ(kErtmMaxTransmissions, chan->info().max_transmissions);
      EXPECT_EQ(kMaxTxPduPayloadSize, chan->info().max_tx_pdu_payload_size);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm,
                           {ChannelMode::kEnhancedRetransmission, kPreferredMtu, std::nullopt},
                           std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundOkConfigRspWithErtm));

  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

// When the peer rejects ERTM with the result Unacceptable Parameters and the R&FC
// option specifying basic mode, the local device should send a new request with basic mode.
// When the peer then requests basic mode, it should be accepted.
// PTS: L2CAP/CMC/BV-03-C
TEST_F(BrEdrDynamicChannelTest, PeerRejectsERTM) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(
      *sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
      {SignalingChannel::Status::kSuccess, kInboundUnacceptableParamsWithRfcBasicConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;

  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

// Local device that prefers ERTM will renegotiate channel mode to basic mode after peer negotiates
// basic mode and rejects ERTM.
// PTS: L2CAP/CMC/BV-07-C
TEST_F(BrEdrDynamicChannelTest, RenegotiateChannelModeAfterPeerRequestsBasicModeAndRejectsERTM) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  auto config_req_id =
      EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view());

  int open_cb_count = 0;

  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_TRUE(chan->IsOpen());
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  RunLoopUntilIdle();

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});
  RunLoopUntilIdle();

  // Peer requests basic mode.
  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));
  RunLoopUntilIdle();

  // New config request requesting basic mode should be sent in response to unacceptable params
  // response.
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  sig()->ReceiveResponses(config_req_id,
                          {{SignalingChannel::Status::kSuccess,
                            kInboundUnacceptableParamsWithRfcBasicConfigRsp.view()}});

  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);

  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});
}

// The local device should configure basic mode if peer does not indicate support for ERTM when it
// is preferred.
// PTS: L2CAP/CMC/BV-10-C
TEST_F(BrEdrDynamicChannelTest, PreferredModeIsERTMButERTMIsNotInPeerFeatureMask) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Receive features mask without ERTM bit set.
  sig()->ReceiveResponses(ext_info_transaction_id(),
                          {{SignalingChannel::Status::kSuccess, kExtendedFeaturesInfoRsp.view()}});
}

TEST_F(BrEdrDynamicChannelTest, RejectERTMRequestWhenPreferredModeIsBasic) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Peer requests ERTM. Local device should reject with unacceptable params.
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundUnacceptableParamsWithRfcBasicConfigRsp));
}

// Core Spec v5.1, Vol 3, Part A, Sec 5.4:
// If the mode in the remote device's negative Configuration Response does
// not match the mode in the remote device's Configuration Request then the
// local device shall disconnect the channel.
//
// Inbound config request received BEFORE outbound config request:
// <- ConfigurationRequest (with ERTM)
// -> ConfigurationResponse (Ok)
// -> ConfigurationRequest (with ERTM)
// <- ConfigurationResponse (Unacceptable, with Basic)
TEST_F(
    BrEdrDynamicChannelTest,
    DisconnectWhenInboundConfigReqReceivedBeforeOutboundConfigReqSentModeInInboundUnacceptableParamsConfigRspDoesNotMatchPeerConfigReq) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(
      *sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
      {SignalingChannel::Status::kSuccess, kInboundUnacceptableParamsWithRfcBasicConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Receive inbound config request.
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundOkConfigRspWithErtm));

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});
  // Send outbound config request.
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

// Same as above, but inbound config request received AFTER outbound configuration request:
// -> ConfigurationRequest (with ERTM)
// <- ConfigurationRequest (with ERTM)
// -> ConfigurationResponse (Ok)
// <- ConfigurationResponse (Unacceptable, with Basic)
TEST_F(
    BrEdrDynamicChannelTest,
    DisconnectWhenInboundConfigReqReceivedAfterOutboundConfigReqSentAndModeInInboundUnacceptableParamsConfigRspDoesNotMatchPeerConfigReq) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  const auto outbound_config_req_id =
      EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view());
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  ChannelParameters params;
  params.mode = ChannelMode::kEnhancedRetransmission;
  registry()->OpenOutbound(kPsm, params, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});
  // Send outbound config request.
  RunLoopUntilIdle();

  // Receive inbound config request.
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundOkConfigRspWithErtm));

  sig()->ReceiveResponses(outbound_config_req_id,
                          {{SignalingChannel::Status::kSuccess,
                            kInboundUnacceptableParamsWithRfcBasicConfigRsp.view()}});
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, DisconnectAfterReceivingTwoConfigRequestsWithoutDesiredMode) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      EXPECT_FALSE(chan);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundUnacceptableParamsWithRfcBasicConfigRsp));
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundUnacceptableParamsWithRfcBasicConfigRsp));

  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, RetryWhenPeerRejectsConfigReqWithBasicMode) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(
      *sig(), kConfigurationRequest, kOutboundConfigReq.view(),
      {SignalingChannel::Status::kSuccess, kInboundUnacceptableParamsWithRfcBasicConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&open_cb_count](const DynamicChannel* chan) { open_cb_count++; };

  registry()->OpenOutbound(kPsm, kChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  RunLoopUntilIdle();
  EXPECT_EQ(0, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest,
       SendUnacceptableParamsResponseWhenPeerRequestsUnsupportedChannelMode) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Retransmission mode is not supported.
  const auto kInboundConfigReqWithRetransmissionMode =
      MakeConfigReqWithMtuAndRfc(kLocalCId, kMaxMTU, ChannelMode::kRetransmission, 0, 0, 0, 0, 0);
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest,
                                       kInboundConfigReqWithRetransmissionMode,
                                       kOutboundUnacceptableParamsWithRfcBasicConfigRsp));
}

TEST_F(BrEdrDynamicChannelTest,
       SendUnacceptableParamsResponseWhenPeerRequestsUnsupportedChannelModeAndSupportsErtm) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(), {});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Retransmission mode is not supported.
  const auto kInboundConfigReqWithRetransmissionMode =
      MakeConfigReqWithMtuAndRfc(kLocalCId, kMaxMTU, ChannelMode::kRetransmission, 0, 0, 0, 0, 0);
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest,
                                       kInboundConfigReqWithRetransmissionMode,
                                       kOutboundUnacceptableParamsWithRfcERTMConfigRsp));
}

TEST_F(BrEdrDynamicChannelTest, SendUnacceptableParamsResponseWhenPeerRequestErtmWithZeroTxWindow) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  constexpr uint8_t kMaxTransmit = 31;
  constexpr auto kMps = kMaxTxPduPayloadSize;

  // TxWindow of zero is out of range.
  const auto kInboundConfigReqWithZeroTxWindow =
      MakeConfigReqWithMtuAndRfc(kLocalCId, kDefaultMTU, ChannelMode::kEnhancedRetransmission,
                                 /*tx_window=*/0, /*max_transmit=*/kMaxTransmit,
                                 /*retransmission_timeout=*/0, /*monitor_timeout=*/0, /*mps=*/kMps);
  const auto kOutboundConfigRsp = MakeConfigRspWithRfc(
      kRemoteCId, ConfigurationResult::kUnacceptableParameters,
      ChannelMode::kEnhancedRetransmission, /*tx_window=*/1, /*max_transmit=*/kMaxTransmit,
      /*retransmission_timeout=*/0, /*monitor_timeout=*/0, /*mps=*/kMps);
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithZeroTxWindow,
                                       kOutboundConfigRsp));
}

TEST_F(BrEdrDynamicChannelTest,
       SendUnacceptableParamsResponseWhenPeerRequestErtmWithOversizeTxWindow) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  constexpr uint8_t kMaxTransmit = 31;
  constexpr auto kMps = kMaxTxPduPayloadSize;

  // TxWindow of 200 is out of range.
  const auto kInboundConfigReqWithOversizeTxWindow =
      MakeConfigReqWithMtuAndRfc(kLocalCId, kDefaultMTU, ChannelMode::kEnhancedRetransmission,
                                 /*tx_window=*/200, /*max_transmit=*/kMaxTransmit,
                                 /*retransmission_timeout=*/0, /*monitor_timeout=*/0, /*mps=*/kMps);
  const auto kOutboundConfigRsp = MakeConfigRspWithRfc(
      kRemoteCId, ConfigurationResult::kUnacceptableParameters,
      ChannelMode::kEnhancedRetransmission, /*tx_window=*/63, /*max_transmit=*/kMaxTransmit,
      /*retransmission_timeout=*/0, /*monitor_timeout=*/0, /*mps=*/kMps);
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithOversizeTxWindow,
                                       kOutboundConfigRsp));
}

TEST_F(BrEdrDynamicChannelTest, SendUnacceptableParamsResponseWhenPeerRequestErtmWithUndersizeMps) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  registry()->OpenOutbound(kPsm, kERTMChannelParams, {});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(RunLoopUntilIdle());

  constexpr uint8_t kMaxTransmit = 31;
  constexpr uint8_t kTxWindow = kErtmMaxUnackedInboundFrames;

  // MPS of 16 would not be able to fit a 48-byte (minimum MTU) SDU without segmentation.
  const auto kInboundConfigReqWithUndersizeMps =
      MakeConfigReqWithMtuAndRfc(kLocalCId, kDefaultMTU, ChannelMode::kEnhancedRetransmission,
                                 /*tx_window=*/kTxWindow, /*max_transmit=*/kMaxTransmit,
                                 /*retransmission_timeout=*/0, /*monitor_timeout=*/0, /*mps=*/16);
  const auto kOutboundConfigRsp = MakeConfigRspWithRfc(
      kRemoteCId, ConfigurationResult::kUnacceptableParameters,
      ChannelMode::kEnhancedRetransmission, /*tx_window=*/kTxWindow, /*max_transmit=*/kMaxTransmit,
      /*retransmission_timeout=*/0, /*monitor_timeout=*/0, /*mps=*/kMinACLMTU);
  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithUndersizeMps,
                                       kOutboundConfigRsp));
}

// Local config with ERTM incorrectly accepted by peer, then peer requests basic mode which
// the local device must accept. These modes are incompatible, so the local device should
// default to Basic Mode.
TEST_F(BrEdrDynamicChannelTest, OpenBasicModeChannelAfterPeerAcceptsErtmThenPeerRequestsBasicMode) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;

  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_EQ(ChannelMode::kBasic, chan->info().mode);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  // Request ERTM.
  RunLoopUntilIdle();

  // Peer requests basic mode.
  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  // Disconnect
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

// Same as above, but the peer sends its positive response after sending its Basic Mode request.
TEST_F(BrEdrDynamicChannelTest, OpenBasicModeChannelAfterPeerRequestsBasicModeThenPeerAcceptsErtm) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;

  auto open_cb = [&open_cb_count](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_EQ(ChannelMode::kBasic, chan->info().mode);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, kERTMChannelParams, std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  // Peer requests basic mode.
  RETURN_IF_FATAL(
      sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp));

  // Local device will request ERTM.
  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});
  // Request ERTM & Disconnect
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, MtuChannelParameterSentInConfigReq) {
  constexpr uint16_t kPreferredMtu = kDefaultMTU + 1;
  const auto kExpectedOutboundConfigReq = MakeConfigReqWithMtu(kRemoteCId, kPreferredMtu);

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kExpectedOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_EQ(kPreferredMtu, chan->info().max_rx_sdu_size);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, {ChannelMode::kBasic, kPreferredMtu, std::nullopt}, open_cb);
  RunLoopUntilIdle();

  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp);
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, UseMinMtuWhenMtuChannelParameterIsBelowMin) {
  constexpr uint16_t kMtu = kMinACLMTU - 1;
  const auto kExpectedOutboundConfigReq = MakeConfigReqWithMtu(kRemoteCId, kMinACLMTU);

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kExpectedOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_EQ(kMinACLMTU, chan->info().max_rx_sdu_size);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, {ChannelMode::kBasic, kMtu, std::nullopt}, open_cb);
  RunLoopUntilIdle();

  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kOutboundOkConfigRsp);
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, UseMaxPduPayloadSizeWhenMtuChannelParameterExceedsItWithErtm) {
  constexpr uint16_t kPreferredMtu = kMaxInboundPduPayloadSize + 1;
  const auto kExpectedOutboundConfigReq =
      MakeConfigReqWithMtuAndRfc(kRemoteCId, kMaxInboundPduPayloadSize,
                                 ChannelMode::kEnhancedRetransmission, kErtmMaxUnackedInboundFrames,
                                 kErtmMaxInboundRetransmissions, 0, 0, kMaxInboundPduPayloadSize);

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kExpectedOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  int open_cb_count = 0;
  auto open_cb = [&](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_EQ(kMaxInboundPduPayloadSize, chan->info().max_rx_sdu_size);
    }
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm,
                           {ChannelMode::kEnhancedRetransmission, kPreferredMtu, std::nullopt},
                           std::move(open_cb));

  RETURN_IF_FATAL(RunLoopUntilIdle());

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});

  RETURN_IF_FATAL(sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM,
                                       kOutboundOkConfigRspWithErtm));

  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, BasicModeChannelReportsChannelInfoWithBasicModeAndSduCapacities) {
  constexpr uint16_t kPreferredMtu = kDefaultMTU + 1;
  const auto kExpectedOutboundConfigReq = MakeConfigReqWithMtu(kRemoteCId, kPreferredMtu);

  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kExpectedOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  constexpr uint16_t kPeerMtu = kDefaultMTU + 2;
  const auto kInboundConfigReq = MakeConfigReqWithMtu(kLocalCId, kPeerMtu);

  int open_cb_count = 0;
  auto open_cb = [&](const DynamicChannel* chan) {
    ASSERT_TRUE(chan);
    EXPECT_EQ(ChannelMode::kBasic, chan->info().mode);
    EXPECT_EQ(kPreferredMtu, chan->info().max_rx_sdu_size);
    EXPECT_EQ(kPeerMtu, chan->info().max_tx_sdu_size);
    open_cb_count++;
  };

  registry()->OpenOutbound(kPsm, {ChannelMode::kBasic, kPreferredMtu, std::nullopt}, open_cb);
  RunLoopUntilIdle();

  const ByteBuffer& kExpectedOutboundOkConfigRsp = MakeConfigRspWithMtu(kRemoteCId, kPeerMtu);
  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq, kExpectedOutboundOkConfigRsp);
  RunLoopUntilIdle();
  EXPECT_EQ(1, open_cb_count);
}

TEST_F(BrEdrDynamicChannelTest, Receive2ConfigReqsWithContinuationFlagInFirstReq) {
  constexpr uint16_t kTxMtu = kMinACLMTU;
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReqWithErtm.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  const auto kInboundConfigReq0 =
      MakeConfigReqWithMtu(kLocalCId, kTxMtu, kConfigurationContinuation);
  const auto kOutboundConfigRsp1 = MakeConfigRspWithMtuAndRfc(
      kRemoteCId, ConfigurationResult::kSuccess, ChannelMode::kEnhancedRetransmission, kTxMtu,
      kErtmNFramesInTxWindow, kErtmMaxTransmissions, 2000, 12000, kMaxTxPduPayloadSize);

  size_t open_cb_count = 0;
  auto open_cb = [&](const DynamicChannel* chan) {
    if (open_cb_count == 0) {
      ASSERT_TRUE(chan);
      EXPECT_EQ(kTxMtu, chan->info().max_tx_sdu_size);
      EXPECT_EQ(ChannelMode::kEnhancedRetransmission, chan->info().mode);
    }
    open_cb_count++;
  };

  sig()->ReceiveResponses(ext_info_transaction_id(), {{SignalingChannel::Status::kSuccess,
                                                       kExtendedFeaturesInfoRspWithERTM.view()}});
  registry()->OpenOutbound(kPsm, kERTMChannelParams, open_cb);
  RunLoopUntilIdle();

  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq0,
                       kOutboundEmptyContinuationConfigRsp);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, open_cb_count);

  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReqWithERTM, kOutboundConfigRsp1);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, open_cb_count);
}

// The unknown options from both configuration requests should be included when responding
// with the "unknown options" result.
TEST_F(BrEdrDynamicChannelTest,
       Receive2ConfigReqsWithContinuationFlagInFirstReqAndUnknownOptionInBothReqs) {
  EXPECT_OUTBOUND_REQ(*sig(), kConnectionRequest, kConnReq.view(),
                      {SignalingChannel::Status::kSuccess, kOkConnRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kConfigurationRequest, kOutboundConfigReq.view(),
                      {SignalingChannel::Status::kSuccess, kInboundEmptyConfigRsp.view()});
  EXPECT_OUTBOUND_REQ(*sig(), kDisconnectionRequest, kDisconReq.view(),
                      {SignalingChannel::Status::kSuccess, kDisconRsp.view()});

  constexpr uint8_t kUnknownOption0Type = 0x70;
  constexpr uint8_t kUnknownOption1Type = 0x71;
  const StaticByteBuffer kInboundConfigReq0(
      // Destination CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),
      // Flags (C = 1)
      0x01, 0x00,
      // Unknown Option
      kUnknownOption0Type, 0x01, 0x00);
  const StaticByteBuffer kInboundConfigReq1(
      // Destination CID
      LowerBits(kLocalCId), UpperBits(kLocalCId),
      // Flags (C = 0)
      0x00, 0x00,
      // Unknown Option
      kUnknownOption1Type, 0x01, 0x00);

  const StaticByteBuffer kOutboundUnknownOptionsConfigRsp(
      // Source CID
      LowerBits(kRemoteCId), UpperBits(kRemoteCId),
      // Flags (C = 0)
      0x00, 0x00,
      // Result
      LowerBits(static_cast<uint16_t>(ConfigurationResult::kUnknownOptions)),
      UpperBits(static_cast<uint16_t>(ConfigurationResult::kUnknownOptions)),
      // Unknown Options
      kUnknownOption0Type, 0x01, 0x00, kUnknownOption1Type, 0x01, 0x00);

  size_t open_cb_count = 0;
  auto open_cb = [&](const DynamicChannel* chan) { open_cb_count++; };

  registry()->OpenOutbound(kPsm, kChannelParams, open_cb);
  RunLoopUntilIdle();

  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq0,
                       kOutboundEmptyContinuationConfigRsp);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, open_cb_count);

  sig()->ReceiveExpect(kConfigurationRequest, kInboundConfigReq1, kOutboundUnknownOptionsConfigRsp);
  RunLoopUntilIdle();
  EXPECT_EQ(0u, open_cb_count);
}

}  // namespace
}  // namespace bt::l2cap::internal
