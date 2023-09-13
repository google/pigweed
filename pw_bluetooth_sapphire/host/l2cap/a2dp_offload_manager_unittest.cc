// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/a2dp_offload_manager.h"

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/host_error.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bt::l2cap {
namespace {

namespace hci_android = bt::hci_spec::vendor::android;
using namespace bt::testing;

constexpr hci_spec::ConnectionHandle kTestHandle1 = 0x0001;
constexpr ChannelId kLocalId = 0x0040;
constexpr ChannelId kRemoteId = 0x9042;

A2dpOffloadManager::Configuration BuildConfiguration(
    hci_android::A2dpCodecType codec = hci_android::A2dpCodecType::kSbc) {
  hci_android::A2dpScmsTEnable scms_t_enable;
  scms_t_enable.enabled = pw::bluetooth::emboss::GenericEnableParam::DISABLE;
  scms_t_enable.header = 0x00;

  hci_android::A2dpOffloadCodecInformation codec_information;
  switch (codec) {
    case hci_android::A2dpCodecType::kSbc:
      codec_information.sbc.blocklen_subbands_alloc_method = 0x00;
      codec_information.sbc.min_bitpool_value = 0x00;
      codec_information.sbc.max_bitpool_value = 0xFF;
      memset(codec_information.sbc.reserved, 0, sizeof(codec_information.sbc.reserved));
      break;
    case hci_android::A2dpCodecType::kAac:
      codec_information.aac.object_type = 0x00;
      codec_information.aac.variable_bit_rate = hci_android::A2dpAacEnableVariableBitRate::kDisable;
      memset(codec_information.aac.reserved, 0, sizeof(codec_information.aac.reserved));
      break;
    case hci_android::A2dpCodecType::kLdac:
      codec_information.ldac.vendor_id = 0x0000012D;
      codec_information.ldac.codec_id = 0x00AA;
      codec_information.ldac.bitrate_index = hci_android::A2dpBitrateIndex::kLow;
      codec_information.ldac.ldac_channel_mode = hci_android::A2dpLdacChannelMode::kStereo;
      memset(codec_information.ldac.reserved, 0, sizeof(codec_information.ldac.reserved));
      break;
    default:
      memset(codec_information.aptx.reserved, 0, sizeof(codec_information.aptx.reserved));
      break;
  }

  A2dpOffloadManager::Configuration config;
  config.codec = codec;
  config.max_latency = 0xFFFF;
  config.scms_t_enable = scms_t_enable;
  config.sampling_frequency = hci_android::A2dpSamplingFrequency::k44100Hz;
  config.bits_per_sample = hci_android::A2dpBitsPerSample::k16BitsPerSample;
  config.channel_mode = hci_android::A2dpChannelMode::kMono;
  config.encoded_audio_bit_rate = 0x0;
  config.codec_information = codec_information;

  return config;
}

using TestingBase = FakeDispatcherControllerTest<MockController>;

class A2dpOffloadTest : public TestingBase {
 public:
  A2dpOffloadTest() = default;
  ~A2dpOffloadTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    offload_mgr_ = std::make_unique<A2dpOffloadManager>(cmd_channel()->AsWeakPtr());
  }

  void TearDown() override { TestingBase::TearDown(); }

  A2dpOffloadManager* offload_mgr() const { return offload_mgr_.get(); }

 private:
  std::unique_ptr<A2dpOffloadManager> offload_mgr_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(A2dpOffloadTest);
};

class StartA2dpOffloadTest : public A2dpOffloadTest,
                             public ::testing::WithParamInterface<hci_android::A2dpCodecType> {};

TEST_P(StartA2dpOffloadTest, StartA2dpOffloadSuccess) {
  const hci_android::A2dpCodecType codec = GetParam();
  A2dpOffloadManager::Configuration config = BuildConfiguration(codec);

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());
}

const std::vector<hci_android::A2dpCodecType> kA2dpCodecTypeParams = {
    hci_android::A2dpCodecType::kSbc, hci_android::A2dpCodecType::kAac,
    hci_android::A2dpCodecType::kLdac, hci_android::A2dpCodecType::kAptx};
INSTANTIATE_TEST_SUITE_P(ChannelManagerTest, StartA2dpOffloadTest,
                         ::testing::ValuesIn(kA2dpCodecTypeParams));

TEST_F(A2dpOffloadTest, StartA2dpOffloadInvalidConfiguration) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_error());
}

TEST_F(A2dpOffloadTest, StartAndStopA2dpOffloadSuccess) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  EXPECT_CMD_PACKET_OUT(test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  RunUntilIdle();
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());
}

TEST_F(A2dpOffloadTest, StartA2dpOffloadAlreadyStarted) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  start_result.reset();
  offload_mgr()->StartA2dpOffload(config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU,
                                  [&start_result](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress), res);
                                    start_result = res;
                                  });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_error());
}

TEST_F(A2dpOffloadTest, StartA2dpOffloadStillStarting) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  EXPECT_FALSE(start_result.has_value());

  offload_mgr()->StartA2dpOffload(config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU,
                                  [&start_result](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress), res);
                                    start_result = res;
                                  });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());
}

TEST_F(A2dpOffloadTest, StartA2dpOffloadStillStopping) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  EXPECT_CMD_PACKET_OUT(test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  EXPECT_FALSE(stop_result.has_value());

  start_result.reset();
  offload_mgr()->StartA2dpOffload(config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU,
                                  [&start_result](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress), res);
                                    start_result = res;
                                  });
  RunUntilIdle();
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_error());
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());
}

TEST_F(A2dpOffloadTest, StopA2dpOffloadStillStarting) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  EXPECT_FALSE(start_result.has_value());

  EXPECT_CMD_PACKET_OUT(test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  RunUntilIdle();
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());
}

TEST_F(A2dpOffloadTest, StopA2dpOffloadStillStopping) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  EXPECT_CMD_PACKET_OUT(test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  EXPECT_FALSE(stop_result.has_value());

  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(HostError::kInProgress), res);
    stop_result = res;
  });
  RunUntilIdle();
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());
}

TEST_F(A2dpOffloadTest, StopA2dpOffloadAlreadyStopped) {
  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  RunUntilIdle();
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());
}

TEST_F(A2dpOffloadTest, A2dpOffloadOnlyOneChannel) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result_0;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result_0](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result_0 = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result_0.has_value());
  EXPECT_TRUE(start_result_0->is_ok());

  std::optional<hci::Result<>> start_result_1;
  offload_mgr()->StartA2dpOffload(config, kLocalId + 1, kRemoteId + 1, kTestHandle1, kMaxMTU,
                                  [&start_result_1](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress), res);
                                    start_result_1 = res;
                                  });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId + 1, kTestHandle1));
  ASSERT_TRUE(start_result_1.has_value());
  EXPECT_TRUE(start_result_1->is_error());
}

TEST_F(A2dpOffloadTest, DifferentChannelCannotStopA2dpOffloading) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                                                      pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(test_device(),
                        StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
                        &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config, kLocalId, kRemoteId, kTestHandle1, kMaxMTU, [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(kLocalId + 1, kTestHandle1 + 1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());

  EXPECT_CMD_PACKET_OUT(test_device(), StopA2dpOffloadRequest(), &command_complete);

  // Can still stop it from the correct one.
  stop_result = std::nullopt;
  offload_mgr()->RequestStopA2dpOffload(kLocalId, kTestHandle1, [&stop_result](auto res) {
    EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
    stop_result = res;
  });
  RunUntilIdle();
  EXPECT_FALSE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());
}

}  // namespace
}  // namespace bt::l2cap
