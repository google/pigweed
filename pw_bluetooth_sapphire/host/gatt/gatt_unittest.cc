// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt.h"

#include "fake_client.h"
#include "lib/gtest/test_loop_fixture.h"
#include "mock_server.h"
#include "src/connectivity/bluetooth/core/bt-host/att/att.h"
#include "src/connectivity/bluetooth/core/bt-host/common/host_error.h"
#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/local_service_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"

namespace bt::gatt::internal {
namespace {

const PeerId kPeerId(1);
constexpr UUID kTestServiceUuid0(uint16_t{0xbeef});

// Factory function for tests of client-facing behavior that don't care about the server
std::unique_ptr<Server> CreateMockServer(PeerId peer_id,
                                         fxl::WeakPtr<LocalServiceManager> local_services) {
  return std::make_unique<testing::MockServer>(peer_id, std::move(local_services));
}

class GattTest : public ::gtest::TestLoopFixture {
 public:
  GattTest() = default;
  ~GattTest() override = default;

 protected:
  struct ServiceWatcherData {
    PeerId peer_id;
    std::vector<att::Handle> removed;
    ServiceList added;
    ServiceList modified;
  };

  void SetUp() override {
    auto client = std::make_unique<testing::FakeClient>(dispatcher());
    fake_client_weak_ = client->AsFakeWeakPtr();
    client_ = std::move(client);
    gatt_ = GATT::Create();
  }

  void TearDown() override {
    // Clear any previous expectations that are based on the ATT Write Request,
    // so that write requests sent during RemoteService::ShutDown() are ignored.
    fake_client()->set_write_request_callback({});
    gatt_.reset();
  }

  GATT* gatt() const { return gatt_.get(); }

  fxl::WeakPtr<testing::FakeClient> fake_client() const { return fake_client_weak_; }
  std::unique_ptr<Client> take_client() { return std::move(client_); }

 private:
  std::unique_ptr<GATT> gatt_;
  std::unique_ptr<Client> client_;
  fxl::WeakPtr<testing::FakeClient> fake_client_weak_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GattTest);
};

TEST_F(GattTest, RemoteServiceWatcherNotifiesAddedModifiedAndRemovedService) {
  const att::Handle kGattSvcStartHandle(1);
  const att::Handle kSvcChangedChrcHandle(2);
  const att::Handle kSvcChangedChrcValueHandle(3);
  const att::Handle kCCCDescriptorHandle(4);
  const att::Handle kGattSvcEndHandle(kCCCDescriptorHandle);

  ServiceData gatt_svc(ServiceKind::PRIMARY, kGattSvcStartHandle, kGattSvcEndHandle,
                       types::kGenericAttributeService);
  CharacteristicData service_changed_chrc(Property::kIndicate, std::nullopt, kSvcChangedChrcHandle,
                                          kSvcChangedChrcValueHandle,
                                          types::kServiceChangedCharacteristic);
  DescriptorData ccc_descriptor(kCCCDescriptorHandle, types::kClientCharacteristicConfig);
  fake_client()->set_services({gatt_svc});
  fake_client()->set_characteristics({service_changed_chrc});
  fake_client()->set_descriptors({ccc_descriptor});

  // Return success when a Service Changed Client Characteristic Config descriptor write is
  // performed.
  int write_request_count = 0;
  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        write_request_count++;
        EXPECT_EQ(kCCCDescriptorHandle, handle);
        status_callback(fitx::ok());
      });

  std::vector<ServiceWatcherData> svc_watcher_data;
  gatt()->RegisterRemoteServiceWatcherForPeer(
      kPeerId, [&](std::vector<att::Handle> removed, ServiceList added, ServiceList modified) {
        svc_watcher_data.push_back(ServiceWatcherData{.peer_id = kPeerId,
                                                      .removed = std::move(removed),
                                                      .added = std::move(added),
                                                      .modified = std::move(modified)});
      });

  gatt()->AddConnection(kPeerId, take_client(), CreateMockServer);
  RunLoopUntilIdle();
  EXPECT_EQ(write_request_count, 0);

  gatt()->DiscoverServices(kPeerId, /*service_uuids=*/{});
  RunLoopUntilIdle();
  EXPECT_EQ(write_request_count, 1);
  ASSERT_EQ(1u, svc_watcher_data.size());
  ASSERT_EQ(1u, svc_watcher_data[0].added.size());
  EXPECT_EQ(kPeerId, svc_watcher_data[0].peer_id);
  EXPECT_EQ(kGattSvcStartHandle, svc_watcher_data[0].added[0]->handle());

  // Add, modify, and remove a service.
  const att::Handle kSvc1StartHandle(5);
  const att::Handle kSvc1EndHandle(kSvc1StartHandle);

  // Add a test service to ensure that service discovery occurs after the Service Changed
  // characteristic is configured.
  ServiceData svc1(ServiceKind::PRIMARY, kSvc1StartHandle, kSvc1EndHandle, kTestServiceUuid0);
  fake_client()->set_services({gatt_svc, svc1});

  // Send a notification that svc1 has been added.
  auto svc_changed_range_buffer = StaticByteBuffer(
      LowerBits(kSvc1StartHandle), UpperBits(kSvc1StartHandle),  // start handle of affected range
      LowerBits(kSvc1EndHandle), UpperBits(kSvc1EndHandle)       // end handle of affected range
  );
  fake_client()->SendNotification(/*indicate=*/true, kSvcChangedChrcValueHandle,
                                  svc_changed_range_buffer, /*maybe_truncated=*/false);
  RunLoopUntilIdle();
  ASSERT_EQ(2u, svc_watcher_data.size());
  ASSERT_EQ(1u, svc_watcher_data[1].added.size());
  EXPECT_EQ(0u, svc_watcher_data[1].removed.size());
  EXPECT_EQ(0u, svc_watcher_data[1].modified.size());
  EXPECT_EQ(kPeerId, svc_watcher_data[1].peer_id);
  EXPECT_EQ(kSvc1StartHandle, svc_watcher_data[1].added[0]->handle());
  bool original_service_removed = false;
  svc_watcher_data[1].added[0]->AddRemovedHandler([&]() { original_service_removed = true; });

  // Send a notification that svc1 has been modified.
  fake_client()->SendNotification(/*indicate=*/true, kSvcChangedChrcValueHandle,
                                  svc_changed_range_buffer, /*maybe_truncated=*/false);
  RunLoopUntilIdle();
  EXPECT_TRUE(original_service_removed);
  ASSERT_EQ(3u, svc_watcher_data.size());
  EXPECT_EQ(0u, svc_watcher_data[2].added.size());
  EXPECT_EQ(0u, svc_watcher_data[2].removed.size());
  ASSERT_EQ(1u, svc_watcher_data[2].modified.size());
  EXPECT_EQ(kPeerId, svc_watcher_data[2].peer_id);
  EXPECT_EQ(kSvc1StartHandle, svc_watcher_data[2].modified[0]->handle());
  bool modified_service_removed = false;
  svc_watcher_data[2].modified[0]->AddRemovedHandler([&]() { modified_service_removed = true; });

  // Remove the service.
  fake_client()->set_services({gatt_svc});

  // Send a notification that svc1 has been removed.
  fake_client()->SendNotification(/*indicate=*/true, kSvcChangedChrcValueHandle,
                                  svc_changed_range_buffer, /*maybe_truncated=*/false);
  RunLoopUntilIdle();
  EXPECT_TRUE(modified_service_removed);
  ASSERT_EQ(4u, svc_watcher_data.size());
  EXPECT_EQ(0u, svc_watcher_data[3].added.size());
  ASSERT_EQ(1u, svc_watcher_data[3].removed.size());
  EXPECT_EQ(0u, svc_watcher_data[3].modified.size());
  EXPECT_EQ(kPeerId, svc_watcher_data[3].peer_id);
  EXPECT_EQ(kSvc1StartHandle, svc_watcher_data[3].removed[0]);
}

// Register 3 service watchers, 2 of which are for the same peer.  Then unregister 2 service
// watchers (one for each peer) and ensure only the third is called.
TEST_F(GattTest, MultipleRegisterRemoteServiceWatcherForPeers) {
  const PeerId kPeerId0(0);
  const PeerId kPeerId1(1);

  // Configure peer 0 with 1 service.
  const att::Handle kSvcStartHandle0(42);
  const att::Handle kSvcEndHandle0(kSvcStartHandle0);
  ServiceData svc_data_0(ServiceKind::PRIMARY, kSvcStartHandle0, kSvcEndHandle0, kTestServiceUuid0);
  auto client_0 = std::make_unique<testing::FakeClient>(dispatcher());
  client_0->set_services({svc_data_0});

  gatt()->AddConnection(kPeerId0, std::move(client_0), CreateMockServer);

  std::vector<ServiceWatcherData> watcher_data_0;
  GATT::RemoteServiceWatcherId id_0 = gatt()->RegisterRemoteServiceWatcherForPeer(
      kPeerId0, [&](std::vector<att::Handle> removed, ServiceList added, ServiceList modified) {
        watcher_data_0.push_back(ServiceWatcherData{.peer_id = kPeerId0,
                                                    .removed = std::move(removed),
                                                    .added = std::move(added),
                                                    .modified = std::move(modified)});
      });

  // Configure peer 1 with gatt service.
  const att::Handle kGattSvcStartHandle(1);
  const att::Handle kSvcChangedChrcHandle(2);
  const att::Handle kSvcChangedChrcValueHandle(3);
  const att::Handle kCCCDescriptorHandle(4);
  const att::Handle kGattSvcEndHandle(kCCCDescriptorHandle);
  ServiceData gatt_svc(ServiceKind::PRIMARY, kGattSvcStartHandle, kGattSvcEndHandle,
                       types::kGenericAttributeService);
  CharacteristicData service_changed_chrc(Property::kIndicate, std::nullopt, kSvcChangedChrcHandle,
                                          kSvcChangedChrcValueHandle,
                                          types::kServiceChangedCharacteristic);
  DescriptorData ccc_descriptor(kCCCDescriptorHandle, types::kClientCharacteristicConfig);
  auto client_1 = std::make_unique<testing::FakeClient>(dispatcher());
  auto client_1_weak = client_1.get();
  client_1->set_services({gatt_svc});
  client_1->set_characteristics({service_changed_chrc});
  client_1->set_descriptors({ccc_descriptor});

  // Return success when a Service Changed Client Characteristic Config descriptor write is
  // performed.
  client_1->set_write_request_callback([&](att::Handle handle, const auto& value,
                                           auto status_callback) { status_callback(fitx::ok()); });

  gatt()->AddConnection(kPeerId1, std::move(client_1), CreateMockServer);

  // Register 2 watchers for kPeerId1.
  std::vector<ServiceWatcherData> watcher_data_1;
  GATT::RemoteServiceWatcherId id_1 = gatt()->RegisterRemoteServiceWatcherForPeer(
      kPeerId1, [&](std::vector<att::Handle> removed, ServiceList added, ServiceList modified) {
        watcher_data_1.push_back(ServiceWatcherData{.peer_id = kPeerId1,
                                                    .removed = std::move(removed),
                                                    .added = std::move(added),
                                                    .modified = std::move(modified)});
      });
  EXPECT_NE(id_0, id_1);

  std::vector<ServiceWatcherData> watcher_data_2;
  gatt()->RegisterRemoteServiceWatcherForPeer(
      kPeerId1, [&](std::vector<att::Handle> removed, ServiceList added, ServiceList modified) {
        watcher_data_2.push_back(ServiceWatcherData{.peer_id = kPeerId1,
                                                    .removed = std::move(removed),
                                                    .added = std::move(added),
                                                    .modified = std::move(modified)});
      });

  // Service discovery should complete and all service watchers should be notified.
  gatt()->DiscoverServices(kPeerId0, /*service_uuids=*/{});
  gatt()->DiscoverServices(kPeerId1, /*service_uuids=*/{});
  RunLoopUntilIdle();
  ASSERT_EQ(watcher_data_0.size(), 1u);
  ASSERT_EQ(watcher_data_0[0].added.size(), 1u);
  EXPECT_EQ(watcher_data_0[0].added[0]->handle(), kSvcStartHandle0);

  ASSERT_EQ(watcher_data_1.size(), 1u);
  ASSERT_EQ(watcher_data_1[0].added.size(), 1u);
  EXPECT_EQ(watcher_data_1[0].added[0]->handle(), kGattSvcStartHandle);

  ASSERT_EQ(watcher_data_2.size(), 1u);
  ASSERT_EQ(watcher_data_2[0].added.size(), 1u);
  EXPECT_EQ(watcher_data_2[0].added[0]->handle(), kGattSvcStartHandle);

  gatt()->UnregisterRemoteServiceWatcher(id_0);
  gatt()->UnregisterRemoteServiceWatcher(id_1);

  const att::Handle kSvcStartHandle1(84);
  const att::Handle kSvcEndHandle1(kSvcStartHandle1);
  ServiceData svc_data_1(ServiceKind::PRIMARY, kSvcStartHandle1, kSvcEndHandle1, kTestServiceUuid0);
  client_1_weak->set_services({gatt_svc, svc_data_1});

  // Send a notification that service kSvcStartHandle1 has been added.
  auto svc_changed_range_buffer = StaticByteBuffer(
      LowerBits(kSvcStartHandle1), UpperBits(kSvcStartHandle1),  // start handle of affected range
      LowerBits(kSvcEndHandle1), UpperBits(kSvcEndHandle1)       // end handle of affected range
  );
  client_1_weak->SendNotification(/*indicate=*/true, kSvcChangedChrcValueHandle,
                                  svc_changed_range_buffer, /*maybe_truncated=*/false);
  RunLoopUntilIdle();
  // Unregistered handlers should not be notified.
  ASSERT_EQ(watcher_data_0.size(), 1u);
  ASSERT_EQ(watcher_data_1.size(), 1u);

  // Still registered handler should be notified of added service.
  ASSERT_EQ(watcher_data_2.size(), 2u);
  ASSERT_EQ(watcher_data_2[1].added.size(), 1u);
  EXPECT_EQ(watcher_data_2[1].added[0]->handle(), kSvcStartHandle1);
}

TEST_F(GattTest, ServiceDiscoveryFailureShutsDownConnection) {
  fxl::WeakPtr<testing::MockServer> mock_server;
  auto mock_server_factory = [&](PeerId peer_id, fxl::WeakPtr<LocalServiceManager> local_services) {
    auto unique_mock_server =
        std::make_unique<testing::MockServer>(peer_id, std::move(local_services));
    mock_server = unique_mock_server->AsMockWeakPtr();
    return unique_mock_server;
  };
  fake_client()->set_discover_services_callback(
      [](ServiceKind kind) { return ToResult(att::ErrorCode::kRequestNotSupported); });
  gatt()->AddConnection(kPeerId, take_client(), std::move(mock_server_factory));
  ASSERT_TRUE(mock_server);
  EXPECT_FALSE(mock_server->was_shut_down());
  gatt()->DiscoverServices(kPeerId, std::vector<UUID>{});
  RunLoopUntilIdle();
  EXPECT_TRUE(mock_server->was_shut_down());
}

class GattTestBoolParam : public GattTest, public ::testing::WithParamInterface<bool> {};

TEST_P(GattTestBoolParam, SendIndicationReceiveResponse) {
  fxl::WeakPtr<testing::MockServer> mock_server;
  auto mock_server_factory = [&](PeerId peer_id, fxl::WeakPtr<LocalServiceManager> local_services) {
    auto unique_mock_server =
        std::make_unique<testing::MockServer>(peer_id, std::move(local_services));
    mock_server = unique_mock_server->AsMockWeakPtr();
    return unique_mock_server;
  };
  gatt()->AddConnection(kPeerId, take_client(), std::move(mock_server_factory));
  ASSERT_TRUE(mock_server);

  // Configure how the mock server handles updates sent from the GATT object.
  IndicationCallback mock_ind_cb = nullptr;
  const std::vector<uint8_t> kIndicateVal{114};  // 114 is arbitrary
  testing::UpdateHandler handler = [&](auto /*ignore*/, auto /*ignore*/, const ByteBuffer& bytes,
                                       IndicationCallback ind_cb) {
    EXPECT_EQ(kIndicateVal, bytes.ToVector());
    mock_ind_cb = std::move(ind_cb);
  };
  mock_server->set_update_handler(std::move(handler));

  // Register an arbitrary service with a single characteristic on which to send indications. This
  // boilerplate isn't strictly necessary as gatt::GATT itself is not responsible for verifying
  // that a service exists before sending an update, but it's a more realistic test.
  const bt::UUID kSvcUuid = bt::UUID(112u);
  auto svc = std::make_unique<Service>(/*primary=*/true, kSvcUuid);
  const IdType kChrcId = 13;
  const bt::UUID kChrcUuid = bt::UUID(113u);
  const att::AccessRequirements kReadPerm, kWritePerm;  // Default is not allowed
  // Allow "update" (i.e. indications / notifications) with no security
  const att::AccessRequirements kUpdatePerm(/*encryption=*/false, /*authentication=*/false,
                                            /*authorization=*/false);
  auto chrc = std::make_unique<Characteristic>(kChrcId, kChrcUuid, Property::kIndicate,
                                               /*extended_properties=*/0, kReadPerm, kWritePerm,
                                               kUpdatePerm);
  svc->AddCharacteristic(std::move(chrc));

  std::optional<IdType> svc_id = std::nullopt;
  auto id_cb = [&svc_id](IdType received_id) {
    EXPECT_NE(kInvalidId, received_id);
    svc_id = received_id;
  };
  gatt()->RegisterService(std::move(svc), std::move(id_cb), NopReadHandler, NopWriteHandler,
                          NopCCCallback);
  RunLoopUntilIdle();
  EXPECT_TRUE(svc_id.has_value());

  std::optional<att::Result<>> indicate_status;
  auto indicate_cb = [&](att::Result<> status) { indicate_status = status; };
  gatt()->SendUpdate(*svc_id, kChrcId, kPeerId, kIndicateVal, std::move(indicate_cb));
  RunLoopUntilIdle();
  EXPECT_TRUE(mock_ind_cb);
  EXPECT_FALSE(indicate_status.has_value());
  if (GetParam()) {
    mock_ind_cb(fitx::ok());
    ASSERT_TRUE(indicate_status.has_value());
    EXPECT_TRUE(indicate_status->is_ok());
  } else {
    mock_ind_cb(ToResult(HostError::kTimedOut));
    ASSERT_TRUE(indicate_status.has_value());
    EXPECT_EQ(ToResult(HostError::kTimedOut), *indicate_status);
  }
}

INSTANTIATE_TEST_SUITE_P(GattTestBoolParamTests, GattTestBoolParam, ::testing::Bool());
}  // namespace
}  // namespace bt::gatt::internal
