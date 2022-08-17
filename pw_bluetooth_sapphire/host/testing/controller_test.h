// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_

#include <lib/async/cpp/task.h>

#include <memory>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/macros.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_packet.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/mock_hci_wrapper.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/sco_data_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

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
    transport_ = hci::Transport::Create(ControllerTest<ControllerTestDoubleType>::SetUpTestHci());
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

  // Wires up MockHciWrapper to the controller test double so that packets can be exchanged.
  void StartTestDevice() {
    BT_ASSERT(mock_hci_);
    BT_ASSERT(test_device_);

    mock_hci_->set_send_acl_cb([this](std::unique_ptr<hci::ACLDataPacket> packet) {
      if (test_device_) {
        test_device_->HandleACLPacket(std::move(packet));
        return ZX_OK;
      }
      return ZX_ERR_IO_NOT_PRESENT;
    });
    test_device_->StartAclChannel([this](std::unique_ptr<hci::ACLDataPacket> packet) {
      if (mock_hci_) {
        mock_hci_->ReceiveAclPacket(std::move(packet));
      }
    });

    mock_hci_->set_send_command_cb([this](std::unique_ptr<hci::CommandPacket> packet) {
      if (test_device_) {
        test_device_->HandleCommandPacket(std::move(packet));
        return ZX_OK;
      }
      return ZX_ERR_IO_NOT_PRESENT;
    });
    test_device_->StartCmdChannel([this](std::unique_ptr<hci::EventPacket> packet) {
      // TODO(fxbug.dev/97629): Remove this PostTask and call ReceiveEvent() synchronously.
      async::PostTask(dispatcher(), [this, packet = std::move(packet)]() mutable {
        if (mock_hci_) {
          mock_hci_->ReceiveEvent(std::move(packet));
        }
      });
    });

    if (sco_enabled_) {
      mock_hci_->set_send_sco_cb([this](std::unique_ptr<hci::ScoDataPacket> packet) {
        if (test_device_) {
          test_device_->HandleScoPacket(std::move(packet));
          return ZX_OK;
        }
        return ZX_ERR_IO_NOT_PRESENT;
      });
      test_device_->StartScoChannel([this](std::unique_ptr<hci::ScoDataPacket> packet) {
        if (mock_hci_) {
          mock_hci_->ReceiveScoPacket(std::move(packet));
        }
      });
    }
  }

  // Set the vendor features that the transport will be configured to return.
  void set_vendor_features(hci::VendorFeaturesBits features) {
    BT_ASSERT(!transport_);
    vendor_features_ = features;
  }

  // Set a function to be called when HciWrapper's EncodeSetAclPriorityCommand method is called.
  void set_encode_acl_priority_command_cb(
      hci::testing::MockHciWrapper::EncodeAclPriorityCommandFunction cb) {
    encode_acl_priority_command_cb_ = std::move(cb);
  }

  // Set a function to be called when HciWrapper's ConfigureSco method is called.
  void set_configure_sco_cb(hci::testing::MockHciWrapper::ConfigureScoFunction cb) {
    configure_sco_cb_ = std::move(cb);
  }

  // Set a function to be called when HciWrapper's ResetSco method is called.
  void set_reset_sco_cb(hci::testing::MockHciWrapper::ResetScoFunction cb) {
    reset_sco_cb_ = std::move(cb);
  }

 private:
  // Initializes |test_device_| and returns the HciWrapper which can be passed to classes that are
  // under test.
  std::unique_ptr<hci::HciWrapper> SetUpTestHci() {
    // Wrap MockHciWrapper callbacks so that tests can change them after handing off MockHciWrapper
    // to Transport.
    auto encode_set_acl_priority_cb =
        [this](hci_spec::ConnectionHandle connection,
               hci::AclPriority priority) -> fitx::result<zx_status_t, DynamicByteBuffer> {
      if (encode_acl_priority_command_cb_) {
        return encode_acl_priority_command_cb_(connection, priority);
      }
      return fitx::error(ZX_ERR_NOT_SUPPORTED);
    };
    auto config_sco_cb = [this](hci::ScoCodingFormat coding_format, hci::ScoEncoding encoding,
                                hci::ScoSampleRate sample_rate,
                                hci::HciWrapper::StatusCallback callback) mutable {
      if (configure_sco_cb_) {
        configure_sco_cb_(coding_format, encoding, sample_rate, std::move(callback));
      }
    };
    auto reset_sco_cb = [this](hci::HciWrapper::StatusCallback callback) mutable {
      if (reset_sco_cb_) {
        reset_sco_cb_(std::move(callback));
      }
    };

    auto hci_wrapper = std::make_unique<hci::testing::MockHciWrapper>();
    mock_hci_ = hci_wrapper->GetWeakPtr();
    hci_wrapper->SetVendorFeatures(vendor_features_);
    hci_wrapper->set_sco_supported(sco_enabled_);
    hci_wrapper->SetEncodeAclPriorityCommandCallback(std::move(encode_set_acl_priority_cb));
    hci_wrapper->set_configure_sco_callback(std::move(config_sco_cb));
    hci_wrapper->SetResetScoCallback(std::move(reset_sco_cb));

    test_device_ = std::make_unique<ControllerTestDoubleType>();
    test_device_->set_error_callback([mock_hci = mock_hci_](zx_status_t status) {
      if (mock_hci) {
        mock_hci->SimulateError(status);
      }
    });

    return hci_wrapper;
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

  fxl::WeakPtr<hci::testing::MockHciWrapper> mock_hci_;
  std::unique_ptr<ControllerTestDoubleType> test_device_;
  std::unique_ptr<hci::Transport> transport_;
  hci::ACLPacketHandler data_received_callback_;

  hci::VendorFeaturesBits vendor_features_ = static_cast<hci::VendorFeaturesBits>(0);
  // If true, return a valid SCO channel from DeviceWrapper.
  bool sco_enabled_ = true;
  hci::testing::MockHciWrapper::EncodeAclPriorityCommandFunction encode_acl_priority_command_cb_;
  hci::testing::MockHciWrapper::ConfigureScoFunction configure_sco_cb_;
  hci::testing::MockHciWrapper::ResetScoFunction reset_sco_cb_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ControllerTest);
  static_assert(std::is_base_of<ControllerTestDoubleBase, ControllerTestDoubleType>::value,
                "TestBase must be used with a derivative of ControllerTestDoubleBase");
};

}  // namespace bt::testing

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_TESTING_CONTROLLER_TEST_H_
