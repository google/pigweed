// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/host.h"

#include <lib/inspect/testing/cpp/inspect.h>
#include <lib/zx/channel.h>

#include <memory>

#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bthost::testing {

using TestingBase = ::gtest::TestLoopFixture;

static zx_status_t hosttest_open_command_channel(void *ctx, zx_handle_t in);
static zx_status_t hosttest_open_acl_data_channel(void *ctx, zx_handle_t in);
static zx_status_t hosttest_open_sco_data_channel(void *ctx, zx_handle_t in);
static zx_status_t hosttest_open_snoop_channel(void *ctx, zx_handle_t in);

static bt_hci_protocol_ops_t hosttest_hci_protocol_ops = {
    .open_command_channel = hosttest_open_command_channel,
    .open_acl_data_channel = hosttest_open_acl_data_channel,
    .open_sco_channel = hosttest_open_sco_data_channel,
    .open_snoop_channel = hosttest_open_snoop_channel,
};

class HostTest : public TestingBase {
 public:
  HostTest() = default;
  ~HostTest() override = default;

  void SetUp() override { host_ = Host::Create(hci_proto(), std::nullopt); }

  void TearDown() override {
    if (host_) {
      host_->ShutDown();
    }
    host_ = nullptr;
    TestingBase::TearDown();
  }

  zx_status_t OpenCommandChannel(zx_handle_t in) {
    EXPECT_FALSE(cmd_channel_.has_value());
    cmd_channel_ = in;
    return ZX_OK;
  }

  zx_status_t OpenAclDataChannel(zx_handle_t in) {
    EXPECT_FALSE(acl_channel_.has_value());
    acl_channel_ = in;
    return ZX_OK;
  }

  zx_status_t OpenSnoopChannel(zx_handle_t in) {
    EXPECT_FALSE(snoop_channel_.has_value());
    snoop_channel_ = in;
    return ZX_OK;
  }

 protected:
  Host *host() const { return host_.get(); }

  void DestroyHost() { host_ = nullptr; }

 private:
  fbl::RefPtr<Host> host_;

  // The channels that are provided to the device on initialization.
  // TODO(fxbug.dev/90952): Optionally wire these up to a TestController or MockController.
  std::optional<zx_handle_t> cmd_channel_;
  std::optional<zx_handle_t> acl_channel_;
  std::optional<zx_handle_t> snoop_channel_;

  bt_hci_protocol_t hci_proto() {
    bt_hci_protocol_t hci_proto;
    hci_proto.ops = &hosttest_hci_protocol_ops;
    hci_proto.ctx = this;
    return hci_proto;
  }

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(HostTest);
};

zx_status_t hosttest_open_command_channel(void *ctx, zx_handle_t in) {
  HostTest *test = static_cast<HostTest *>(ctx);
  return test->OpenCommandChannel(in);
}

zx_status_t hosttest_open_acl_data_channel(void *ctx, zx_handle_t in) {
  HostTest *test = static_cast<HostTest *>(ctx);
  return test->OpenAclDataChannel(in);
}

zx_status_t hosttest_open_sco_data_channel(void *ctx, zx_handle_t in) {
  zx_handle_close(in);
  return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t hosttest_open_snoop_channel(void *ctx, zx_handle_t in) {
  HostTest *test = static_cast<HostTest *>(ctx);
  return test->OpenSnoopChannel(in);
}

namespace {

TEST_F(HostTest, InitializeFailsWhenCommandTimesOut) {
  inspect::Inspector inspector;
  auto bt_host_node = inspector.GetRoot().CreateChild("bt-host");

  std::optional<bool> init_cb_result;
  bool error_cb_called = false;
  host()->Initialize(
      bt_host_node, [&](bool success) { init_cb_result = success; },
      [&]() { error_cb_called = true; });

  // Any command sent to the command channel will time out, so run the loop until it does, and the
  // transport should signal failure, which should call our init callback.
  constexpr zx::duration kCommandTimeout = zx::sec(12);
  RunLoopFor(kCommandTimeout);
  ASSERT_TRUE(init_cb_result.has_value());
  EXPECT_FALSE(*init_cb_result);
  EXPECT_FALSE(error_cb_called);
  init_cb_result.reset();

  host()->ShutDown();
  EXPECT_FALSE(init_cb_result.has_value());
  EXPECT_FALSE(error_cb_called);

  DestroyHost();
  EXPECT_FALSE(init_cb_result.has_value());
  EXPECT_FALSE(error_cb_called);
}

}  // namespace
}  // namespace bthost::testing
