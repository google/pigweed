// Copyright 2024 The Pigweed Authors
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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream.h"

#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::iso {

constexpr hci_spec::CigIdentifier kCigId = 0x22;
constexpr hci_spec::CisIdentifier kCisId = 0x42;

constexpr hci_spec::ConnectionHandle kCisHandleId = 0x59e;
using MockControllerTestBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;

class IsoStreamTest : public MockControllerTestBase {
 public:
  IsoStreamTest() = default;
  ~IsoStreamTest() override = default;

  void SetUp() override {
    MockControllerTestBase::SetUp(
        pw::bluetooth::Controller::FeaturesBits::kHciIso);
    hci::DataBufferInfo iso_buffer_info(/*max_data_length=*/100,
                                        /*max_num_packets=*/5);
    transport()->InitializeIsoDataChannel(iso_buffer_info);
    iso_stream_ = IsoStream::Create(
        kCigId,
        kCisId,
        kCisHandleId,
        /*on_established_cb=*/
        [this](pw::bluetooth::emboss::StatusCode status,
               std::optional<WeakSelf<IsoStream>::WeakPtr>,
               const std::optional<CisEstablishedParameters>& parameters) {
          ASSERT_FALSE(establishment_status_.has_value());
          establishment_status_ = status;
          established_parameters_ = parameters;
        },
        transport()->command_channel()->AsWeakPtr(),
        /*on_closed_cb=*/
        [this]() {
          ASSERT_FALSE(closed_);
          closed_ = true;
        });
  }

 protected:
  // Send an HCI_LE_CIS_Established event with the provided status
  void EstablishCis(pw::bluetooth::emboss::StatusCode status);

  // Call IsoStream::SetupDataPath().
  // |cmd_complete_status| is nullopt if we do not expect an
  // LE_Setup_ISO_Data_Path command to be generated, otherwise it should be set
  // to the status code we want to generate in the response frame.
  // |expected_cb_result| should be set to the expected result of the callback
  // from IsoStream::SetupDataPath.
  void SetupDataPath(pw::bluetooth::emboss::DataPathDirection direction,
                     const std::optional<std::vector<uint8_t>>& codec_config,
                     const std::optional<pw::bluetooth::emboss::StatusCode>&
                         cmd_complete_status,
                     iso::IsoStream::SetupDataPathError expected_cb_result,
                     bool generate_mismatched_cid = false);

  IsoStream* iso_stream() { return iso_stream_.get(); }

  std::optional<pw::bluetooth::emboss::StatusCode> establishment_status() {
    return establishment_status_;
  }

  std::optional<CisEstablishedParameters> established_parameters() {
    return established_parameters_;
  }

  bool closed() { return closed_; }

 private:
  std::unique_ptr<IsoStream> iso_stream_;
  std::optional<pw::bluetooth::emboss::StatusCode> establishment_status_;
  std::optional<CisEstablishedParameters> established_parameters_;
  bool closed_ = false;
};

static DynamicByteBuffer LECisEstablishedPacketWithDefaultValues(
    pw::bluetooth::emboss::StatusCode status) {
  return testing::LECisEstablishedEventPacket(
      status,
      kCisHandleId,
      /*cig_sync_delay_us=*/0x123456,  // Must be in [0x0000ea, 0x7fffff]
      /*cis_sync_delay_us=*/0x7890ab,  // Must be in [0x0000ea, 0x7fffff]
      /*transport_latency_c_to_p_us=*/0x654321,  // Must be in [0x0000ea,
                                                 // 0x7fffff]
      /*transport_latency_p_to_c_us=*/0x0fedcb,  // Must be in [0x0000ea,
                                                 // 0x7fffff]
      /*phy_c_to_p=*/pw::bluetooth::emboss::IsoPhyType::LE_2M,
      /*phy_c_to_p=*/pw::bluetooth::emboss::IsoPhyType::LE_CODED,
      /*nse=*/0x10,               // Must be in [0x01, 0x1f]
      /*bn_c_to_p=*/0x05,         // Must be in [0x00, 0x0f]
      /*bn_p_to_c=*/0x0f,         // Must be in [0x00, 0x0f]
      /*ft_c_to_p=*/0x01,         // Must be in [0x01, 0xff]
      /*ft_p_to_c=*/0xff,         // Must be in [0x01, 0xff]
      /*max_pdu_c_to_p=*/0x0042,  // Must be in [0x0000, 0x00fb]
      /*max_pdu_p_to_c=*/0x00fb,  // Must be in [0x0000, 0x00fb]
      /*iso_interval=*/0x0222     // Must be in [0x0004, 0x0c80]
  );
}

void IsoStreamTest::EstablishCis(pw::bluetooth::emboss::StatusCode status) {
  DynamicByteBuffer le_cis_established_packet =
      LECisEstablishedPacketWithDefaultValues(status);
  test_device()->SendCommandChannelPacket(le_cis_established_packet);
  RunUntilIdle();
  ASSERT_TRUE(establishment_status().has_value());
  EXPECT_EQ(*(establishment_status()), status);
  if (status == pw::bluetooth::emboss::StatusCode::SUCCESS) {
    EXPECT_TRUE(established_parameters().has_value());
  } else {
    EXPECT_FALSE(established_parameters().has_value());
  }
}

bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter> GenerateCodecId() {
  const uint16_t kCompanyId = 0x1234;
  bt::StaticPacket<pw::bluetooth::emboss::CodecIdWriter> codec_id;
  auto codec_id_view = codec_id.view();
  codec_id_view.coding_format().Write(pw::bluetooth::emboss::CodingFormat::LC3);
  codec_id_view.company_id().Write(kCompanyId);
  return codec_id;
}

void IsoStreamTest::SetupDataPath(
    pw::bluetooth::emboss::DataPathDirection direction,
    const std::optional<std::vector<uint8_t>>& codec_configuration,
    const std::optional<pw::bluetooth::emboss::StatusCode>& cmd_complete_status,
    iso::IsoStream::SetupDataPathError expected_cb_result,
    bool generate_mismatched_cid) {
  const uint32_t kControllerDelay = 1234;  // Must be < 4000000
  std::optional<iso::IsoStream::SetupDataPathError> actual_cb_result;

  if (cmd_complete_status.has_value()) {
    auto setup_data_path_packet =
        testing::LESetupIsoDataPathPacket(kCisHandleId,
                                          direction,
                                          /*HCI*/ 0,
                                          GenerateCodecId(),
                                          kControllerDelay,
                                          codec_configuration);
    hci_spec::ConnectionHandle cis_handle =
        kCisHandleId + (generate_mismatched_cid ? 1 : 0);
    auto setup_data_path_complete_packet =
        testing::LESetupIsoDataPathResponse(*cmd_complete_status, cis_handle);
    EXPECT_CMD_PACKET_OUT(test_device(),
                          setup_data_path_packet,
                          &setup_data_path_complete_packet);
  }

  iso_stream()->SetupDataPath(
      direction,
      GenerateCodecId(),
      codec_configuration,
      kControllerDelay,
      [&actual_cb_result](iso::IsoStream::SetupDataPathError result) {
        actual_cb_result = result;
      });
  RunUntilIdle();
  ASSERT_TRUE(actual_cb_result.has_value());
  EXPECT_EQ(expected_cb_result, *actual_cb_result);
}

TEST_F(IsoStreamTest, CisEstablishedSuccessfully) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
}

TEST_F(IsoStreamTest, CisEstablishmentFailed) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::MEMORY_CAPACITY_EXCEEDED);
}

TEST_F(IsoStreamTest, ClosedCallsCloseCallback) {
  EXPECT_FALSE(closed());
  iso_stream()->Close();
  EXPECT_TRUE(closed());
}

TEST_F(IsoStreamTest, SetupDataPathSuccessfully) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(pw::bluetooth::emboss::DataPathDirection::OUTPUT,
                /*codec_configuration=*/std::nullopt,
                pw::bluetooth::emboss::StatusCode::SUCCESS,
                iso::IsoStream::SetupDataPathError::kSuccess);
}

TEST_F(IsoStreamTest, SetupDataPathBeforeCisEstablished) {
  SetupDataPath(pw::bluetooth::emboss::DataPathDirection::OUTPUT,
                /*codec_configuration=*/std::nullopt,
                /*cmd_complete_status=*/std::nullopt,
                iso::IsoStream::SetupDataPathError::kCisNotEstablished);
}

TEST_F(IsoStreamTest, SetupInputDataPathTwice) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::INPUT,
      /*codec_configuration=*/std::nullopt,
      /*cmd_complete_status=*/pw::bluetooth::emboss::StatusCode::SUCCESS,
      iso::IsoStream::SetupDataPathError::kSuccess);
  SetupDataPath(pw::bluetooth::emboss::DataPathDirection::INPUT,
                /*codec_configuration=*/std::nullopt,
                /*cmd_complete_status=*/std::nullopt,
                iso::IsoStream::SetupDataPathError::kStreamAlreadyExists);
}

TEST_F(IsoStreamTest, SetupOutputDataPathTwice) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::OUTPUT,
      /*codec_configuration=*/std::nullopt,
      /*cmd_complete_status=*/pw::bluetooth::emboss::StatusCode::SUCCESS,
      iso::IsoStream::SetupDataPathError::kSuccess);
  SetupDataPath(pw::bluetooth::emboss::DataPathDirection::OUTPUT,
                /*codec_configuration=*/std::nullopt,
                /*cmd_complete_status=*/std::nullopt,
                iso::IsoStream::SetupDataPathError::kStreamAlreadyExists);
}

TEST_F(IsoStreamTest, SetupBothInputAndOutputDataPaths) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::OUTPUT,
      /*codec_configuration=*/std::nullopt,
      /*cmd_complete_status=*/pw::bluetooth::emboss::StatusCode::SUCCESS,
      iso::IsoStream::SetupDataPathError::kSuccess);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::INPUT,
      /*codec_configuration=*/std::nullopt,
      /*cmd_complete_status=*/pw::bluetooth::emboss::StatusCode::SUCCESS,
      iso::IsoStream::SetupDataPathError::kSuccess);
}

TEST_F(IsoStreamTest, SetupDataPathInvalidArgs) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(static_cast<pw::bluetooth::emboss::DataPathDirection>(250),
                /*codec_configuration=*/std::nullopt,
                /*cmd_complete_status=*/std::nullopt,
                iso::IsoStream::SetupDataPathError::kInvalidArgs);
}

TEST_F(IsoStreamTest, SetupDataPathWithCodecConfig) {
  std::vector<uint8_t> codec_config{5, 6, 7, 8};
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::OUTPUT,
      codec_config,
      /*cmd_complete_status=*/pw::bluetooth::emboss::StatusCode::SUCCESS,
      iso::IsoStream::SetupDataPathError::kSuccess);
}

// If the connection ID doesn't match in the command complete packet, fail
TEST_F(IsoStreamTest, SetupDataPathHandleMismatch) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::INPUT,
      /*codec_configuration=*/std::nullopt,
      /*cmd_complete_status=*/pw::bluetooth::emboss::StatusCode::SUCCESS,
      iso::IsoStream::SetupDataPathError::kStreamRejectedByController,
      /*generate_mismatched_cid=*/true);
}

TEST_F(IsoStreamTest, SetupDataPathControllerError) {
  EstablishCis(pw::bluetooth::emboss::StatusCode::SUCCESS);
  SetupDataPath(
      pw::bluetooth::emboss::DataPathDirection::INPUT,
      /*codec_configuration=*/std::nullopt,
      /*cmd_complete_status=*/
      pw::bluetooth::emboss::StatusCode::CONNECTION_ALREADY_EXISTS,
      iso::IsoStream::SetupDataPathError::kStreamRejectedByController);
}

}  // namespace bt::iso
