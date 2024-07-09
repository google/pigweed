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

#include "pw_bluetooth_sapphire/internal/host/l2cap/a2dp_offload_manager.h"

#include <memory>

#include "pw_bluetooth_sapphire/internal/host/common/host_error.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::l2cap {
namespace {

namespace hci_android = bt::hci_spec::vendor::android;
namespace android_hci = pw::bluetooth::vendor::android_hci;
using namespace bt::testing;

constexpr hci_spec::ConnectionHandle kTestHandle1 = 0x0001;
constexpr ChannelId kLocalId = 0x0040;
constexpr ChannelId kRemoteId = 0x9042;

A2dpOffloadManager::Configuration BuildConfiguration(
    android_hci::A2dpCodecType codec = android_hci::A2dpCodecType::SBC) {
  A2dpOffloadManager::Configuration config;
  config.codec = codec;
  config.max_latency = 0xFFFF;
  config.scms_t_enable.view().enabled().Write(
      pw::bluetooth::emboss::GenericEnableParam::DISABLE);
  config.scms_t_enable.view().header().Write(0x00);
  config.sampling_frequency = android_hci::A2dpSamplingFrequency::HZ_44100;
  config.bits_per_sample = android_hci::A2dpBitsPerSample::BITS_PER_SAMPLE_16;
  config.channel_mode = android_hci::A2dpChannelMode::MONO;
  config.encoded_audio_bit_rate = 0x0;

  switch (codec) {
    case android_hci::A2dpCodecType::SBC:
      config.sbc_configuration.view().block_length().Write(
          android_hci::SbcBlockLen::BLOCK_LEN_4);
      config.sbc_configuration.view().subbands().Write(
          android_hci::SbcSubBands::SUBBANDS_4);
      config.sbc_configuration.view().allocation_method().Write(
          android_hci::SbcAllocationMethod::SNR);
      config.sbc_configuration.view().min_bitpool_value().Write(0x00);
      config.sbc_configuration.view().max_bitpool_value().Write(0xFF);
      break;
    case android_hci::A2dpCodecType::AAC:
      config.aac_configuration.view().object_type().Write(0x00);
      config.aac_configuration.view().variable_bit_rate().Write(
          android_hci::AacEnableVariableBitRate::DISABLE);
      break;
    case android_hci::A2dpCodecType::LDAC:
      config.ldac_configuration.view().vendor_id().Write(
          hci_android::kLdacVendorId);
      config.ldac_configuration.view().codec_id().Write(
          hci_android::kLdacCodecId);
      config.ldac_configuration.view().bitrate_index().Write(
          android_hci::LdacBitrateIndex::LOW);
      config.ldac_configuration.view().ldac_channel_mode().stereo().Write(true);
      break;
    default:
      break;
  }

  return config;
}

using TestingBase = FakeDispatcherControllerTest<MockController>;

class A2dpOffloadTest : public TestingBase {
 public:
  A2dpOffloadTest() = default;
  ~A2dpOffloadTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    offload_mgr_ =
        std::make_unique<A2dpOffloadManager>(cmd_channel()->AsWeakPtr());
  }

  void TearDown() override { TestingBase::TearDown(); }

  A2dpOffloadManager* offload_mgr() const { return offload_mgr_.get(); }

 private:
  std::unique_ptr<A2dpOffloadManager> offload_mgr_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(A2dpOffloadTest);
};

class StartA2dpOffloadTest
    : public A2dpOffloadTest,
      public ::testing::WithParamInterface<android_hci::A2dpCodecType> {};

TEST_P(StartA2dpOffloadTest, StartA2dpOffloadSuccess) {
  const android_hci::A2dpCodecType codec = GetParam();
  A2dpOffloadManager::Configuration config = BuildConfiguration(codec);

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());
}

const std::vector<android_hci::A2dpCodecType> kA2dpCodecTypeParams = {
    android_hci::A2dpCodecType::SBC,
    android_hci::A2dpCodecType::AAC,
    android_hci::A2dpCodecType::LDAC};
INSTANTIATE_TEST_SUITE_P(ChannelManagerTest,
                         StartA2dpOffloadTest,
                         ::testing::ValuesIn(kA2dpCodecTypeParams));

TEST_F(A2dpOffloadTest, StartA2dpOffloadInvalidConfiguration) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete = CommandCompletePacket(
      hci_android::kA2dpOffloadCommand,
      pw::bluetooth::emboss::StatusCode::INVALID_HCI_COMMAND_PARAMETERS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::
                               INVALID_HCI_COMMAND_PARAMETERS),
                  res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_error());
}

TEST_F(A2dpOffloadTest, StartAndStopA2dpOffloadSuccess) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  EXPECT_CMD_PACKET_OUT(
      test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
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

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  start_result.reset();
  offload_mgr()->StartA2dpOffload(config,
                                  kLocalId,
                                  kRemoteId,
                                  kTestHandle1,
                                  kMaxMTU,
                                  [&start_result](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress),
                                              res);
                                    start_result = res;
                                  });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_error());
}

TEST_F(A2dpOffloadTest, StartA2dpOffloadStillStarting) {
  A2dpOffloadManager::Configuration config = BuildConfiguration();

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  EXPECT_FALSE(start_result.has_value());

  offload_mgr()->StartA2dpOffload(config,
                                  kLocalId,
                                  kRemoteId,
                                  kTestHandle1,
                                  kMaxMTU,
                                  [&start_result](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress),
                                              res);
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

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  EXPECT_CMD_PACKET_OUT(
      test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        stop_result = res;
      });
  EXPECT_FALSE(stop_result.has_value());

  start_result.reset();
  offload_mgr()->StartA2dpOffload(config,
                                  kLocalId,
                                  kRemoteId,
                                  kTestHandle1,
                                  kMaxMTU,
                                  [&start_result](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress),
                                              res);
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

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  EXPECT_FALSE(start_result.has_value());

  EXPECT_CMD_PACKET_OUT(
      test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
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

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  EXPECT_CMD_PACKET_OUT(
      test_device(), StopA2dpOffloadRequest(), &command_complete);

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        stop_result = res;
      });
  EXPECT_FALSE(stop_result.has_value());

  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
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
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
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

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result_0;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result_0](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result_0 = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result_0.has_value());
  EXPECT_TRUE(start_result_0->is_ok());

  std::optional<hci::Result<>> start_result_1;
  offload_mgr()->StartA2dpOffload(config,
                                  kLocalId + 1,
                                  kRemoteId + 1,
                                  kTestHandle1,
                                  kMaxMTU,
                                  [&start_result_1](auto res) {
                                    EXPECT_EQ(ToResult(HostError::kInProgress),
                                              res);
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

  const auto command_complete =
      CommandCompletePacket(hci_android::kA2dpOffloadCommand,
                            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_CMD_PACKET_OUT(
      test_device(),
      StartA2dpOffloadRequest(config, kTestHandle1, kRemoteId, kMaxMTU),
      &command_complete);

  std::optional<hci::Result<>> start_result;
  offload_mgr()->StartA2dpOffload(
      config,
      kLocalId,
      kRemoteId,
      kTestHandle1,
      kMaxMTU,
      [&start_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        start_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  EXPECT_TRUE(test_device()->AllExpectedCommandPacketsSent());
  ASSERT_TRUE(start_result.has_value());
  EXPECT_TRUE(start_result->is_ok());

  std::optional<hci::Result<>> stop_result;
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId + 1, kTestHandle1 + 1, [&stop_result](auto res) {
        EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::SUCCESS), res);
        stop_result = res;
      });
  RunUntilIdle();
  EXPECT_TRUE(offload_mgr()->IsChannelOffloaded(kLocalId, kTestHandle1));
  ASSERT_TRUE(stop_result.has_value());
  EXPECT_TRUE(stop_result->is_ok());

  EXPECT_CMD_PACKET_OUT(
      test_device(), StopA2dpOffloadRequest(), &command_complete);

  // Can still stop it from the correct one.
  stop_result = std::nullopt;
  offload_mgr()->RequestStopA2dpOffload(
      kLocalId, kTestHandle1, [&stop_result](auto res) {
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
