// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/fidl/host_server.h"

#include <fuchsia/bluetooth/control/cpp/fidl_test_base.h>
#include <lib/zx/channel.h>

#include "adapter_test_fixture.h"
#include "fuchsia/bluetooth/control/cpp/fidl.h"
#include "fuchsia/bluetooth/cpp/fidl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/device_address.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/gap.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt_host.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bthost {
namespace {

// Limiting the de-scoped aliases here helps test cases be more specific about whether they're using
// FIDL names or bt-host internal names.
using bt::CreateStaticByteBuffer;
using bt::LowerBits;
using bt::UpperBits;
using bt::l2cap::testing::FakeChannel;
using bt::testing::FakePeer;
using HostPairingDelegate = bt::gap::PairingDelegate;
using fuchsia::bluetooth::control::InputCapabilityType;
using fuchsia::bluetooth::control::OutputCapabilityType;
using FidlErrorCode = fuchsia::bluetooth::ErrorCode;
using FidlStatus = fuchsia::bluetooth::Status;
using fuchsia::bluetooth::control::PairingOptions;
using FidlPairingDelegate = fuchsia::bluetooth::control::PairingDelegate;
using fuchsia::bluetooth::control::PairingSecurityLevel;

using FidlRemoteDevice = fuchsia::bluetooth::control::RemoteDevice;

namespace fbt = fuchsia::bluetooth;
namespace fsys = fuchsia::bluetooth::sys;

const bt::DeviceAddress kLETestAddr(bt::DeviceAddress::Type::kLEPublic, {0x01, 0, 0, 0, 0, 0});
const bt::DeviceAddress kBrEdrTestAddr(bt::DeviceAddress::Type::kBREDR, {0x01, 0, 0, 0, 0, 0});

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

class FIDL_HostServerTest : public bthost::testing::AdapterTestFixture {
 public:
  FIDL_HostServerTest() = default;
  ~FIDL_HostServerTest() override = default;

  void SetUp() override {
    AdapterTestFixture::SetUp();

    // Create a HostServer and bind it to a local client.
    fidl::InterfaceHandle<fuchsia::bluetooth::host::Host> host_handle;
    gatt_host_ = GattHost::CreateForTesting(dispatcher(), gatt());
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

    RunLoopUntilIdle();
    AdapterTestFixture::TearDown();
  }

 protected:
  HostServer* host_server() const { return host_server_.get(); }

  fuchsia::bluetooth::host::Host* host_client() const { return host_.get(); }

  // Create and bind a MockPairingDelegate and attach it to the HostServer under test. It is
  // heap-allocated to permit its explicit destruction.
  [[nodiscard]] std::unique_ptr<MockPairingDelegate> SetMockPairingDelegate(
      InputCapabilityType input_capability, OutputCapabilityType output_capability) {
    using ::testing::StrictMock;
    fidl::InterfaceHandle<FidlPairingDelegate> pairing_delegate_handle;
    auto pairing_delegate = std::make_unique<StrictMock<MockPairingDelegate>>(
        pairing_delegate_handle.NewRequest(), dispatcher());
    host_client()->SetPairingDelegate(input_capability, output_capability,
                                      std::move(pairing_delegate_handle));

    // Wait for the Control/SetPairingDelegate message to process.
    RunLoopUntilIdle();
    return pairing_delegate;
  }

  std::tuple<bt::gap::Peer*, FakeChannel*> ConnectFakePeer(bool connect_le = true) {
    auto device_addr = connect_le ? kLETestAddr : kBrEdrTestAddr;
    auto* peer = adapter()->peer_cache()->NewPeer(device_addr, true);
    EXPECT_TRUE(peer->temporary());
    // This is to capture the channel created during the Connection process
    FakeChannel* fake_chan = nullptr;
    fake_domain()->set_channel_callback(
        [&fake_chan](fbl::RefPtr<FakeChannel> new_fake_chan) { fake_chan = new_fake_chan.get(); });

    auto fake_peer = std::make_unique<FakePeer>(device_addr);
    test_device()->AddPeer(std::move(fake_peer));
    // Initialize with error to ensure callback is called
    FidlStatus connect_status =
        fidl_helpers::NewFidlError(FidlErrorCode::BAD_STATE, "This should disappear");
    host_client()->Connect(peer->identifier().ToString(), [&connect_status](FidlStatus cb_status) {
      ASSERT_FALSE(cb_status.error);
      connect_status = std::move(cb_status);
    });
    RunLoopUntilIdle();
    if (connect_status.error != nullptr) {
      peer = nullptr;
      fake_chan = nullptr;
    }
    return std::make_tuple(peer, fake_chan);
  }

 private:
  std::unique_ptr<HostServer> host_server_;
  fbl::RefPtr<GattHost> gatt_host_;
  fuchsia::bluetooth::host::HostPtr host_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_HostServerTest);
};

TEST_F(FIDL_HostServerTest, FidlIoCapabilitiesMapToHostIoCapability) {
  // Isolate HostServer's private bt::gap::PairingDelegate implementation.
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());

  // Getter should be safe to call when no PairingDelegate assigned.
  EXPECT_EQ(bt::sm::IOCapability::kNoInputNoOutput, host_pairing_delegate->io_capability());

  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);
  EXPECT_EQ(bt::sm::IOCapability::kKeyboardDisplay, host_pairing_delegate->io_capability());
}

TEST_F(FIDL_HostServerTest, HostCompletePairingCallsFidlOnPairingComplete) {
  using namespace ::testing;

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

TEST_F(FIDL_HostServerTest, HostConfirmPairingRequestsConsentPairingOverFidl) {
  using namespace ::testing;
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  auto* const peer = adapter()->peer_cache()->NewPeer(kLETestAddr, /*connectable=*/true);
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

TEST_F(FIDL_HostServerTest,
       HostDisplayPasskeyRequestsPasskeyDisplayOrNumericComparisonPairingOverFidl) {
  using namespace ::testing;
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  auto* const peer = adapter()->peer_cache()->NewPeer(kLETestAddr, /*connectable=*/true);
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

TEST_F(FIDL_HostServerTest, HostRequestPasskeyRequestsPasskeyEntryPairingOverFidl) {
  using namespace ::testing;
  auto host_pairing_delegate = static_cast<HostPairingDelegate*>(host_server());
  auto fidl_pairing_delegate =
      SetMockPairingDelegate(InputCapabilityType::KEYBOARD, OutputCapabilityType::DISPLAY);

  auto* const peer = adapter()->peer_cache()->NewPeer(kLETestAddr, /*connectable=*/true);
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

TEST_F(FIDL_HostServerTest, WatchState) {
  std::optional<fsys::HostInfo> info;
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_id());
  ASSERT_TRUE(info->has_technology());
  ASSERT_TRUE(info->has_address());
  ASSERT_TRUE(info->has_local_name());
  ASSERT_TRUE(info->has_discoverable());
  ASSERT_TRUE(info->has_discovering());

  EXPECT_EQ(adapter()->identifier().value(), info->id().value);
  EXPECT_EQ(fsys::TechnologyType::DUAL_MODE, info->technology());
  EXPECT_EQ(fbt::AddressType::PUBLIC, info->address().type);
  EXPECT_TRUE(
      ContainersEqual(adapter()->state().controller_address().bytes(), info->address().bytes));
  EXPECT_EQ("fuchsia", info->local_name());
  EXPECT_FALSE(info->discoverable());
  EXPECT_FALSE(info->discovering());
}

TEST_F(FIDL_HostServerTest, WatchDiscoveryState) {
  std::optional<fsys::HostInfo> info;

  // Make initial watch call so that subsequent calls remain pending.
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  ASSERT_TRUE(info.has_value());
  info.reset();

  // Watch for updates.
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  EXPECT_FALSE(info.has_value());

  host_server()->StartDiscovery([](auto) {});
  RunLoopUntilIdle();
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_discovering());
  EXPECT_TRUE(info->discovering());

  info.reset();
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  EXPECT_FALSE(info.has_value());
  host_server()->StopDiscovery([](auto) {});
  RunLoopUntilIdle();
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_discovering());
  EXPECT_FALSE(info->discovering());
}

TEST_F(FIDL_HostServerTest, WatchDiscoverableState) {
  std::optional<fsys::HostInfo> info;

  // Make initial watch call so that subsequent calls remain pending.
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  ASSERT_TRUE(info.has_value());
  info.reset();

  // Watch for updates.
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  EXPECT_FALSE(info.has_value());

  host_server()->SetDiscoverable(true, [](auto) {});
  RunLoopUntilIdle();
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_discoverable());
  EXPECT_TRUE(info->discoverable());

  info.reset();
  host_server()->WatchState([&](auto value) { info = std::move(value); });
  EXPECT_FALSE(info.has_value());
  host_server()->SetDiscoverable(false, [](auto) {});
  RunLoopUntilIdle();
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_discoverable());
  EXPECT_FALSE(info->discoverable());
}

TEST_F(FIDL_HostServerTest, InitiatePairingLeDefault) {
  // clang-format off
  const auto kExpected = CreateStaticByteBuffer(
      0x01,  // code: "Pairing Request"
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x05,  // AuthReq: bonding, MITM (Authenticated)
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  // clang-format on

  auto [peer, fake_chan] = ConnectFakePeer();
  ASSERT_TRUE(peer);
  ASSERT_TRUE(fake_chan);
  ASSERT_EQ(bt::gap::Peer::ConnectionState::kConnected, peer->le()->connection_state());

  bool pairing_request_sent = false;
  // This test only checks that PairingState kicks off an LE pairing feature exchange correctly, as
  // the call to Pair is only responsible for starting pairing, not for completing it.
  auto expect_default_bytebuffer = [&pairing_request_sent, kExpected](bt::ByteBufferPtr sent) {
    ASSERT_TRUE(sent);
    ASSERT_EQ(*sent, kExpected);
    pairing_request_sent = true;
  };
  fake_chan->SetSendCallback(expect_default_bytebuffer, dispatcher());
  FidlStatus pair_status;
  PairingOptions opts;
  host_client()->Pair(fuchsia::bluetooth::PeerId{.value = peer->identifier().value()},
                      std::move(opts),
                      [&pair_status](FidlStatus status) { pair_status = std::move(status); });
  RunLoopUntilIdle();
  ASSERT_EQ(pair_status.error, nullptr);
  ASSERT_TRUE(pairing_request_sent);
}

TEST_F(FIDL_HostServerTest, InitiatePairingLeEncrypted) {
  // clang-format off
  const auto kExpected = CreateStaticByteBuffer(
      0x01,  // code: "Pairing Request"
      0x03,  // IO cap.: NoInputNoOutput
      0x00,  // OOB: not present
      0x01,  // AuthReq: bonding, no MITM (not authenticated)
      0x10,  // encr. key size: 16 (default max)
      0x00,  // initiator keys: none
      0x03   // responder keys: enc key and identity info
  );
  // clang-format on

  auto [peer, fake_chan] = ConnectFakePeer();
  ASSERT_TRUE(peer);
  ASSERT_TRUE(fake_chan);
  ASSERT_EQ(bt::gap::Peer::ConnectionState::kConnected, peer->le()->connection_state());

  bool pairing_request_sent = false;
  // This test only checks that PairingState kicks off an LE pairing feature exchange correctly, as
  // the call to Pair is only responsible for starting pairing, not for completing it.
  auto expect_default_bytebuffer = [&pairing_request_sent, kExpected](bt::ByteBufferPtr sent) {
    ASSERT_TRUE(sent);
    ASSERT_EQ(*sent, kExpected);
    pairing_request_sent = true;
  };
  fake_chan->SetSendCallback(expect_default_bytebuffer, dispatcher());

  FidlStatus pair_status;
  PairingOptions opts;
  opts.set_le_security_level(PairingSecurityLevel::ENCRYPTED);
  host_client()->Pair(fuchsia::bluetooth::PeerId{.value = peer->identifier().value()},
                      std::move(opts),
                      [&pair_status](FidlStatus status) { pair_status = std::move(status); });
  RunLoopUntilIdle();
  ASSERT_EQ(pair_status.error, nullptr);
  ASSERT_TRUE(pairing_request_sent);
}

TEST_F(FIDL_HostServerTest, InitiateBrEdrPairingLePeerFails) {
  auto [peer, fake_chan] = ConnectFakePeer();
  ASSERT_TRUE(peer);
  ASSERT_TRUE(fake_chan);
  ASSERT_EQ(bt::gap::Peer::ConnectionState::kConnected, peer->le()->connection_state());

  FidlStatus pair_status;
  PairingOptions opts;
  // Set pairing option with classic
  opts.set_transport(fuchsia::bluetooth::control::TechnologyType::CLASSIC);
  auto pair_cb = [&pair_status](FidlStatus cb_status) {
    ASSERT_TRUE(cb_status.error);
    pair_status = std::move(cb_status);
  };
  host_client()->Pair(fuchsia::bluetooth::PeerId{.value = peer->identifier().value()},
                      std::move(opts), pair_cb);
  RunLoopUntilIdle();
  ASSERT_EQ(pair_status.error->error_code, FidlErrorCode::NOT_FOUND);
}

}  // namespace
}  // namespace bthost
