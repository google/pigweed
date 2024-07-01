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

#include "pw_bluetooth_sapphire/internal/host/hci/legacy_low_energy_scanner.h"

#include "pw_bluetooth_sapphire/internal/host/hci/fake_local_address_delegate.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/fake_peer.h"

namespace bt::hci {

using bt::testing::FakeController;
using bt::testing::FakePeer;
using TestingBase = bt::testing::FakeDispatcherControllerTest<FakeController>;

constexpr pw::chrono::SystemClock::duration kPwScanResponseTimeout =
    std::chrono::seconds(2);

const StaticByteBuffer kPlainAdvDataBytes('T', 'e', 's', 't');
const StaticByteBuffer kPlainScanRspBytes('D', 'a', 't', 'a');

const DeviceAddress kPublicAddress1(DeviceAddress::Type::kLEPublic, {1});

class LegacyLowEnergyScannerTest : public TestingBase,
                                   public LowEnergyScanner::Delegate {
 public:
  LegacyLowEnergyScannerTest() = default;
  ~LegacyLowEnergyScannerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    FakeController::Settings settings;
    settings.ApplyLegacyLEConfig();
    test_device()->set_settings(settings);

    scanner_ = std::make_unique<LegacyLowEnergyScanner>(
        fake_address_delegate(), transport()->GetWeakPtr(), dispatcher());
    scanner_->set_delegate(this);
  }

  void TearDown() override {
    scanner_ = nullptr;
    test_device()->Stop();
    TestingBase::TearDown();
  }

  bool StartScan(bool active,
                 pw::chrono::SystemClock::duration period =
                     LowEnergyScanner::kPeriodInfinite) {
    LowEnergyScanner::ScanOptions options{
        .active = active,
        .filter_duplicates = true,
        .period = period,
        .scan_response_timeout = kPwScanResponseTimeout};
    return scanner()->StartScan(
        options, [this](auto status) { last_scan_status_ = status; });
  }

  using PeerFoundCallback =
      fit::function<void(const LowEnergyScanResult&, const ByteBuffer&)>;
  void set_peer_found_callback(PeerFoundCallback cb) {
    peer_found_cb_ = std::move(cb);
  }

  // LowEnergyScanner::Delegate override:
  void OnPeerFound(const LowEnergyScanResult& result,
                   const ByteBuffer& data) override {
    if (peer_found_cb_) {
      peer_found_cb_(result, data);
    }
  }

  LowEnergyScanner* scanner() const { return scanner_.get(); }
  FakeLocalAddressDelegate* fake_address_delegate() {
    return &fake_address_delegate_;
  }

 private:
  std::unique_ptr<LowEnergyScanner> scanner_;
  PeerFoundCallback peer_found_cb_;
  FakeLocalAddressDelegate fake_address_delegate_{dispatcher()};
  LowEnergyScanner::ScanStatus last_scan_status_;
};

// Ensure we can parse an advertising report that is batched with a scan
// response
TEST_F(LegacyLowEnergyScannerTest, ParseBatchedAdvertisingReport) {
  {
    auto peer = std::make_unique<FakePeer>(
        kPublicAddress1, dispatcher(), true, true, false);
    peer->set_advertising_data(kPlainAdvDataBytes);
    peer->set_scan_response(kPlainScanRspBytes);
    test_device()->AddPeer(std::move(peer));
  }

  bool peer_found_callback_called = false;
  std::unordered_map<DeviceAddress, std::unique_ptr<DynamicByteBuffer>> map;

  set_peer_found_callback(
      [&](const LowEnergyScanResult& result, const ByteBuffer& data) {
        peer_found_callback_called = true;
        map[result.address] = std::make_unique<DynamicByteBuffer>(data.size());
        data.Copy(&*map[result.address]);
      });

  EXPECT_TRUE(this->StartScan(true));
  RunUntilIdle();

  auto peer = test_device()->FindPeer(kPublicAddress1);
  DynamicByteBuffer buffer = peer->BuildLegacyAdvertisingReportEvent(true);
  test_device()->SendCommandChannelPacket(buffer);
  RunUntilIdle();
  ASSERT_TRUE(peer_found_callback_called);
  ASSERT_EQ(1u, map.count(peer->address()));
  EXPECT_EQ(kPlainAdvDataBytes.ToString() + kPlainScanRspBytes.ToString(),
            map[peer->address()]->ToString());
}

}  // namespace bt::hci
