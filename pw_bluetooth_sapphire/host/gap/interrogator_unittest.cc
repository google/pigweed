// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/interrogator.h"

#include <lib/async/default.h>

#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/status.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/util.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace bt::gap {

constexpr hci::ConnectionHandle kConnectionHandle = 0x0BAA;
const DeviceAddress kTestDevAddr(DeviceAddress::Type::kBREDR, {1});

class TestInterrogator final : public Interrogator {
 public:
  // Reuse constructor
  using Interrogator::Interrogator;
  ~TestInterrogator() override = default;

  using InterrogationRefPtr = Interrogator::InterrogationRefPtr;

  void set_send_commands_cb(fit::function<void(InterrogationRefPtr)> cb) {
    send_commands_cb_ = std::move(cb);
  }

 private:
  // Interrogator overrides:

  void SendCommands(InterrogationRefPtr interrogation) override {
    if (send_commands_cb_) {
      send_commands_cb_(std::move(interrogation));
    }
  }

  fit::function<void(InterrogationRefPtr)> send_commands_cb_;
};

using InterrogationRefPtr = TestInterrogator::InterrogationRefPtr;

using TestingBase = bt::testing::FakeControllerTest<bt::testing::TestController>;
class InterrogatorTest : public TestingBase {
 public:
  InterrogatorTest() = default;
  ~InterrogatorTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    peer_cache_ =
        std::make_unique<PeerCache>(inspector_.GetRoot().CreateChild(PeerCache::kInspectNodeName));
    interrogator_ = std::make_unique<TestInterrogator>(peer_cache_.get(), transport()->WeakPtr(),
                                                       async_get_default_dispatcher());

    StartTestDevice();
  }

  void TearDown() override {
    RunLoopUntilIdle();
    test_device()->Stop();
    interrogator_ = nullptr;
    peer_cache_ = nullptr;
    TestingBase::TearDown();
  }

 protected:
  PeerCache* peer_cache() const { return peer_cache_.get(); }

  TestInterrogator* interrogator() const { return interrogator_.get(); }

 private:
  inspect::Inspector inspector_;
  std::unique_ptr<PeerCache> peer_cache_;
  std::unique_ptr<TestInterrogator> interrogator_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(InterrogatorTest);
};

using GAP_InterrogatorTest = InterrogatorTest;

TEST_F(GAP_InterrogatorTest, DroppingInterrogationRefCompletesInterrogation) {
  std::optional<InterrogationRefPtr> ref;
  interrogator()->set_send_commands_cb([&ref](InterrogationRefPtr r) { ref = std::move(r); });

  auto* peer = peer_cache()->NewPeer(kTestDevAddr, true);

  ASSERT_FALSE(ref.has_value());

  std::optional<hci::Status> result;
  interrogator()->Start(peer->identifier(), kConnectionHandle,
                        [&](hci::Status status) { result = status; });

  ASSERT_TRUE(ref.has_value());
  EXPECT_FALSE(result.has_value());

  // Dropping ref should call result callback with success status.
  ref.reset();

  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->is_success());
}

}  // namespace bt::gap
