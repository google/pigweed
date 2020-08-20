// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/l2cap/dynamic_channel_registry.h"

#include <lib/async/cpp/task.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace l2cap {
namespace internal {
namespace {

constexpr int kNumChannelsAllowed = 256;
constexpr ChannelId kLargestChannelId = kFirstDynamicChannelId + kNumChannelsAllowed - 1;
constexpr uint16_t kPsm = 0x0001;
constexpr ChannelId kLocalCId = 0x0040;
constexpr ChannelId kRemoteCId = 0x60a3;
constexpr ChannelParameters kChannelParams;

class FakeDynamicChannel final : public DynamicChannel {
 public:
  FakeDynamicChannel(DynamicChannelRegistry* registry, PSM psm, ChannelId local_cid,
                     ChannelId remote_cid)
      : DynamicChannel(registry, psm, local_cid, remote_cid) {}
  ~FakeDynamicChannel() override { EXPECT_FALSE(IsConnected()); }

  // DynamicChannel overrides
  bool IsConnected() const override { return connected_; }
  bool IsOpen() const override { return open_; }

  ChannelInfo info() const override { return ChannelInfo::MakeBasicMode(kDefaultMTU, kDefaultMTU); }

  void DoConnect(ChannelId remote_cid) {
    ASSERT_TRUE(SetRemoteChannelId(remote_cid))
        << "Could not set non-unique remote_cid " << remote_cid;
    connected_ = true;
  }

  void DoOpen(bool new_open = true) {
    open_ = new_open;
    if (new_open) {
      set_opened();
    }
    open_result_cb_();
  }

  void DoRemoteClose() {
    open_ = false;
    connected_ = false;
    OnDisconnected();
  }

  // After calling |set_defer_disconnect_callback|, this returns the callback
  // passed to |Disconnect|, or an empty callback if |Disconnect| hasn't been
  // called.
  DisconnectDoneCallback& disconnect_done_callback() { return disconnect_done_callback_; }

  void set_defer_disconnect_done_callback() { defer_disconnect_done_callback_ = true; }

 private:
  // DynamicChannel overrides
  void Open(fit::closure open_result_cb) override { open_result_cb_ = std::move(open_result_cb); }
  void Disconnect(DisconnectDoneCallback done_cb) override {
    open_ = false;
    connected_ = false;

    bt_log(DEBUG, "l2cap", "Got Disconnect %#.4x callback: %d", local_cid(), psm());

    ASSERT_FALSE(disconnect_done_callback_);
    if (defer_disconnect_done_callback_) {
      disconnect_done_callback_ = std::move(done_cb);
    } else {
      done_cb();
    }
  }

  fit::closure open_result_cb_;
  DisconnectDoneCallback disconnect_done_callback_;

  // If true, the Disconnect call does not immediately signal its callback. The
  // test will have to call it explicitly with |disconnect_callback()|.
  bool defer_disconnect_done_callback_ = false;

  bool connected_ = false;
  bool open_ = false;
};

// Fake registry subclass for testing inherited logic. Stubs out |MakeOutbound|
// to vend FakeDynamicChannels.
class TestDynamicChannelRegistry final : public DynamicChannelRegistry {
 public:
  TestDynamicChannelRegistry(DynamicChannelCallback close_cb,
                             ServiceRequestCallback service_request_cb)
      : DynamicChannelRegistry(kLargestChannelId, std::move(close_cb),
                               std::move(service_request_cb)) {}

  // Returns previous channel created.
  FakeDynamicChannel* last_channel() { return last_channel_; }

  // Make public for testing.
  using DynamicChannelRegistry::FindAvailableChannelId;
  using DynamicChannelRegistry::FindChannelByLocalId;
  using DynamicChannelRegistry::FindChannelByRemoteId;
  using DynamicChannelRegistry::RequestService;

 private:
  // DynamicChannelRegistry overrides
  DynamicChannelPtr MakeOutbound(PSM psm, ChannelId local_cid, ChannelParameters params) override {
    return MakeChannelInternal(psm, local_cid, kInvalidChannelId);
  }

  DynamicChannelPtr MakeInbound(PSM psm, ChannelId local_cid, ChannelId remote_cid,
                                ChannelParameters params) override {
    auto channel = MakeChannelInternal(psm, local_cid, remote_cid);
    channel->DoConnect(remote_cid);
    return channel;
  }

  std::unique_ptr<FakeDynamicChannel> MakeChannelInternal(PSM psm, ChannelId local_cid,
                                                          ChannelId remote_cid) {
    auto channel = std::make_unique<FakeDynamicChannel>(this, psm, local_cid, remote_cid);
    last_channel_ = channel.get();
    return channel;
  }

  FakeDynamicChannel* last_channel_ = nullptr;
};

// DynamicChannelCallback static handler
void DoNothing(const DynamicChannel* channel) {}

// ServiceRequestCallback static handler
std::optional<DynamicChannelRegistry::ServiceInfo> RejectAllServices(PSM /*psm*/) {
  return std::nullopt;
}

TEST(L2CAP_DynamicChannelRegistryTest, OpenAndRemoteCloseChannel) {
  bool close_cb_called = false;
  auto close_cb = [&close_cb_called](const DynamicChannel* chan) {
    EXPECT_FALSE(close_cb_called);
    close_cb_called = true;
    EXPECT_TRUE(chan);
    EXPECT_FALSE(chan->IsConnected());
    EXPECT_FALSE(chan->IsOpen());
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
  };

  TestDynamicChannelRegistry registry(std::move(close_cb), RejectAllServices);
  EXPECT_EQ(kFirstDynamicChannelId, registry.FindAvailableChannelId());

  bool open_result_cb_called = false;
  auto open_result_cb = [&open_result_cb_called](const DynamicChannel* chan) {
    EXPECT_FALSE(open_result_cb_called);
    open_result_cb_called = true;
    EXPECT_TRUE(chan);
    EXPECT_EQ(kPsm, chan->psm());
    EXPECT_EQ(kLocalCId, chan->local_cid());
    EXPECT_EQ(kRemoteCId, chan->remote_cid());
  };

  registry.OpenOutbound(kPsm, kChannelParams, std::move(open_result_cb));
  registry.last_channel()->DoConnect(kRemoteCId);
  registry.last_channel()->DoOpen();

  EXPECT_TRUE(open_result_cb_called);
  EXPECT_FALSE(close_cb_called);
  auto channel_by_local_id = registry.FindChannelByLocalId(kLocalCId);
  auto channel_by_remote_id = registry.FindChannelByRemoteId(kRemoteCId);
  EXPECT_TRUE(channel_by_local_id);
  EXPECT_TRUE(channel_by_remote_id);
  EXPECT_EQ(channel_by_local_id, channel_by_remote_id);

  registry.last_channel()->DoRemoteClose();
  EXPECT_TRUE(close_cb_called);
  EXPECT_FALSE(registry.FindChannelByLocalId(kLocalCId));
  EXPECT_FALSE(registry.FindChannelByRemoteId(kRemoteCId));
  EXPECT_EQ(kFirstDynamicChannelId, registry.FindAvailableChannelId());
}

TEST(L2CAP_DynamicChannelRegistryTest, OpenAndLocalCloseChannel) {
  bool registry_close_cb_called = false;
  auto registry_close_cb = [&](const DynamicChannel*) { registry_close_cb_called = true; };

  TestDynamicChannelRegistry registry(std::move(registry_close_cb), RejectAllServices);

  bool open_result_cb_called = false;
  auto open_result_cb = [&open_result_cb_called](const DynamicChannel* chan) {
    EXPECT_FALSE(open_result_cb_called);
    open_result_cb_called = true;
    EXPECT_TRUE(chan);
  };

  registry.OpenOutbound(kPsm, kChannelParams, std::move(open_result_cb));
  registry.last_channel()->DoConnect(kRemoteCId);
  registry.last_channel()->DoOpen();

  EXPECT_TRUE(open_result_cb_called);
  EXPECT_TRUE(registry.FindChannelByLocalId(kLocalCId));
  EXPECT_EQ(kFirstDynamicChannelId + 1, registry.FindAvailableChannelId());

  bool close_cb_called = false;
  registry.CloseChannel(kLocalCId, [&] { close_cb_called = true; });
  EXPECT_FALSE(registry_close_cb_called);
  EXPECT_TRUE(close_cb_called);
  EXPECT_FALSE(registry.FindChannelByLocalId(kLocalCId));
  EXPECT_EQ(kFirstDynamicChannelId, registry.FindAvailableChannelId());
}

TEST(L2CAP_DynamicChannelRegistryTest, RejectServiceRequest) {
  bool service_request_cb_called = false;
  auto service_request_cb = [&service_request_cb_called](PSM psm) {
    EXPECT_FALSE(service_request_cb_called);
    EXPECT_EQ(kPsm, psm);
    service_request_cb_called = true;
    return std::nullopt;
  };

  TestDynamicChannelRegistry registry(DoNothing, std::move(service_request_cb));

  registry.RequestService(kPsm, registry.FindAvailableChannelId(), kRemoteCId);
  EXPECT_TRUE(service_request_cb_called);
  EXPECT_FALSE(registry.last_channel());
  EXPECT_FALSE(registry.FindChannelByLocalId(kLocalCId));
  EXPECT_EQ(kFirstDynamicChannelId, registry.FindAvailableChannelId());
}

TEST(L2CAP_DynamicChannelRegistryTest, AcceptServiceRequestThenOpenOk) {
  bool open_result_cb_called = false;
  DynamicChannelRegistry::DynamicChannelCallback open_result_cb =
      [&open_result_cb_called](const DynamicChannel* chan) {
        EXPECT_FALSE(open_result_cb_called);
        open_result_cb_called = true;
        EXPECT_TRUE(chan);
        EXPECT_EQ(kPsm, chan->psm());
        EXPECT_EQ(kLocalCId, chan->local_cid());
        EXPECT_EQ(kRemoteCId, chan->remote_cid());
      };

  bool service_request_cb_called = false;
  auto service_request_cb = [&service_request_cb_called,
                             open_result_cb = std::move(open_result_cb)](PSM psm) mutable {
    EXPECT_FALSE(service_request_cb_called);
    EXPECT_EQ(kPsm, psm);
    service_request_cb_called = true;
    return DynamicChannelRegistry::ServiceInfo{ChannelParameters(), open_result_cb.share()};
  };

  TestDynamicChannelRegistry registry(DoNothing, std::move(service_request_cb));

  registry.RequestService(kPsm, registry.FindAvailableChannelId(), kRemoteCId);
  EXPECT_TRUE(service_request_cb_called);
  EXPECT_EQ(kFirstDynamicChannelId + 1, registry.FindAvailableChannelId());
  ASSERT_TRUE(registry.last_channel());
  registry.last_channel()->DoOpen();

  EXPECT_TRUE(open_result_cb_called);
  EXPECT_TRUE(registry.FindChannelByLocalId(kLocalCId));
}

TEST(L2CAP_DynamicChannelRegistryTest, AcceptServiceRequestThenOpenFails) {
  bool open_result_cb_called = false;
  DynamicChannelRegistry::DynamicChannelCallback open_result_cb =
      [&open_result_cb_called](const DynamicChannel* chan) { open_result_cb_called = true; };

  bool service_request_cb_called = false;
  auto service_request_cb = [&service_request_cb_called,
                             open_result_cb = std::move(open_result_cb)](PSM psm) mutable {
    EXPECT_FALSE(service_request_cb_called);
    EXPECT_EQ(kPsm, psm);
    service_request_cb_called = true;
    return DynamicChannelRegistry::ServiceInfo{ChannelParameters(), open_result_cb.share()};
  };

  TestDynamicChannelRegistry registry(DoNothing, std::move(service_request_cb));

  registry.RequestService(kPsm, registry.FindAvailableChannelId(), kRemoteCId);
  EXPECT_TRUE(service_request_cb_called);
  ASSERT_TRUE(registry.last_channel());
  registry.last_channel()->DoOpen(false);

  // Don't get channels that failed to open.
  EXPECT_FALSE(open_result_cb_called);
  EXPECT_FALSE(registry.FindChannelByLocalId(kLocalCId));

  // The channel ID should be released upon this failure.
  EXPECT_EQ(kFirstDynamicChannelId, registry.FindAvailableChannelId());
}

TEST(L2CAP_DynamicChannelRegistryTest, DestroyRegistryWithOpenChannelClosesIt) {
  bool close_cb_called = false;
  auto close_cb = [&close_cb_called](const DynamicChannel* chan) { close_cb_called = true; };

  TestDynamicChannelRegistry registry(std::move(close_cb), RejectAllServices);

  bool open_result_cb_called = false;
  auto open_result_cb = [&open_result_cb_called](const DynamicChannel* chan) {
    EXPECT_FALSE(open_result_cb_called);
    open_result_cb_called = true;
    EXPECT_TRUE(chan);
  };

  registry.OpenOutbound(kPsm, kChannelParams, std::move(open_result_cb));
  registry.last_channel()->DoConnect(kRemoteCId);
  registry.last_channel()->DoOpen();

  EXPECT_TRUE(open_result_cb_called);
  EXPECT_TRUE(registry.FindChannelByLocalId(kLocalCId));
  EXPECT_FALSE(close_cb_called);

  // |registry| goes out of scope and FakeDynamicChannel's dtor checks that it
  // is disconnected.
}

TEST(L2CAP_DynamicChannelRegistryTest, ErrorConnectingChannel) {
  bool open_result_cb_called = false;
  auto open_result_cb = [&open_result_cb_called](const DynamicChannel* chan) {
    EXPECT_FALSE(open_result_cb_called);
    open_result_cb_called = true;
    EXPECT_FALSE(chan);
  };
  bool close_cb_called = false;
  auto close_cb = [&close_cb_called](auto) { close_cb_called = true; };

  TestDynamicChannelRegistry registry(std::move(close_cb), RejectAllServices);

  registry.OpenOutbound(kPsm, kChannelParams, std::move(open_result_cb));
  registry.last_channel()->DoOpen(false);

  EXPECT_TRUE(open_result_cb_called);
  EXPECT_FALSE(close_cb_called);
  EXPECT_FALSE(registry.FindChannelByLocalId(kLocalCId));

  // Recycle channel ID immediately for the following channel attempt.
  EXPECT_EQ(kFirstDynamicChannelId, registry.FindAvailableChannelId());
}

TEST(L2CAP_DynamicChannelRegistryTest, ExhaustedChannelIds) {
  int open_result_cb_count = 0;

  // This callback expects the channel to be creatable.
  DynamicChannelRegistry::DynamicChannelCallback success_open_result_cb =
      [&open_result_cb_count](const DynamicChannel* chan) {
        ASSERT_NE(nullptr, chan);
        EXPECT_NE(kInvalidChannelId, chan->local_cid());
        open_result_cb_count++;
      };

  int close_cb_count = 0;
  auto close_cb = [&close_cb_count](auto) { close_cb_count++; };

  TestDynamicChannelRegistry registry(std::move(close_cb), RejectAllServices);

  // Open a lot of channels.
  for (int i = 0; i < kNumChannelsAllowed; i++) {
    registry.OpenOutbound(kPsm + i, kChannelParams, success_open_result_cb.share());
    registry.last_channel()->DoConnect(kRemoteCId + i);
    registry.last_channel()->DoOpen();
  }
  EXPECT_EQ(kNumChannelsAllowed, open_result_cb_count);
  EXPECT_EQ(0, close_cb_count);

  // Ensure that channel IDs are exhausted.
  EXPECT_EQ(kInvalidChannelId, registry.FindAvailableChannelId());

  // This callback expects the channel to fail creation.
  auto fail_open_result_cb = [&open_result_cb_count](const DynamicChannel* chan) {
    EXPECT_FALSE(chan);
    open_result_cb_count++;
  };

  // Try to open a new channel.
  registry.OpenOutbound(kPsm, kChannelParams, std::move(fail_open_result_cb));
  EXPECT_EQ(kNumChannelsAllowed + 1, open_result_cb_count);
  EXPECT_EQ(0, close_cb_count);

  // Close the most recently opened channel.
  auto last_remote_cid = registry.last_channel()->remote_cid();
  registry.last_channel()->DoRemoteClose();
  EXPECT_EQ(1, close_cb_count);
  EXPECT_NE(kInvalidChannelId, registry.FindAvailableChannelId());

  // Try to open a channel again.
  registry.OpenOutbound(kPsm, kChannelParams, success_open_result_cb.share());
  registry.last_channel()->DoConnect(last_remote_cid);
  registry.last_channel()->DoOpen();
  EXPECT_EQ(kNumChannelsAllowed + 2, open_result_cb_count);
  EXPECT_EQ(1, close_cb_count);
}

TEST(L2CAP_DynamicChannelRegistryTest, ChannelIdNotReusedUntilDisconnectionCompletes) {
  TestDynamicChannelRegistry registry(DoNothing, RejectAllServices);

  registry.OpenOutbound(kPsm, kChannelParams, DoNothing);
  registry.last_channel()->DoConnect(kRemoteCId);
  registry.last_channel()->DoOpen();

  ASSERT_TRUE(registry.FindChannelByLocalId(kLocalCId));
  ASSERT_NE(kLocalCId, registry.FindAvailableChannelId());

  // Close the channel but don't let the disconnection complete.
  FakeDynamicChannel* const first_channel = registry.last_channel();
  first_channel->set_defer_disconnect_done_callback();
  bool close_cb_called = false;
  registry.CloseChannel(kLocalCId, [&] { close_cb_called = true; });

  // New channels should not reuse the "mostly disconnected" channel's ID.
  EXPECT_FALSE(close_cb_called);
  EXPECT_NE(kLocalCId, registry.FindAvailableChannelId());
  ASSERT_TRUE(registry.FindChannelByLocalId(kLocalCId));
  EXPECT_FALSE(registry.FindChannelByLocalId(kLocalCId)->IsConnected());

  // Open a new channel and make sure it has a different channel ID.
  bool open_result_cb_called = false;
  auto open_result_cb = [&open_result_cb_called](const DynamicChannel* chan) {
    EXPECT_FALSE(open_result_cb_called);
    open_result_cb_called = true;
    ASSERT_TRUE(chan);
    EXPECT_EQ(kLocalCId + 1, chan->local_cid());
  };
  registry.OpenOutbound(kPsm, kChannelParams, std::move(open_result_cb));
  registry.last_channel()->DoConnect(kRemoteCId + 1);
  registry.last_channel()->DoOpen();
  EXPECT_TRUE(open_result_cb_called);

  // Complete the disconnection for the first channel opened.
  ASSERT_TRUE(first_channel->disconnect_done_callback());
  first_channel->disconnect_done_callback()();
  EXPECT_TRUE(close_cb_called);
  EXPECT_EQ(kLocalCId, registry.FindAvailableChannelId());
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bt
