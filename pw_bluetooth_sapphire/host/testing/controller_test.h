// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_

#include <lib/async/cpp/task.h>
#include <zircon/assert.h>

#include <memory>

#include <fbl/macros.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/device_wrapper.h"
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
class ControllerTest : public ::gtest::TestLoopFixture {
 public:
  // Default data buffer information used by ACLDataChannel.
  static constexpr size_t kDefaultMaxAclDataPacketLength = 1024;
  static constexpr size_t kDefaultMaxAclPacketCount = 5;

  // Default data buffer information used by ScoDataChannel.
  static constexpr size_t kDefaultMaxScoPacketLength = 255;
  static constexpr size_t kDefaultMaxScoPacketCount = 5;

  ControllerTest() = default;
  ~ControllerTest() override = default;

 protected:
  void SetUp() override { SetUp(/*sco_enabled=*/true); }

  void SetUp(bool sco_enabled) {
    sco_enabled_ = sco_enabled;
    std::unique_ptr<hci::DeviceWrapper> device =
        ControllerTest<ControllerTestDoubleType>::SetUpTestDevice();
    std::unique_ptr<hci::HciWrapper> hci_wrapper =
        hci::HciWrapper::Create(std::move(device), dispatcher());
    transport_ = hci::Transport::Create(std::move(hci_wrapper)).take_value();
  }

  void TearDown() override {
    if (!transport_)
      return;

    RunLoopUntilIdle();
    transport_ = nullptr;
    test_device_ = nullptr;
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
  ControllerTestDoubleType* test_device() const { return test_device_.get(); }
  zx::channel test_cmd_chan() { return std::move(cmd1_); }
  zx::channel test_acl_chan() { return std::move(acl1_); }
  zx::channel test_sco_chan() { return std::move(sco1_); }

  // Starts processing data on the control, ACL, and SCO channels.
  void StartTestDevice() {
    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());
    test_device()->StartScoChannel(test_sco_chan());
  }

  // Set the vendor features that the transport will be configured to return.
  void set_vendor_features(bt_vendor_features_t features) {
    ZX_ASSERT(!transport_);
    vendor_features_ = features;
  }

  // Set a callback to be called when the device's EncodeVendorCommand method is called.
  void set_encode_vendor_command_cb(hci::DummyDeviceWrapper::EncodeCallback cb) {
    vendor_encode_cb_ = std::move(cb);
  }

  // Set a callback to be called when the device's ConfigureSco method is called.
  void set_configure_sco_cb(hci::DummyDeviceWrapper::ConfigureScoCallback cb) {
    configure_sco_cb_ = std::move(cb);
  }

  // Set a callback to be called when the device's ResetSco method is called.
  void set_reset_sco_cb(hci::DummyDeviceWrapper::ResetScoCallback cb) {
    reset_sco_cb_ = std::move(cb);
  }

 private:
  // Channels to be moved to the tests
  zx::channel cmd1_;
  zx::channel acl1_;
  zx::channel sco1_;

  // Initializes |test_device_| and returns the DeviceWrapper endpoint which can
  // be passed to classes that are under test.
  std::unique_ptr<hci::DeviceWrapper> SetUpTestDevice() {
    zx::channel cmd0;
    zx::channel acl0;

    zx_status_t status = zx::channel::create(0, &cmd0, &cmd1_);
    ZX_DEBUG_ASSERT(ZX_OK == status);

    status = zx::channel::create(0, &acl0, &acl1_);
    ZX_DEBUG_ASSERT(ZX_OK == status);

    // Wrap DummyDeviceWrapper callbacks so that tests can change them after handing off
    // DeviceWrapper to Transport.
    auto vendor_encode_cb = [this](auto cmd, auto params) -> fpromise::result<DynamicByteBuffer> {
      if (vendor_encode_cb_) {
        return vendor_encode_cb_(cmd, params);
      }
      return fpromise::error();
    };
    auto config_sco_cb = [this](auto format, auto encoding, auto rate, auto callback, auto cookie) {
      if (configure_sco_cb_) {
        configure_sco_cb_(format, encoding, rate, callback, cookie);
      }
    };
    auto reset_sco_cb = [this](auto callback, auto cookie) {
      if (reset_sco_cb_) {
        reset_sco_cb_(callback, cookie);
      }
    };
    auto hci_dev = std::make_unique<hci::DummyDeviceWrapper>(
        std::move(cmd0), std::move(acl0), vendor_features_, std::move(vendor_encode_cb));

    if (sco_enabled_) {
      zx::channel sco0;
      status = zx::channel::create(0, &sco0, &sco1_);
      ZX_ASSERT(ZX_OK == status);
      hci_dev->set_sco_channel(std::move(sco0));
    }

    hci_dev->set_configure_sco_callback(std::move(config_sco_cb));
    hci_dev->set_reset_sco_callback(std::move(reset_sco_cb));

    test_device_ = std::make_unique<ControllerTestDoubleType>();

    return hci_dev;
  }

  void OnAclDataReceived(hci::ACLDataPacketPtr data_packet) {
    // Accessing |data_received_callback_| is racy but unlikely to cause issues
    // in unit tests. NOTE(armansito): Famous last words?
    if (!data_received_callback_)
      return;

    async::PostTask(dispatcher(), [this, packet = std::move(data_packet)]() mutable {
      data_received_callback_(std::move(packet));
    });
  }

  std::unique_ptr<ControllerTestDoubleType> test_device_;
  std::unique_ptr<hci::Transport> transport_;
  hci::ACLPacketHandler data_received_callback_;

  bt_vendor_features_t vendor_features_ = 0u;
  // If true, return a valid SCO channel from DeviceWrapper.
  bool sco_enabled_ = true;
  hci::DummyDeviceWrapper::EncodeCallback vendor_encode_cb_;
  hci::DummyDeviceWrapper::ConfigureScoCallback configure_sco_cb_;
  hci::DummyDeviceWrapper::ResetScoCallback reset_sco_cb_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ControllerTest);
  static_assert(std::is_base_of<ControllerTestDoubleBase, ControllerTestDoubleType>::value,
                "TestBase must be used with a derivative of ControllerTestDoubleBase");
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_
