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
    MockControllerTestBase::SetUp();
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
        cmd_channel()->AsWeakPtr(),
        /*on_closed_cb=*/
        [this]() {
          ASSERT_FALSE(closed_);
          closed_ = true;
        });
  }

 protected:
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

TEST_F(IsoStreamTest, CisEstablishedSuccessfully) {
  DynamicByteBuffer le_cis_established_packet =
      LECisEstablishedPacketWithDefaultValues(
          pw::bluetooth::emboss::StatusCode::SUCCESS);
  test_device()->SendCommandChannelPacket(le_cis_established_packet);
  RunUntilIdle();
  ASSERT_TRUE(establishment_status().has_value());
  EXPECT_EQ(*(establishment_status()),
            pw::bluetooth::emboss::StatusCode::SUCCESS);
  EXPECT_TRUE(established_parameters().has_value());
}

TEST_F(IsoStreamTest, CisEstablishmentFailed) {
  DynamicByteBuffer le_cis_established_packet =
      LECisEstablishedPacketWithDefaultValues(
          pw::bluetooth::emboss::StatusCode::MEMORY_CAPACITY_EXCEEDED);
  test_device()->SendCommandChannelPacket(le_cis_established_packet);
  RunUntilIdle();
  ASSERT_TRUE(establishment_status().has_value());
  EXPECT_EQ(*(establishment_status()),
            pw::bluetooth::emboss::StatusCode::MEMORY_CAPACITY_EXCEEDED);
  EXPECT_FALSE(established_parameters().has_value());
}

TEST_F(IsoStreamTest, ClosedCallsCloseCallback) {
  EXPECT_FALSE(closed());
  iso_stream()->Close();
  EXPECT_TRUE(closed());
}

}  // namespace bt::iso
