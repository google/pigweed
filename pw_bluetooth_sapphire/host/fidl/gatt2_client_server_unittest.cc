// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt2_client_server.h"

#include "fuchsia/bluetooth/gatt2/cpp/fidl.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/fake_gatt_fixture.h"
#include "src/lib/testing/loop_fixture/test_loop_fixture.h"

namespace bthost {

namespace {

namespace fb = fuchsia::bluetooth;
namespace fbg = fuchsia::bluetooth::gatt2;

constexpr bt::PeerId kPeerId(1);
constexpr bt::UUID kTestServiceUuid0(uint16_t{0xdead});
constexpr bt::UUID kTestServiceUuid1(uint16_t{0xbeef});
constexpr bt::UUID kTestServiceUuid3(uint16_t{0xbaad});

class Gatt2ClientServerTest : public bt::fidl::testing::FakeGattFixture {
 public:
  Gatt2ClientServerTest() = default;
  ~Gatt2ClientServerTest() override = default;

  void SetUp() override {
    server_ = std::make_unique<Gatt2ClientServer>(kPeerId, gatt()->GetWeakPtr(),
                                                  proxy_.NewRequest(), [this]() {
                                                    error_cb_called_ = true;
                                                    server_.reset();
                                                  });
    proxy_.set_error_handler([this](zx_status_t epitaph) { proxy_epitaph_ = epitaph; });
  }

  fbg::Client* proxy() const { return proxy_.get(); }

  void UnbindProxy() { proxy_.Unbind(); }

  std::optional<zx_status_t> proxy_epitaph() const { return proxy_epitaph_; }

  bool server_error_cb_called() const { return error_cb_called_; }

 private:
  std::unique_ptr<Gatt2ClientServer> server_;
  fbg::ClientPtr proxy_;
  bool error_cb_called_ = false;
  std::optional<zx_status_t> proxy_epitaph_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(Gatt2ClientServerTest);
};

TEST_F(Gatt2ClientServerTest, FidlClientClosingProxyCallsServerErrorCallback) {
  UnbindProxy();
  RunLoopUntilIdle();
  EXPECT_TRUE(server_error_cb_called());
}

TEST_F(Gatt2ClientServerTest, WatchServicesListsServicesOnFirstRequestAndUpdatesOnSecondRequest) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);
  ASSERT_TRUE(updated[0].has_kind());
  EXPECT_EQ(updated[0].kind(), fbg::ServiceKind::PRIMARY);
  ASSERT_TRUE(updated[0].has_type());
  EXPECT_EQ(bt::UUID(updated[0].type().value), kTestServiceUuid0);
  EXPECT_FALSE(updated[0].has_characteristics());
  EXPECT_FALSE(updated[0].has_includes());

  updated.clear();
  removed.clear();
  watch_cb_count = 0;
  // WatchServices before service update. Request should be queued.
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 0);
  EXPECT_EQ(updated.size(), 0u);
  EXPECT_EQ(removed.size(), 0u);

  const bt::att::Handle kSvcStartHandle1(2);
  const bt::att::Handle kSvcEndHandle1(kSvcStartHandle1);
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1, /*notify=*/true);

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  EXPECT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle1);
  ASSERT_TRUE(updated[0].has_kind());
  EXPECT_EQ(updated[0].kind(), fbg::ServiceKind::PRIMARY);
  ASSERT_TRUE(updated[0].has_type());
  EXPECT_EQ(bt::UUID(updated[0].type().value), kTestServiceUuid0);
  EXPECT_FALSE(updated[0].has_characteristics());
  EXPECT_FALSE(updated[0].has_includes());
}

TEST_F(Gatt2ClientServerTest,
       WatchServicesWithUuidsListsServicesOnFirstRequestAndUpdatesOnSecondRequest) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  const bt::att::Handle kSvcStartHandle1(2);
  const bt::att::Handle kSvcEndHandle1(kSvcStartHandle1);
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1,
                                   kTestServiceUuid1);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(
      /*uuids=*/{fb::Uuid{kTestServiceUuid0.value()}},
      [&](std::vector<fbg::ServiceInfo> cb_updated, std::vector<fbg::Handle> cb_removed) {
        watch_cb_count++;
        updated = std::move(cb_updated);
        removed = std::move(cb_removed);
      });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);

  updated.clear();
  removed.clear();
  watch_cb_count = 0;

  // WatchServices before service update. Request should be queued.
  proxy()->WatchServices(
      /*uuids=*/{fb::Uuid{kTestServiceUuid0.value()}},
      [&](std::vector<fbg::ServiceInfo> cb_updated, std::vector<fbg::Handle> cb_removed) {
        watch_cb_count++;
        updated = std::move(cb_updated);
        removed = std::move(cb_removed);
      });

  // Service should be ignored because UUID does not match.
  const bt::att::Handle kSvcStartHandle2(3);
  const bt::att::Handle kSvcEndHandle2(kSvcStartHandle2);
  bt::gatt::ServiceData svc_data_2(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle2, kSvcEndHandle2,
                                   kTestServiceUuid1);
  fake_gatt()->AddPeerService(kPeerId, svc_data_2, /*notify=*/true);
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 0);
  EXPECT_EQ(updated.size(), 0u);
  EXPECT_EQ(removed.size(), 0u);

  // Service UUID matches, so response should be received.
  const bt::att::Handle kSvcStartHandle3(4);
  const bt::att::Handle kSvcEndHandle3(kSvcStartHandle3);
  bt::gatt::ServiceData svc_data_3(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle3, kSvcEndHandle3,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_3, /*notify=*/true);
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  EXPECT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle3);
}

TEST_F(Gatt2ClientServerTest, ServiceWatcherResultsIgnoredBeforeWatchServices) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);

  const bt::att::Handle kSvcStartHandle1(2);
  const bt::att::Handle kSvcEndHandle1(kSvcStartHandle1);
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1,
                                   kTestServiceUuid0);

  // Notifications should be ignored.
  fake_gatt()->AddPeerService(kPeerId, svc_data_0, /*notify=*/true);
  fake_gatt()->RemovePeerService(kPeerId, kSvcStartHandle0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1, /*notify=*/true);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  // Since removal notification was ignored, the removed list should be empty.
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle1);
}

TEST_F(Gatt2ClientServerTest, RemoveConnectedServiceClosesRemoteServiceAndNotifiesServiceWatcher) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);

  fbg::RemoteServicePtr service_ptr;
  proxy()->ConnectToService(updated[0].handle(), service_ptr.NewRequest());
  std::optional<zx_status_t> service_error;
  service_ptr.set_error_handler([&](zx_status_t status) { service_error = status; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_error);

  fake_gatt()->RemovePeerService(kPeerId, kSvcStartHandle0);

  updated.clear();
  removed.clear();
  watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 1u);
  ASSERT_EQ(updated.size(), 0u);
  EXPECT_EQ(removed[0].value, kSvcStartHandle0);
  ASSERT_TRUE(service_error.has_value());
  EXPECT_EQ(service_error.value(), ZX_ERR_CONNECTION_RESET);
}

TEST_F(Gatt2ClientServerTest, ModifiedService) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);

  updated.clear();
  removed.clear();
  watch_cb_count = 0;

  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 0);

  // Adding same service will send "modified" service to service watcher.
  fake_gatt()->AddPeerService(kPeerId, svc_data_0, /*notify=*/true);

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);
}

TEST_F(Gatt2ClientServerTest, ReplacedService) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);

  updated.clear();
  removed.clear();
  watch_cb_count = 0;

  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 0);

  // Adding a service with the same handle but different type will send "removed" + "added" services
  // to service watcher.
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid1);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1, /*notify=*/true);

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  ASSERT_EQ(removed.size(), 1u);
  ASSERT_EQ(updated.size(), 1u);
  EXPECT_EQ(removed[0].value, kSvcStartHandle0);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);
  ASSERT_TRUE(updated[0].has_type());
  EXPECT_EQ(bt::UUID(updated[0].type().value), kTestServiceUuid1);
}

// When a service is added and removed between calls to WatchServices, only the removed handle is
// sent in the response.
TEST_F(Gatt2ClientServerTest, ServiceAddedFollowedByServiceRemovedBetweenWatchServices) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](auto, auto) { watch_cb_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);

  const bt::att::Handle kSvcStartHandle1(2);
  const bt::att::Handle kSvcEndHandle1(kSvcStartHandle1);
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1,
                                   kTestServiceUuid1);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1);
  fake_gatt()->RemovePeerService(kPeerId, kSvcStartHandle1);
  RunLoopUntilIdle();

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](std::vector<fbg::ServiceInfo> cb_updated,
                                           std::vector<fbg::Handle> cb_removed) {
    watch_cb_count++;
    updated = std::move(cb_updated);
    removed = std::move(cb_removed);
  });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 1u);
  ASSERT_EQ(updated.size(), 0u);
  EXPECT_EQ(removed[0].value, kSvcStartHandle1);
}

TEST_F(Gatt2ClientServerTest, WatchServicesCalledTwiceClosesServer) {
  // Prevent GATT::ListServices() from completing so that we can queue a second WatchServices
  // request.
  fake_gatt()->stop_list_services();

  int watch_cb_count_0 = 0;
  int watch_cb_count_1 = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](auto, auto) { watch_cb_count_0++; });
  proxy()->WatchServices(/*uuids=*/{}, [&](auto, auto) { watch_cb_count_1++; });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count_0, 0);
  EXPECT_EQ(watch_cb_count_1, 0);
  EXPECT_TRUE(server_error_cb_called());
  ASSERT_TRUE(proxy_epitaph());
  EXPECT_EQ(proxy_epitaph().value(), ZX_ERR_ALREADY_BOUND);
}

TEST_F(Gatt2ClientServerTest, ListServicesFails) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                 kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data);

  fake_gatt()->set_list_services_status(bt::ToResult(bt::HostError::kPacketMalformed));

  int watch_cb_count = 0;
  proxy()->WatchServices(/*uuids=*/{}, [&](auto, auto) { watch_cb_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 0);
  EXPECT_TRUE(server_error_cb_called());
  ASSERT_TRUE(proxy_epitaph());
  EXPECT_EQ(proxy_epitaph().value(), ZX_ERR_CONNECTION_RESET);
}

TEST_F(Gatt2ClientServerTest, ConnectToServiceInvalidHandle) {
  fbg::ServiceHandle invalid_handle{static_cast<uint64_t>(bt::att::kHandleMax) + 1};
  fbg::RemoteServicePtr service_ptr;
  proxy()->ConnectToService(invalid_handle, service_ptr.NewRequest());

  std::optional<zx_status_t> service_epitaph;
  service_ptr.set_error_handler([&](zx_status_t epitaph) { service_epitaph = epitaph; });

  RunLoopUntilIdle();
  ASSERT_TRUE(service_epitaph);
  EXPECT_EQ(service_epitaph.value(), ZX_ERR_INVALID_ARGS);
}

TEST_F(Gatt2ClientServerTest, ConnectToServiceServiceAlreadyConnected) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                 kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data);

  fbg::RemoteServicePtr service_ptr_0;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_0.NewRequest());

  std::optional<zx_status_t> service_epitaph_0;
  service_ptr_0.set_error_handler([&](zx_status_t epitaph) { service_epitaph_0 = epitaph; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_epitaph_0);

  fbg::RemoteServicePtr service_ptr_1;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_1.NewRequest());

  std::optional<zx_status_t> service_epitaph_1;
  service_ptr_1.set_error_handler([&](zx_status_t epitaph) { service_epitaph_1 = epitaph; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_epitaph_0);
  ASSERT_TRUE(service_epitaph_1);
  EXPECT_EQ(service_epitaph_1.value(), ZX_ERR_ALREADY_EXISTS);
}

TEST_F(Gatt2ClientServerTest, ConnectToServiceNotFoundThenConnectToServiceWithSameHandleSucceeds) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);

  fbg::RemoteServicePtr service_ptr_0;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_0.NewRequest());

  std::optional<zx_status_t> service_epitaph_0;
  service_ptr_0.set_error_handler([&](zx_status_t epitaph) { service_epitaph_0 = epitaph; });

  RunLoopUntilIdle();
  ASSERT_TRUE(service_epitaph_0);
  EXPECT_EQ(service_epitaph_0.value(), ZX_ERR_NOT_FOUND);

  // Add a service with the same handle as the service that was previously not found.
  bt::gatt::ServiceData svc_data(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                 kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data);

  // Connecting to the service after it is added should succeed.
  fbg::RemoteServicePtr service_ptr_1;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_1.NewRequest());

  std::optional<zx_status_t> service_epitaph_1;
  service_ptr_1.set_error_handler([&](zx_status_t epitaph) { service_epitaph_1 = epitaph; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_epitaph_1);
}

TEST_F(Gatt2ClientServerTest, ClientClosesRemoteServiceAndReconnectsFollowedByServiceRemoved) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                 kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data);

  fbg::RemoteServicePtr service_ptr_0;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_0.NewRequest());

  std::optional<zx_status_t> service_epitaph_0;
  service_ptr_0.set_error_handler([&](zx_status_t epitaph) { service_epitaph_0 = epitaph; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_epitaph_0);

  service_ptr_0.Unbind();

  fbg::RemoteServicePtr service_ptr_1;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_1.NewRequest());

  std::optional<zx_status_t> service_epitaph_1;
  service_ptr_1.set_error_handler([&](zx_status_t epitaph) { service_epitaph_1 = epitaph; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_epitaph_1);

  // Server should not crash when both service removed handlers are called (one was registered for
  // each call to ConnectToService). The second handler should do nothing.
  fake_gatt()->RemovePeerService(kPeerId, kSvcStartHandle0);
  RunLoopUntilIdle();
  ASSERT_TRUE(service_epitaph_1);
  EXPECT_EQ(service_epitaph_1.value(), ZX_ERR_CONNECTION_RESET);
}

// The service removed handler should gracefully handle being called after the service has already
// been removed from the FIDL server.
TEST_F(Gatt2ClientServerTest, ClientClosesRemoteServiceFollowedByServiceRemoved) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                 kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data);

  fbg::RemoteServicePtr service_ptr_0;
  proxy()->ConnectToService(fbg::ServiceHandle{kSvcStartHandle0}, service_ptr_0.NewRequest());

  std::optional<zx_status_t> service_epitaph_0;
  service_ptr_0.set_error_handler([&](zx_status_t epitaph) { service_epitaph_0 = epitaph; });

  RunLoopUntilIdle();
  EXPECT_FALSE(service_epitaph_0);

  // Unbinding the client end should remove the service from the server's map.
  service_ptr_0.Unbind();
  RunLoopUntilIdle();

  // Server should not crash when service removed handler is called and finds that the service isn't
  // in the server's map.
  fake_gatt()->RemovePeerService(kPeerId, kSvcStartHandle0);
  RunLoopUntilIdle();
}

TEST_F(Gatt2ClientServerTest,
       WatchServicesWithDifferentUuidsBetweenFirstAndSecondRequestListsServicesOnSecondRequest) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  const bt::att::Handle kSvcStartHandle1(2);
  const bt::att::Handle kSvcEndHandle1(kSvcStartHandle1);
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1,
                                   kTestServiceUuid1);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(
      /*uuids=*/{fb::Uuid{kTestServiceUuid0.value()}},
      [&](std::vector<fbg::ServiceInfo> cb_updated, std::vector<fbg::Handle> cb_removed) {
        watch_cb_count++;
        updated = std::move(cb_updated);
        removed = std::move(cb_removed);
      });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  ASSERT_EQ(updated.size(), 1u);
  ASSERT_TRUE(updated[0].has_handle());
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);

  updated.clear();
  removed.clear();
  watch_cb_count = 0;

  // Service with UUID not in next WatchServices() UUID list should be ignored.
  const bt::att::Handle kSvcStartHandle2(3);
  const bt::att::Handle kSvcEndHandle2(kSvcStartHandle2);
  bt::gatt::ServiceData svc_data_2(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle2, kSvcEndHandle2,
                                   kTestServiceUuid3);
  fake_gatt()->AddPeerService(kPeerId, svc_data_2);

  // UUIDs changed, so WatchServices() should immediately receive a response with all existing
  // services that match UUIDs.
  proxy()->WatchServices(
      /*uuids=*/{fb::Uuid{kTestServiceUuid0.value()}, fb::Uuid{kTestServiceUuid1.value()}},
      [&](std::vector<fbg::ServiceInfo> cb_updated, std::vector<fbg::Handle> cb_removed) {
        watch_cb_count++;
        updated = std::move(cb_updated);
        removed = std::move(cb_removed);
      });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  EXPECT_EQ(updated.size(), 2u);
  ASSERT_TRUE(updated[0].has_handle());
  ASSERT_TRUE(updated[1].has_handle());
  std::sort(updated.begin(), updated.end(), [](fbg::ServiceInfo& a, fbg::ServiceInfo& b) {
    return a.handle().value < b.handle().value;
  });
  EXPECT_EQ(updated[0].handle().value, kSvcStartHandle0);
  EXPECT_EQ(updated[1].handle().value, kSvcStartHandle1);
}

TEST_F(
    Gatt2ClientServerTest,
    WatchServicesWithReorderedUuidsBetweenFirstAndSecondRequestDoesNotListsServicesOnSecondRequest) {
  const bt::att::Handle kSvcStartHandle0(1);
  const bt::att::Handle kSvcEndHandle0(kSvcStartHandle0);
  bt::gatt::ServiceData svc_data_0(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0,
                                   kTestServiceUuid0);
  fake_gatt()->AddPeerService(kPeerId, svc_data_0);

  const bt::att::Handle kSvcStartHandle1(2);
  const bt::att::Handle kSvcEndHandle1(kSvcStartHandle1);
  bt::gatt::ServiceData svc_data_1(bt::gatt::ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1,
                                   kTestServiceUuid1);
  fake_gatt()->AddPeerService(kPeerId, svc_data_1);

  std::vector<fbg::ServiceInfo> updated;
  std::vector<fbg::Handle> removed;
  int watch_cb_count = 0;
  proxy()->WatchServices(
      /*uuids=*/{fb::Uuid{kTestServiceUuid0.value()}, fb::Uuid{kTestServiceUuid1.value()}},
      [&](std::vector<fbg::ServiceInfo> cb_updated, std::vector<fbg::Handle> cb_removed) {
        watch_cb_count++;
        updated = std::move(cb_updated);
        removed = std::move(cb_removed);
      });

  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 1);
  EXPECT_EQ(removed.size(), 0u);
  EXPECT_EQ(updated.size(), 2u);

  updated.clear();
  removed.clear();
  watch_cb_count = 0;

  // UUIDs order changed but set of UUIDs is the same, so WatchServices() should not receive a
  // response.
  proxy()->WatchServices(
      /*uuids=*/{fb::Uuid{kTestServiceUuid1.value()}, fb::Uuid{kTestServiceUuid0.value()}},
      [&](std::vector<fbg::ServiceInfo> cb_updated, std::vector<fbg::Handle> cb_removed) {
        watch_cb_count++;
        updated = std::move(cb_updated);
        removed = std::move(cb_removed);
      });
  RunLoopUntilIdle();
  EXPECT_EQ(watch_cb_count, 0);
}

}  // namespace

}  // namespace bthost
