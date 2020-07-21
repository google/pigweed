// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_client_server.h"

#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer_test.h"

namespace bthost {
namespace {

namespace fgatt = fuchsia::bluetooth::gatt;

constexpr bt::PeerId kPeerId(1);
constexpr bt::UUID kHeartRate((uint16_t)0x180D);
constexpr bt::UUID kHid((uint16_t)0x1812);

class FIDL_GattClientServerTest : public bt::gatt::testing::FakeLayerTest {
 public:
  FIDL_GattClientServerTest() = default;
  ~FIDL_GattClientServerTest() override = default;

  void SetUp() override {
    fidl::InterfaceHandle<fgatt::Client> handle;
    server_ = std::make_unique<GattClientServer>(kPeerId, gatt()->AsWeakPtr(), handle.NewRequest());
    proxy_.Bind(std::move(handle));
  }

  fgatt::Client* proxy() const { return proxy_.get(); }

 private:
  std::unique_ptr<GattClientServer> server_;
  fgatt::ClientPtr proxy_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(FIDL_GattClientServerTest);
};

TEST_F(FIDL_GattClientServerTest, ListServices) {
  bt::gatt::ServiceData data1(bt::gatt::ServiceKind::PRIMARY, 1, 1, kHeartRate);
  bt::gatt::ServiceData data2(bt::gatt::ServiceKind::SECONDARY, 2, 2, kHid);
  gatt()->AddPeerService(kPeerId, data1);
  gatt()->AddPeerService(kPeerId, data2);

  std::vector<fgatt::ServiceInfo> results;
  proxy()->ListServices({}, [&](auto status, auto cb_results) {
    EXPECT_FALSE(status.error);
    results = std::move(cb_results);
  });
  RunLoopUntilIdle();

  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(kHeartRate.ToString(), results[0].type);
  EXPECT_TRUE(results[0].primary);
  EXPECT_EQ(kHid.ToString(), results[1].type);
  EXPECT_FALSE(results[1].primary);
}

}  // namespace
}  // namespace bthost
