// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sco_connection.h"

#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"

namespace bt::gap {
namespace {

hci::ConnectionHandle kConnectionHandle = 1u;

using TestingBase = ::gtest::TestLoopFixture;
class GAP_ScoConnectionTest : public TestingBase {
 public:
  GAP_ScoConnectionTest() = default;
  ~GAP_ScoConnectionTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();

    auto fake_conn = std::make_unique<hci::testing::FakeConnection>(
        kConnectionHandle, hci::Connection::LinkType::kSCO, hci::Connection::Role::kMaster,
        DeviceAddress(), DeviceAddress());
    hci_conn_ = fake_conn->WeakPtr();
    sco_conn_ = ScoConnection::Create(std::move(fake_conn));
  }

  void TearDown() override {
    sco_conn_ = nullptr;
    TestingBase::TearDown();
  }

  auto sco_conn() { return sco_conn_; }

  auto hci_conn() { return hci_conn_; }

 private:
  fbl::RefPtr<ScoConnection> sco_conn_;
  fxl::WeakPtr<hci::Connection> hci_conn_;
};

TEST_F(GAP_ScoConnectionTest, Send) { EXPECT_FALSE(sco_conn()->Send(nullptr)); }

TEST_F(GAP_ScoConnectionTest, MaxTxSduSize) { EXPECT_EQ(sco_conn()->max_tx_sdu_size(), 0u); }

TEST_F(GAP_ScoConnectionTest, ActivateAndDeactivate) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  EXPECT_TRUE(sco_conn()->Activate(nullptr, std::move(closed_cb)));
  EXPECT_EQ(close_count, 0u);
  EXPECT_TRUE(hci_conn());

  sco_conn()->Deactivate();
  EXPECT_EQ(close_count, 0u);
  EXPECT_FALSE(hci_conn());

  // Deactivating should be idempotent.
  sco_conn()->Deactivate();
}

TEST_F(GAP_ScoConnectionTest, ActivateAndClose) {
  size_t close_count = 0;
  auto closed_cb = [&] { close_count++; };

  EXPECT_TRUE(sco_conn()->Activate(nullptr, std::move(closed_cb)));
  EXPECT_EQ(close_count, 0u);
  EXPECT_TRUE(hci_conn());

  sco_conn()->Close();
  EXPECT_EQ(close_count, 1u);
  EXPECT_FALSE(hci_conn());

  // Closing should be idempotent.
  sco_conn()->Close();
  EXPECT_EQ(close_count, 1u);
}

TEST_F(GAP_ScoConnectionTest, UniqueId) { EXPECT_EQ(sco_conn()->unique_id(), kConnectionHandle); }

}  // namespace
}  // namespace bt::gap
