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

#include "pw_bluetooth_sapphire/internal/host/iso/iso_stream_manager.h"

#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"

namespace bt::iso {

// Connection handles, must be in the range [0x0000, 0x0eff]
constexpr hci_spec::ConnectionHandle kAclConnectionHandleId1 = 0x123;
constexpr hci_spec::ConnectionHandle kAclConnectionHandleId2 = 0x222;
constexpr hci_spec::ConnectionHandle kCisHandleId = 0xe09;

constexpr hci_spec::CigIdentifier kCigId = 0x11;
constexpr hci_spec::CisIdentifier kCisId = 0x18;
const bt::iso::CigCisIdentifier kCigCisId(kCigId, kCisId);

using MockControllerTestBase =
    bt::testing::FakeDispatcherControllerTest<bt::testing::MockController>;

class IsoStreamManagerTest : public MockControllerTestBase {
 public:
  IsoStreamManagerTest() = default;
  ~IsoStreamManagerTest() override = default;
  IsoStreamManager* iso_stream_manager() { return iso_stream_manager_.get(); }

  void SetUp() override {
    MockControllerTestBase::SetUp();
    iso_stream_manager_ = std::make_unique<IsoStreamManager>(
        kAclConnectionHandleId1, cmd_channel()->AsWeakPtr());
  }

  AcceptCisStatus CallAcceptCis(CigCisIdentifier id,
                                bool* cb_invoked = nullptr) {
    AcceptCisStatus status;
    if (cb_invoked) {
      *cb_invoked = false;
    }
    status = iso_stream_manager_->AcceptCis(
        id,
        [cb_invoked](
            pw::bluetooth::emboss::StatusCode status,
            std::optional<WeakSelf<IsoStream>::WeakPtr> iso_weak_ptr,
            const std::optional<CisEstablishedParameters>& cis_parameters) {
          if (cb_invoked) {
            *cb_invoked = true;
          }
        });
    return status;
  }

 private:
  std::unique_ptr<IsoStreamManager> iso_stream_manager_;
};

// Verify that we ignore a CIS request whose ACL connection handle doesn't match
// ours.
TEST_F(IsoStreamManagerTest, IgnoreIncomingWrongConnection) {
  DynamicByteBuffer request_packet = testing::LECISRequestEventPacket(
      kAclConnectionHandleId2, kCisHandleId, kCigId, kCisId);
  test_device()->SendCommandChannelPacket(request_packet);
}

// Verify that we reject a CIS request whose ACL connection handle matches ours,
// but that we are not waiting for.
TEST_F(IsoStreamManagerTest, RejectIncomingConnection) {
  const auto le_reject_cis_packet = testing::LERejectCISRequestCommandPacket(
      kCisHandleId, pw::bluetooth::emboss::StatusCode::UNSPECIFIED_ERROR);
  EXPECT_CMD_PACKET_OUT(test_device(), le_reject_cis_packet);

  DynamicByteBuffer request_packet = testing::LECISRequestEventPacket(
      kAclConnectionHandleId1, kCisHandleId, kCigId, kCisId);
  test_device()->SendCommandChannelPacket(request_packet);
}

// We should be able to wait on multiple CIS requests simultaneously, as long as
// they do not have the identical CIG/CIS values.
TEST_F(IsoStreamManagerTest, MultipleCISAcceptRequests) {
  const CigCisIdentifier kId1(0x14, 0x04);
  const CigCisIdentifier kId2(0x14, 0x05);

  // We should not be waiting on these connections
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId1));
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId2));

  // Start waiting
  EXPECT_EQ(CallAcceptCis(kId1), AcceptCisStatus::kSuccess);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId1));
  EXPECT_EQ(CallAcceptCis(kId2), AcceptCisStatus::kSuccess);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId2));

  // When a duplicate request comes in, we should decline it but continue to
  // wait
  EXPECT_EQ(CallAcceptCis(kId1), AcceptCisStatus::kAlreadyExists);
  ASSERT_TRUE(iso_stream_manager()->HandlerRegistered(kId1));

  // When a matching request arrives, we should accept it and stop waiting on it
  const auto le_accept_cis_packet =
      testing::LEAcceptCISRequestCommandPacket(kCisHandleId);
  EXPECT_CMD_PACKET_OUT(test_device(), le_accept_cis_packet);
  DynamicByteBuffer request_packet = testing::LECISRequestEventPacket(
      kAclConnectionHandleId1, kCisHandleId, kId1.cig_id(), kId1.cis_id());
  test_device()->SendCommandChannelPacket(request_packet);
  RunUntilIdle();
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId1));

  // Because we've already accepted the request, we should disallow any requests
  // that have the same CIG/CIS as an existing stream
  EXPECT_EQ(CallAcceptCis(kId1), AcceptCisStatus::kAlreadyExists);
  ASSERT_FALSE(iso_stream_manager()->HandlerRegistered(kId1));
}

}  // namespace bt::iso
