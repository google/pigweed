// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/fidl/host_server.h"

#include <fuchsia/bluetooth/control/cpp/fidl_test_base.h>
#include <lib/zx/channel.h>

#include "gmock/gmock.h"
#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt_host.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bthost {
namespace {

// Limiting the de-scoped aliases here helps test cases be more specific about whether they're using
// FIDL names or bt-host internal names.
using bt::testing::FakeController;
using bt::testing::FakePeer;
using TestingBase = bt::testing::FakeControllerTest<FakeController>;
using HostPairingDelegate = bt::gap::PairingDelegate;
using fuchsia::bluetooth::control::InputCapabilityType;
using fuchsia::bluetooth::control::OutputCapabilityType;
using FidlPairingDelegate = fuchsia::bluetooth::control::PairingDelegate;
using FidlRemoteDevice = fuchsia::bluetooth::control::RemoteDevice;

const bt::DeviceAddress kTestAddr(bt::DeviceAddress::Type::kLEPublic, {0x01, 0, 0, 0, 0, 0});

class MockPairingDelegate : public fuchsia::bluetooth::control::testing::PairingDelegate_TestBase {
 public:
  MockPairingDelegate(fidl::InterfaceRequest<PairingDelegate> request,
                      async_dispatcher_t* dispatcher)
      : binding_(this, std::move(request), dispatcher) {}

  ~MockPairingDelegate() override = default;

  MOCK_METHOD(void, OnPairingRequest,
              (fuchsia::bluetooth::control::RemoteDevice device,
               fuchsia::bluetooth::control::PairingMethod method, fidl::StringPtr displayed_passkey,
               OnPairingRequestCallback callback),
              (override));
  MOCK_METHOD(void, OnPairingComplete, (std::string device_id, fuchsia::bluetooth::Status status),
              (override));
  MOCK_METHOD(void, OnRemoteKeypress,
              (std::string device_id, fuchsia::bluetooth::control::PairingKeypressType keypress),
              (override));

 private:
  fidl::Binding<PairingDelegate> binding_;

  void NotImplemented_(const std::string& name) override {
    FAIL() << name << " is not implemented";
  }
};

class HostServerTest : public TestingBase {
 public:
  HostServerTest() = default;
  ~HostServerTest() override = default;

  void SetUp() override {
    TestingBase::SetUp();

    // Boilerplate to create and initialize an Adapter with emulated dependencies.
    auto data_domain = bt::data::testing::FakeDomain::Create();
    data_domain->Initialize();
    auto gatt = bt::gatt::testing::FakeLayer::Create();
    adapter_ = std::make_unique<bt::gap::Adapter>(transport(), gatt, std::move(data_domain));
    test_device()->StartCmdChannel(test_cmd_chan());
    test_device()->StartAclChannel(test_acl_chan());

    FakeController::Settings settings;
    settings.lmp_features_page0 |= static_cast<uint64_t>(bt::hci::LMPFeature::kLESupported);
    settings.le_acl_data_packet_length = 5;
    settings.le_total_num_acl_data_packets = 1;
    test_device()->set_settings(settings);

    bool success = false;
    auto init_cb = [&](bool cb_success) { success = cb_success; };
    adapter()->Initialize(init_cb, [] {});
    RunLoopUntilIdle();
    ASSERT_TRUE(success);

    // Create a HostServer and bind it to a local client.
    fidl::InterfaceHandle<fuchsia::bluetooth::host::Host> host_handle;
    gatt_host_ = GattHost::CreateForTesting(dispatcher(), std::move(gatt));
    host_server_ = std::make_unique<HostServer>(host_handle.NewRequest().TakeChannel(),
                                                adapter()->AsWeakPtr(), gatt_host_);
    host_.Bind(std::move(host_handle));
  }

  void TearDown() override {
    RunLoopUntilIdle();

    host_ = nullptr;
    host_server_ = nullptr;
    gatt_host_->ShutDown();
    gatt_host_ = nullptr;
    if (adapter()->IsInitialized()) {
      adapter()->ShutDown();
    }
    adapter_ = nullptr;

    RunLoopUntilIdle();
    TestingBase::TearDown();
  }

 protected:
  bt::gap::Adapter* adapter() const { return adapter_.get(); }

  HostServer* host_server() const { return host_server_.get(); }

  fuchsia::bluetooth::host::Host* host_client() const { return host_.get(); }

  // Create and bind a MockPairingDelegate and attach it to the HostServer under test. It is
  // heap-allocated to permit its explicit destruction.
  [[nodiscard]] std::unique_ptr<MockPairingDelegate> SetMockPairingDelegate(
      InputCapabilityType input_capability, OutputCapabilityType output_capability) {
    using testing::StrictMock;
    fidl::InterfaceHandle<FidlPairingDelegate> pairing_delegate_handle;
    auto pairing_delegate = std::make_unique<StrictMock<MockPairingDelegate>>(
        pairing_delegate_handle.NewRequest(), dispatcher());
    host_client()->SetPairingDelegate(input_capability, output_capability,
                                      std::move(pairing_delegate_handle));

    // Wait for the Control/SetPairingDelegate message to process.
    RunLoopUntilIdle();
    return pairing_delegate;
  }

 private:
  std::unique_ptr<bt::gap::Adapter> adapter_;
  std::unique_ptr<HostServer> host_server_;
  fbl::RefPtr<GattHost> gatt_host_;
  fuchsia::bluetooth::host::HostPtr host_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HostServerTest);
};

TEST_F(HostServerTest, FidlIoCapabilitiesMapToHostIoCapability) {
  // Isolate HostServer's private bt::gap::PairingDelegate implementation.
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());

  // Getter should be safe to call when no PairingDelegate assigned.
  EXPECT_EQ(bt::sm::IOCapability::kNoInputNoOutput, host_pairing_delegate->io_capability());

  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);
  EXPECT_EQ(bt::sm::IOCapability::kKeyboardDisplay, host_pairing_delegate->io_capability());
}

TEST_F(HostServerTest, HostCompletePairingCallsFidlOnPairingComplete) {
  using namespace testing;

  // Isolate HostServer's private bt::gap::PairingDelegate implementation.
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  // fuchsia::bluetooth::Status is move-only so check its value in a lambda.
  EXPECT_CALL(*fidl_pairing_delegate, OnPairingComplete(EndsWith("c0decafe"), _))
      .WillOnce([](Unused, fuchsia::bluetooth::Status status) {
        ASSERT_TRUE(status.error);
        EXPECT_EQ(fuchsia::bluetooth::ErrorCode::PROTOCOL_ERROR, status.error->error_code);
        EXPECT_EQ(static_cast<uintmax_t>(bt::sm::ErrorCode::kConfirmValueFailed),
                  status.error->protocol_error_code);
      });
  host_pairing_delegate->CompletePairing(bt::PeerId(0xc0decafe),
                                         bt::sm::Status(bt::sm::ErrorCode::kConfirmValueFailed));

  // Wait for the PairingDelegate/OnPairingComplete message to process.
  RunLoopUntilIdle();
}

TEST_F(HostServerTest, HostConfirmPairingRequestsConsentPairingOverFidl) {
  using namespace testing;
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  auto* const peer = adapter()->peer_cache()->NewPeer(kTestAddr, /*connectable=*/true);
  ASSERT_TRUE(peer);

  EXPECT_CALL(*fidl_pairing_delegate,
              OnPairingRequest(_, fuchsia::bluetooth::control::PairingMethod::CONSENT,
                               fidl::StringPtr(nullptr), _))
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          FidlPairingDelegate::OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/true, /*entered_passkey=*/nullptr);
      });

  bool confirm_cb_value = false;
  HostPairingDelegate::ConfirmCallback confirm_cb = [&confirm_cb_value](bool confirmed) {
    confirm_cb_value = confirmed;
  };
  host_pairing_delegate->ConfirmPairing(peer->identifier(), std::move(confirm_cb));

  // Wait for the PairingDelegate/OnPairingRequest message to process.
  RunLoopUntilIdle();
  EXPECT_TRUE(confirm_cb_value);
}

TEST_F(HostServerTest, HostDisplayPasskeyRequestsPasskeyDisplayOrNumericComparisonPairingOverFidl) {
  using namespace testing;
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  auto* const peer = adapter()->peer_cache()->NewPeer(kTestAddr, /*connectable=*/true);
  ASSERT_TRUE(peer);

  // This call should use PASSKEY_DISPLAY to request that the user perform peer passkey entry.
  using fuchsia::bluetooth::control::PairingMethod;
  using OnPairingRequestCallback = FidlPairingDelegate::OnPairingRequestCallback;
  EXPECT_CALL(*fidl_pairing_delegate,
              OnPairingRequest(_, PairingMethod::PASSKEY_DISPLAY, fidl::StringPtr("012345"), _))
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/false, /*entered_passkey=*/nullptr);
      });

  bool confirm_cb_called = false;
  auto confirm_cb = [&confirm_cb_called](bool confirmed) {
    EXPECT_FALSE(confirmed);
    confirm_cb_called = true;
  };
  using DisplayMethod = HostPairingDelegate::DisplayMethod;
  host_pairing_delegate->DisplayPasskey(peer->identifier(), 12345, DisplayMethod::kPeerEntry,
                                        confirm_cb);

  // Wait for the PairingDelegate/OnPairingRequest message to process.
  RunLoopUntilIdle();
  EXPECT_TRUE(confirm_cb_called);

  // This call should use PASSKEY_COMPARISON to request that the user compare the passkeys shown on
  // the local and peer devices.
  EXPECT_CALL(*fidl_pairing_delegate,
              OnPairingRequest(_, PairingMethod::PASSKEY_COMPARISON, fidl::StringPtr("012345"), _))
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/false, /*entered_passkey=*/nullptr);
      });

  confirm_cb_called = false;
  host_pairing_delegate->DisplayPasskey(peer->identifier(), 12345, DisplayMethod::kComparison,
                                        confirm_cb);

  // Wait for the PairingDelegate/OnPairingRequest message to process.
  RunLoopUntilIdle();
  EXPECT_TRUE(confirm_cb_called);
}

TEST_F(HostServerTest, HostRequestPasskeyRequestsPasskeyEntryPairingOverFidl) {
  using namespace testing;
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  auto* const peer = adapter()->peer_cache()->NewPeer(kTestAddr, /*connectable=*/true);
  ASSERT_TRUE(peer);

  using OnPairingRequestCallback = FidlPairingDelegate::OnPairingRequestCallback;
  EXPECT_CALL(*fidl_pairing_delegate,
              OnPairingRequest(_, fuchsia::bluetooth::control::PairingMethod::PASSKEY_ENTRY,
                               fidl::StringPtr(nullptr), _))
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/false, "012345");
      })
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/true, nullptr);
      })
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/true, u8"🍂");
      })
      .WillOnce([id = peer->identifier()](FidlRemoteDevice device, Unused, Unused,
                                          OnPairingRequestCallback callback) {
        EXPECT_THAT(device.identifier, EndsWith(id.ToString()));
        callback(/*accept=*/true, "012345");
      });

  std::optional<int64_t> passkey_response;
  auto response_cb = [&passkey_response](int64_t passkey) { passkey_response = passkey; };

  // Expect the first three pairing requests to provide passkey values that indicate failures.
  for (int i = 0; i < 3; i++) {
    passkey_response.reset();
    host_pairing_delegate->RequestPasskey(peer->identifier(), response_cb);

    // Wait for the PairingDelegate/OnPairingRequest message to process.
    RunLoopUntilIdle();

    // Include loop counter to help debug test failures.
    ASSERT_TRUE(passkey_response.has_value()) << i;

    // Negative values indicate "reject pairing."
    EXPECT_LT(passkey_response.value(), 0) << i;
  }

  // Last request should succeed.
  passkey_response.reset();
  host_pairing_delegate->RequestPasskey(peer->identifier(), response_cb);

  // Wait for the PairingDelegate/OnPairingRequest message to process.
  RunLoopUntilIdle();
  ASSERT_TRUE(passkey_response.has_value());
  EXPECT_EQ(12345, passkey_response.value());
}

}  // namespace
}  // namespace bthost
