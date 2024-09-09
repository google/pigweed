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

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/gatt_client_server.h"

#include "pw_bluetooth_sapphire/fuchsia/host/fidl/fake_gatt_fixture.h"

namespace bthost {
namespace {

namespace fgatt = fuchsia::bluetooth::gatt;

constexpr bt::PeerId kPeerId(1);
constexpr bt::UUID kHeartRate(uint16_t{0x180D});
constexpr bt::UUID kHid(uint16_t{0x1812});

class GattClientServerTest : public bt::fidl::testing::FakeGattFixture {
 public:
  GattClientServerTest() = default;
  ~GattClientServerTest() override = default;

  void SetUp() override {
    fidl::InterfaceHandle<fgatt::Client> handle;
    server_ = std::make_unique<GattClientServer>(
        kPeerId, gatt()->GetWeakPtr(), handle.NewRequest());
    proxy_.Bind(std::move(handle));
  }

  fgatt::Client* proxy() const { return proxy_.get(); }

 private:
  std::unique_ptr<GattClientServer> server_;
  fgatt::ClientPtr proxy_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(GattClientServerTest);
};

TEST_F(GattClientServerTest, ListServices) {
  bt::gatt::ServiceData data1(bt::gatt::ServiceKind::PRIMARY, 1, 1, kHeartRate);
  bt::gatt::ServiceData data2(bt::gatt::ServiceKind::SECONDARY, 2, 2, kHid);
  fake_gatt()->AddPeerService(kPeerId, data1);
  fake_gatt()->AddPeerService(kPeerId, data2);

  std::vector<fgatt::ServiceInfo> results;
  proxy()->ListServices({}, [&](auto status, auto cb_results) {
    EXPECT_FALSE(status.error);
    results = std::move(cb_results);
  });
  RunLoopUntilIdle();
  ASSERT_EQ(2u, results.size());
  std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
    return a.id < b.id;
  });
  EXPECT_EQ(kHeartRate.ToString(), results[0].type);
  EXPECT_TRUE(results[0].primary);
  EXPECT_EQ(kHid.ToString(), results[1].type);
  EXPECT_FALSE(results[1].primary);
}

}  // namespace
}  // namespace bthost
