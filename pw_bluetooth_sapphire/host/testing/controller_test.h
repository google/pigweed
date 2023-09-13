// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_

#include <memory>

#include <pw_async/heap_dispatcher.h>
#include <pw_async_fuchsia/dispatcher.h>

#include "pw_async/fake_dispatcher_fixture.h"
#include "pw_bluetooth/controller.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::testing {

class ControllerTestDoubleBase;

// ControllerTest is a test harness intended for tests that rely on HCI
// transactions. It is templated on ControllerTestDoubleType which must derive from
// ControllerTestDoubleBase and must be able to send and receive HCI packets over
// Zircon channels, acting as the controller endpoint of HCI.
//
// The testing library provides two such types:
//
//   - MockController (mock_controller.h): Routes HCI packets directly to the
//     test harness. It allows tests to setup expectations based on the receipt
//     of HCI packets.
//
//   - FakeController (fake_controller.h): Emulates a Bluetooth controller. This
//     can respond to HCI commands the way a real controller would (albeit in a
//     contrived fashion), emulate discovery and connection events, etc.
template <class ControllerTestDoubleType>
class ControllerTest {
 public:
  // Default data buffer information used by ACLDataChannel.
  static constexpr size_t kDefaultMaxAclDataPacketLength = 1024;
  static constexpr size_t kDefaultMaxAclPacketCount = 5;

  // Default data buffer information used by ScoDataChannel.
  static constexpr size_t kDefaultMaxScoPacketLength = 255;
  static constexpr size_t kDefaultMaxScoPacketCount = 5;

  ControllerTest(pw::async::Dispatcher& dispatcher) : dispatcher_(dispatcher) {}
  ~ControllerTest() = default;

 protected:
  void Initialize(pw::bluetooth::Controller::FeaturesBits features,
                  bool initialize_transport = true) {
    std::unique_ptr<pw::bluetooth::Controller> controller =
        ControllerTest<ControllerTestDoubleType>::SetUpTestController();
    test_device_->set_features(features);
    transport_ = std::make_unique<hci::Transport>(std::move(controller), dispatcher_);

    if (initialize_transport) {
      std::optional<bool> init_result;
      transport_->Initialize([&init_result](bool success) { init_result = success; });
      ASSERT_TRUE(init_result.has_value());
      ASSERT_TRUE(init_result.value());
    }
  }

  // Directly initializes the ACL data channel and wires up its data rx
  // callback. It is OK to override the data rx callback after this is called.
  //
  // If data buffer information isn't provided, the ACLDataChannel will be
  // initialized with shared BR/EDR/LE buffers using the constants declared
  // above.
  bool InitializeACLDataChannel(const hci::DataBufferInfo& bredr_buffer_info = hci::DataBufferInfo(
                                    kDefaultMaxAclDataPacketLength, kDefaultMaxAclPacketCount),
                                const hci::DataBufferInfo& le_buffer_info = hci::DataBufferInfo()) {
    if (!transport_->InitializeACLDataChannel(bredr_buffer_info, le_buffer_info)) {
      return false;
    }

    transport_->acl_data_channel()->SetDataRxHandler(std::bind(
        &ControllerTest<ControllerTestDoubleType>::OnAclDataReceived, this, std::placeholders::_1));

    return true;
  }

  // Directly initializes the SCO data channel.
  bool InitializeScoDataChannel(const hci::DataBufferInfo& buffer_info = hci::DataBufferInfo(
                                    kDefaultMaxScoPacketLength, kDefaultMaxScoPacketCount)) {
    return transport_->InitializeScoDataChannel(buffer_info);
  }

  // Sets a callback which will be invoked when we receive packets from the test
  // controller. |callback| will be posted on the test loop, thus no locking is
  // necessary within the callback.
  //
  // InitializeACLDataChannel() must be called once and its data rx handler must
  // not be overridden by tests for |callback| to work.
  void set_data_received_callback(hci::ACLPacketHandler callback) {
    data_received_callback_ = std::move(callback);
  }

  hci::Transport* transport() const { return transport_.get(); }
  hci::CommandChannel* cmd_channel() const { return transport_->command_channel(); }
  hci::AclDataChannel* acl_data_channel() const { return transport_->acl_data_channel(); }
  hci::ScoDataChannel* sco_data_channel() const { return transport_->sco_data_channel(); }

  // Deletes |test_device_| and resets the pointer.
  void DeleteTestDevice() { test_device_ = nullptr; }
  void DeleteTransport() { transport_ = nullptr; }

  // Getters for internal fields frequently used by tests.
  const typename ControllerTestDoubleType::WeakPtr& test_device() const { return test_device_; }

 private:
  std::unique_ptr<pw::bluetooth::Controller> SetUpTestController() {
    std::unique_ptr<ControllerTestDoubleType> controller =
        std::make_unique<ControllerTestDoubleType>(dispatcher_);
    test_device_ = controller->GetWeakPtr();
    return controller;
  }

  void OnAclDataReceived(hci::ACLDataPacketPtr data_packet) {
    // Accessing |data_received_callback_| is racy but unlikely to cause issues
    // in unit tests. NOTE(armansito): Famous last words?
    if (!data_received_callback_)
      return;

    heap_dispatcher_.Post([this, packet = std::move(data_packet)](pw::async::Context /*ctx*/,
                                                                  pw::Status status) mutable {
      if (status.ok()) {
        data_received_callback_(std::move(packet));
      }
    });
  }

  pw::async::Dispatcher& dispatcher_;
  pw::async::HeapDispatcher heap_dispatcher_{dispatcher_};
  typename ControllerTestDoubleType::WeakPtr test_device_;
  std::unique_ptr<hci::Transport> transport_;
  hci::ACLPacketHandler data_received_callback_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ControllerTest);
  static_assert(std::is_base_of<ControllerTestDoubleBase, ControllerTestDoubleType>::value,
                "TestBase must be used with a derivative of ControllerTestDoubleBase");
};

// FakeDispatcherControllerTest is a convenience test fixture that initializes ControllerTest with a
// pw_async FakeDispatcherFixture backend. Only if a different underlying dispatcher is desired
// (e.g. Zircon TestLoopFixture) should ControllerTest be referenced directly and passed the desired
// dispatcher, which must implement the pw_async Dispatcher interface.
//
// To properly "TearDown" ControllerTest, the Dispatcher must be driven, then DeleteTransport()
// called.
template <typename ControllerTestDoubleType>
class FakeDispatcherControllerTest : public pw::async::test::FakeDispatcherFixture,
                                     public ControllerTest<ControllerTestDoubleType> {
 protected:
  FakeDispatcherControllerTest() : ControllerTest<ControllerTestDoubleType>(dispatcher()) {}

  void SetUp() override { SetUp(pw::bluetooth::Controller::FeaturesBits::kHciSco); }

  void SetUp(pw::bluetooth::Controller::FeaturesBits features, bool initialize_transport = true) {
    ControllerTest<ControllerTestDoubleType>::Initialize(features, initialize_transport);
    RunUntilIdle();
  }

  void TearDown() override {
    RunUntilIdle();
    ControllerTest<ControllerTestDoubleType>::DeleteTransport();
  }
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_
