// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_service_manager.h"

#include <vector>

#include <fbl/macros.h>
#include <gmock/gmock.h>

#include "fake_client.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/gatt_defs.h"

namespace bt::gatt::internal {
namespace {

using namespace ::testing;

constexpr UUID kTestServiceUuid1(uint16_t{0xbeef});
constexpr UUID kTestServiceUuid2(uint16_t{0xcafe});
constexpr UUID kTestServiceUuid3(uint16_t{0xface});
constexpr UUID kTestUuid3(uint16_t{0xfefe});
constexpr UUID kTestUuid4(uint16_t{0xefef});

// Buffers for descriptor responses.
// ExtendedProperty::kReliableWrite enabled.
const auto kExtendedPropValue = CreateStaticByteBuffer(0x01, 0x00);
const auto kCCCNotifyValue = CreateStaticByteBuffer(0x01, 0x00);
const auto kCCCIndicateValue = CreateStaticByteBuffer(0x02, 0x00);

// Constants used for initializing fake characteristic data.
constexpr att::Handle kStart = 1;
constexpr att::Handle kCharDecl = 2;
constexpr att::Handle kCharValue = 3;
constexpr att::Handle kDesc1 = 4;
constexpr att::Handle kDesc2 = 5;
constexpr att::Handle kEnd = 5;

void NopStatusCallback(att::Status) {}
void NopValueCallback(const ByteBuffer&) {}

class GATT_RemoteServiceManagerTest : public ::gtest::TestLoopFixture {
 public:
  GATT_RemoteServiceManagerTest() = default;
  ~GATT_RemoteServiceManagerTest() override = default;

 protected:
  void SetUp() override {
    auto client = std::make_unique<testing::FakeClient>(dispatcher());
    fake_client_ = client.get();

    mgr_ = std::make_unique<RemoteServiceManager>(std::move(client), dispatcher());
  }

  void TearDown() override {
    // Clear any previous expectations that are based on the ATT Write Request,
    // so that write requests sent during RemoteService::ShutDown() are ignored.
    fake_client()->set_write_request_callback({});
    mgr_ = nullptr;
  }

  // Initializes a RemoteService based on |data|.
  fbl::RefPtr<RemoteService> SetUpFakeService(const ServiceData& data) {
    std::vector<ServiceData> fake_services{{data}};
    fake_client()->set_services(std::move(fake_services));

    mgr()->Initialize(NopStatusCallback);

    ServiceList services;
    mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
      services = std::move(cb_services);
    });

    RunLoopUntilIdle();

    ZX_DEBUG_ASSERT(services.size() == 1u);
    return services[0];
  }

  void SetCharacteristicsAndDescriptors(
      std::vector<CharacteristicData> fake_chrs,
      std::vector<DescriptorData> fake_descrs = std::vector<DescriptorData>()) {
    fake_client()->set_characteristics(std::move(fake_chrs));
    fake_client()->set_descriptors(std::move(fake_descrs));
    fake_client()->set_characteristic_discovery_status(att::Status());
  }

  // Discover the characteristics of |service| based on the given |fake_data|.
  void SetupCharacteristics(
      fbl::RefPtr<RemoteService> service, std::vector<CharacteristicData> fake_chrs,
      std::vector<DescriptorData> fake_descrs = std::vector<DescriptorData>()) {
    ZX_DEBUG_ASSERT(service);

    SetCharacteristicsAndDescriptors(std::move(fake_chrs), std::move(fake_descrs));

    service->DiscoverCharacteristics([](auto, const auto&) {});
    RunLoopUntilIdle();
  }

  fbl::RefPtr<RemoteService> SetupServiceWithChrcs(
      const ServiceData& data, std::vector<CharacteristicData> fake_chrs,
      std::vector<DescriptorData> fake_descrs = std::vector<DescriptorData>()) {
    auto service = SetUpFakeService(data);
    SetupCharacteristics(service, fake_chrs, fake_descrs);
    return service;
  }

  // Create a fake service with one notifiable characteristic.
  fbl::RefPtr<RemoteService> SetupNotifiableService() {
    ServiceData data(ServiceKind::PRIMARY, 1, 4, kTestServiceUuid1);
    auto service = SetUpFakeService(data);

    CharacteristicData chr(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
    DescriptorData desc(4, types::kClientCharacteristicConfig);
    SetupCharacteristics(service, {{chr}}, {{desc}});

    fake_client()->set_write_request_callback(
        [&](att::Handle, const auto&, auto status_callback) { status_callback(att::Status()); });

    RunLoopUntilIdle();

    return service;
  }

  void EnableNotifications(fbl::RefPtr<RemoteService> service, CharacteristicHandle chr_id,
                           att::Status* out_status, IdType* out_id,
                           RemoteService::ValueCallback callback = NopValueCallback) {
    ZX_DEBUG_ASSERT(out_status);
    ZX_DEBUG_ASSERT(out_id);
    service->EnableNotifications(chr_id, std::move(callback),
                                 [&](att::Status cb_status, IdType cb_id) {
                                   *out_status = cb_status;
                                   *out_id = cb_id;
                                 });
    RunLoopUntilIdle();
  }

  RemoteServiceManager* mgr() const { return mgr_.get(); }
  testing::FakeClient* fake_client() const { return fake_client_; }

 private:
  std::unique_ptr<RemoteServiceManager> mgr_;

  // The memory is owned by |mgr_|.
  testing::FakeClient* fake_client_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(GATT_RemoteServiceManagerTest);
};

TEST_F(GATT_RemoteServiceManagerTest, InitializeNoServices) {
  std::vector<fbl::RefPtr<RemoteService>> services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_TRUE(services.empty());

  mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
    services = std::move(cb_services);
  });
  EXPECT_TRUE(services.empty());
}

TEST_F(GATT_RemoteServiceManagerTest, Initialize) {
  ServiceData svc1(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  ServiceData svc2(ServiceKind::PRIMARY, 2, 2, kTestServiceUuid2);
  std::vector<ServiceData> fake_services{{svc1, svc2}};
  fake_client()->set_services(std::move(fake_services));

  ServiceList services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(2u, services.size());
  EXPECT_EQ(svc1.range_start, services[0]->handle());
  EXPECT_EQ(svc2.range_start, services[1]->handle());
  EXPECT_EQ(svc1.type, services[0]->uuid());
  EXPECT_EQ(svc2.type, services[1]->uuid());
}

TEST_F(GATT_RemoteServiceManagerTest, InitializeFailure) {
  fake_client()->set_primary_service_discovery_status(
      att::Status(att::ErrorCode::kRequestNotSupported));

  ServiceList watcher_services;
  mgr()->set_service_watcher([&watcher_services](auto svc) { watcher_services.push_back(svc); });

  ServiceList services;
  mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
    services = std::move(cb_services);
  });
  ASSERT_TRUE(services.empty());

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(att::ErrorCode::kRequestNotSupported, status.protocol_error());
  EXPECT_TRUE(services.empty());
  EXPECT_TRUE(watcher_services.empty());
}

TEST_F(GATT_RemoteServiceManagerTest, InitializeByUUIDNoServices) {
  std::vector<fbl::RefPtr<RemoteService>> services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; }, {kTestServiceUuid1});

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_TRUE(services.empty());

  mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
    services = std::move(cb_services);
  });
  EXPECT_TRUE(services.empty());
}

TEST_F(GATT_RemoteServiceManagerTest, InitializeWithUuids) {
  ServiceData svc1(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  ServiceData svc2(ServiceKind::PRIMARY, 2, 2, kTestServiceUuid2);
  ServiceData svc3(ServiceKind::PRIMARY, 3, 3, kTestServiceUuid3);
  std::vector<ServiceData> fake_services{{svc1, svc2, svc3}};
  fake_client()->set_services(std::move(fake_services));

  ServiceList services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; },
                    {kTestServiceUuid1, kTestServiceUuid3});

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_THAT(services,
              UnorderedElementsAre(Pointee(::testing::Property(&RemoteService::info, Eq(svc1))),
                                   Pointee(::testing::Property(&RemoteService::info, Eq(svc3)))));
}

TEST_F(GATT_RemoteServiceManagerTest, InitializeByUUIDFailure) {
  fake_client()->set_primary_service_discovery_status(
      att::Status(att::ErrorCode::kRequestNotSupported));

  ServiceList watcher_services;
  mgr()->set_service_watcher([&watcher_services](auto svc) { watcher_services.push_back(svc); });

  ServiceList services;
  mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
    services = std::move(cb_services);
  });
  ASSERT_TRUE(services.empty());

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; }, {kTestServiceUuid1});

  RunLoopUntilIdle();

  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(att::ErrorCode::kRequestNotSupported, status.protocol_error());
  EXPECT_TRUE(services.empty());
  EXPECT_TRUE(watcher_services.empty());
}

TEST_F(GATT_RemoteServiceManagerTest, InitializeSecondaryServices) {
  ServiceData svc(ServiceKind::SECONDARY, 1, 1, kTestServiceUuid1);
  std::vector<ServiceData> fake_services{{svc}};
  fake_client()->set_services(std::move(fake_services));

  ServiceList services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, services.size());
  EXPECT_EQ(svc.range_start, services[0]->handle());
  EXPECT_EQ(svc.type, services[0]->uuid());
  EXPECT_EQ(ServiceKind::SECONDARY, services[0]->info().kind);
}

TEST_F(GATT_RemoteServiceManagerTest, InitializePrimaryAndSecondaryServices) {
  ServiceData svc1(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  ServiceData svc2(ServiceKind::SECONDARY, 2, 2, kTestServiceUuid2);
  std::vector<ServiceData> fake_services{{svc1, svc2}};
  fake_client()->set_services(std::move(fake_services));

  ServiceList services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(2u, services.size());
  EXPECT_EQ(ServiceKind::PRIMARY, services[0]->info().kind);
  EXPECT_EQ(ServiceKind::SECONDARY, services[1]->info().kind);
}

TEST_F(GATT_RemoteServiceManagerTest, InitializePrimaryAndSecondaryServicesOutOfOrder) {
  // RemoteServiceManager discovers primary services first, followed by secondary services. Test
  // that the results are stored and represented in the correct order when a secondary service
  // precedes a primary service.
  ServiceData svc1(ServiceKind::SECONDARY, 1, 1, kTestServiceUuid1);
  ServiceData svc2(ServiceKind::PRIMARY, 2, 2, kTestServiceUuid2);
  fake_client()->set_services({{svc1, svc2}});

  ServiceList services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });
  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(2u, services.size());
  EXPECT_EQ(ServiceKind::SECONDARY, services[0]->info().kind);
  EXPECT_EQ(ServiceKind::PRIMARY, services[1]->info().kind);
}

// Tests that an ATT error that occurs during secondary service aborts initialization.
TEST_F(GATT_RemoteServiceManagerTest, InitializeSecondaryServicesFailure) {
  fake_client()->set_secondary_service_discovery_status(
      att::Status(att::ErrorCode::kRequestNotSupported));

  ServiceList watcher_services;
  mgr()->set_service_watcher([&watcher_services](auto svc) { watcher_services.push_back(svc); });

  att::Status status;
  mgr()->Initialize([&status](att::Status val) { status = val; });
  RunLoopUntilIdle();

  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(att::ErrorCode::kRequestNotSupported, status.protocol_error());
  EXPECT_TRUE(watcher_services.empty());
}

// Tests that the "unsupported group type" error is treated as a failure for primary services.
TEST_F(GATT_RemoteServiceManagerTest, InitializePrimaryServicesErrorUnsupportedGroupType) {
  fake_client()->set_primary_service_discovery_status(
      att::Status(att::ErrorCode::kUnsupportedGroupType));

  ServiceList watcher_services;
  mgr()->set_service_watcher([&watcher_services](auto svc) { watcher_services.push_back(svc); });

  att::Status status;
  mgr()->Initialize([&status](att::Status val) { status = val; });
  RunLoopUntilIdle();

  EXPECT_FALSE(status);
  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(att::ErrorCode::kUnsupportedGroupType, status.protocol_error());
  EXPECT_TRUE(watcher_services.empty());
}

// Tests that the "unsupported group type" error is NOT treated as a failure for secondary services.
TEST_F(GATT_RemoteServiceManagerTest,
       InitializeSecondaryServicesErrorUnsupportedGroupTypeIsIgnored) {
  ServiceData svc1(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  fake_client()->set_services({{svc1}});
  fake_client()->set_secondary_service_discovery_status(
      att::Status(att::ErrorCode::kUnsupportedGroupType));

  ServiceList watcher_services;
  mgr()->set_service_watcher([&watcher_services](auto svc) { watcher_services.push_back(svc); });

  att::Status status;
  mgr()->Initialize([&status](att::Status val) { status = val; });
  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, watcher_services.size());
  EXPECT_EQ(svc1, watcher_services[0]->info());
}

TEST_F(GATT_RemoteServiceManagerTest, ListServicesBeforeInit) {
  ServiceData svc(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  std::vector<ServiceData> fake_services{{svc}};
  fake_client()->set_services(std::move(fake_services));

  ServiceList services;
  mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
    services = std::move(cb_services);
  });
  EXPECT_TRUE(services.empty());

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1u, services.size());
  EXPECT_EQ(svc.range_start, services[0]->handle());
  EXPECT_EQ(svc.type, services[0]->uuid());
}

TEST_F(GATT_RemoteServiceManagerTest, ListServicesAfterInit) {
  ServiceData svc(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  std::vector<ServiceData> fake_services{{svc}};
  fake_client()->set_services(std::move(fake_services));

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  ASSERT_TRUE(status);

  ServiceList services;
  mgr()->ListServices(std::vector<UUID>(), [&services](auto status, ServiceList cb_services) {
    services = std::move(cb_services);
  });
  EXPECT_EQ(1u, services.size());
  EXPECT_EQ(svc.range_start, services[0]->handle());
  EXPECT_EQ(svc.type, services[0]->uuid());
}

TEST_F(GATT_RemoteServiceManagerTest, ListServicesByUuid) {
  std::vector<UUID> uuids{kTestServiceUuid1};

  ServiceData svc1(ServiceKind::PRIMARY, 1, 1, kTestServiceUuid1);
  ServiceData svc2(ServiceKind::PRIMARY, 2, 2, kTestServiceUuid2);
  std::vector<ServiceData> fake_services{{svc1, svc2}};
  fake_client()->set_services(std::move(fake_services));

  att::Status list_services_status;
  ServiceList services;
  mgr()->set_service_watcher([&services](auto svc) { services.push_back(svc); });
  mgr()->ListServices(std::move(uuids), [&](att::Status cb_status, ServiceList cb_services) {
    list_services_status = cb_status;
    services = std::move(cb_services);
  });
  ASSERT_TRUE(services.empty());

  att::Status status(HostError::kFailed);
  mgr()->Initialize([&status](att::Status val) { status = val; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_TRUE(list_services_status);
  EXPECT_EQ(1u, services.size());
  EXPECT_EQ(svc1.range_start, services[0]->handle());
  EXPECT_EQ(svc1.type, services[0]->uuid());
}

TEST_F(GATT_RemoteServiceManagerTest, DiscoverCharacteristicsAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  size_t chrcs_size;
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status = cb_status;
    chrcs_size = chrcs.size();
  });

  RunLoopUntilIdle();
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kFailed, status.error());
  EXPECT_EQ(0u, chrcs_size);
  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  EXPECT_FALSE(service->IsDiscovered());
}

TEST_F(GATT_RemoteServiceManagerTest, DiscoverCharacteristicsSuccess) {
  auto data = ServiceData(ServiceKind::PRIMARY, 1, 5, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData fake_chrc1(0, std::nullopt, 2, 3, kTestUuid3);
  CharacteristicData fake_chrc2(0, std::nullopt, 4, 5, kTestUuid4);
  std::vector<CharacteristicData> fake_chrcs{{fake_chrc1, fake_chrc2}};
  fake_client()->set_characteristics(std::move(fake_chrcs));

  std::map<CharacteristicHandle,
           std::pair<CharacteristicData, std::map<DescriptorHandle, DescriptorData>>>
      expected = {{CharacteristicHandle(3), {fake_chrc1, {}}},
                  {CharacteristicHandle(5), {fake_chrc2, {}}}};

  att::Status status1(HostError::kFailed);

  auto cb = [expected](att::Status* status) {
    return [status, expected](att::Status cb_status, const auto& chrcs) {
      *status = cb_status;
      EXPECT_EQ(expected, chrcs);
    };
  };

  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status1 = cb_status;
    EXPECT_EQ(expected, chrcs);
  });

  // Queue a second request.
  att::Status status2(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status2 = cb_status;
    EXPECT_EQ(expected, chrcs);
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  // Only one ATT request should have been made.
  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());

  EXPECT_TRUE(service->IsDiscovered());
  EXPECT_TRUE(status1);
  EXPECT_TRUE(status2);
  EXPECT_EQ(data.range_start, fake_client()->last_chrc_discovery_start_handle());
  EXPECT_EQ(data.range_end, fake_client()->last_chrc_discovery_end_handle());

  // Request discovery again. This should succeed without an ATT request.
  status1 = att::Status(HostError::kFailed);
  service->DiscoverCharacteristics(
      [&status1](att::Status cb_status, const auto&) { status1 = cb_status; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status1);
  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_TRUE(service->IsDiscovered());
}

TEST_F(GATT_RemoteServiceManagerTest, DiscoverCharacteristicsError) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 5, kTestServiceUuid1));

  CharacteristicData chrc1(0, std::nullopt, 2, 3, kTestUuid3);
  CharacteristicData chrc2(0, std::nullopt, 4, 5, kTestUuid4);
  std::vector<CharacteristicData> fake_chrcs{{chrc1, chrc2}};
  fake_client()->set_characteristics(std::move(fake_chrcs));

  fake_client()->set_characteristic_discovery_status(att::Status(HostError::kNotSupported));

  att::Status status1;
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status1 = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  // Queue a second request.
  att::Status status2;
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status2 = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  // Onle one request should have been made.
  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());

  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_FALSE(status1);
  EXPECT_FALSE(status2);
  EXPECT_EQ(HostError::kNotSupported, status1.error());
  EXPECT_EQ(HostError::kNotSupported, status2.error());
}

// Discover descriptors of a service with one characteristic.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsOfOneSuccess) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData fake_chrc(0, std::nullopt, kCharDecl, kCharValue, kTestUuid3);
  fake_client()->set_characteristics({{fake_chrc}});

  DescriptorData fake_desc1(kDesc1, kTestUuid3);
  DescriptorData fake_desc2(kDesc2, kTestUuid4);
  fake_client()->set_descriptors({{fake_desc1, fake_desc2}});

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto chrcs) {
    status = cb_status;
    EXPECT_EQ(1u, chrcs.size());

    std::map<CharacteristicHandle,
             std::pair<CharacteristicData, std::map<DescriptorHandle, DescriptorData>>>
        expected = {{CharacteristicHandle(kCharValue),
                     {fake_chrc, {{kDesc1, fake_desc1}, {kDesc2, fake_desc2}}}}};

    EXPECT_EQ(expected, chrcs);
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_EQ(1u, fake_client()->desc_discovery_count());
  EXPECT_TRUE(service->IsDiscovered());
  EXPECT_TRUE(status);
  EXPECT_EQ(kDesc1, fake_client()->last_desc_discovery_start_handle());
  EXPECT_EQ(kEnd, fake_client()->last_desc_discovery_end_handle());
}

// Discover descriptors of a service with one characteristic.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsOfOneError) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData fake_chrc(0, std::nullopt, kCharDecl, kCharValue, kTestUuid3);
  fake_client()->set_characteristics({{fake_chrc}});

  DescriptorData fake_desc1(kDesc1, kTestUuid3);
  DescriptorData fake_desc2(kDesc2, kTestUuid4);
  fake_client()->set_descriptors({{fake_desc1, fake_desc2}});
  fake_client()->set_descriptor_discovery_status(att::Status(HostError::kNotSupported));

  att::Status status;
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_EQ(1u, fake_client()->desc_discovery_count());
  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kNotSupported, status.error());
}

// Discover descriptors of a service with multiple characteristics
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsOfMultipleSuccess) {
  // Has one descriptor
  CharacteristicData fake_char1(0, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData fake_desc1(4, kTestUuid4);

  // Has no descriptors
  CharacteristicData fake_char2(0, std::nullopt, 5, 6, kTestUuid3);

  // Has two descriptors
  CharacteristicData fake_char3(0, std::nullopt, 7, 8, kTestUuid3);
  DescriptorData fake_desc2(9, kTestUuid4);
  DescriptorData fake_desc3(10, kTestUuid4);

  ServiceData data(ServiceKind::PRIMARY, 1, fake_desc3.handle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);
  fake_client()->set_characteristics({{fake_char1, fake_char2, fake_char3}});
  fake_client()->set_descriptors({{fake_desc1, fake_desc2, fake_desc3}});

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status = cb_status;

    std::map<CharacteristicHandle,
             std::pair<CharacteristicData, std::map<DescriptorHandle, DescriptorData>>>
        expected = {{CharacteristicHandle(3), {fake_char1, {{4, fake_desc1}}}},
                    {CharacteristicHandle(6), {fake_char2, {}}},
                    {CharacteristicHandle(8), {fake_char3, {{9, fake_desc2}, {10, fake_desc3}}}}};

    EXPECT_EQ(expected, chrcs);
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());

  // There should have been two descriptor discovery requests as discovery
  // should have been skipped for characteristic #2 due to its handles.
  EXPECT_EQ(2u, fake_client()->desc_discovery_count());
  EXPECT_TRUE(service->IsDiscovered());
  EXPECT_TRUE(status);
}

// Discover descriptors of a service with multiple characteristics. The first
// request results in an error though others succeed.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsOfMultipleEarlyFail) {
  // Has one descriptor
  CharacteristicData fake_char1(0, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData fake_desc1(4, kTestUuid4);

  // Has no descriptors
  CharacteristicData fake_char2(0, std::nullopt, 5, 6, kTestUuid3);

  // Has two descriptors
  CharacteristicData fake_char3(0, std::nullopt, 7, 8, kTestUuid3);
  DescriptorData fake_desc2(9, kTestUuid4);
  DescriptorData fake_desc3(10, kTestUuid4);

  ServiceData data(ServiceKind::PRIMARY, 1, fake_desc3.handle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);
  fake_client()->set_characteristics({{fake_char1, fake_char2, fake_char3}});
  fake_client()->set_descriptors({{fake_desc1, fake_desc2, fake_desc3}});

  // The first request will fail
  fake_client()->set_descriptor_discovery_status(att::Status(HostError::kNotSupported), 1);

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());

  // There should have been two descriptor discovery requests as discovery
  // should have been skipped for characteristic #2 due to its handles.
  EXPECT_EQ(2u, fake_client()->desc_discovery_count());
  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_EQ(HostError::kNotSupported, status.error());
}

// Discover descriptors of a service with multiple characteristics. The last
// request results in an error while the preceding ones succeed.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsOfMultipleLateFail) {
  // Has one descriptor
  CharacteristicData fake_char1(0, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData fake_desc1(4, kTestUuid4);

  // Has no descriptors
  CharacteristicData fake_char2(0, std::nullopt, 5, 6, kTestUuid3);

  // Has two descriptors
  CharacteristicData fake_char3(0, std::nullopt, 7, 8, kTestUuid3);
  DescriptorData fake_desc2(9, kTestUuid4);
  DescriptorData fake_desc3(10, kTestUuid4);

  ServiceData data(ServiceKind::PRIMARY, 1, fake_desc3.handle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);
  fake_client()->set_characteristics({{fake_char1, fake_char2, fake_char3}});
  fake_client()->set_descriptors({{fake_desc1, fake_desc2, fake_desc3}});

  // The last request will fail
  fake_client()->set_descriptor_discovery_status(att::Status(HostError::kNotSupported), 2);

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto& chrcs) {
    status = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());

  // There should have been two descriptor discovery requests as discovery
  // should have been skipped for characteristic #2 due to its handles.
  EXPECT_EQ(2u, fake_client()->desc_discovery_count());
  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_EQ(HostError::kNotSupported, status.error());
}

// Discover descriptors of a service with extended properties set.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsWithExtendedPropertiesSuccess) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  // The ExtendedProperties of the characteristic is set.
  const Properties props = Property::kExtendedProperties;
  CharacteristicData fake_chrc(props, std::nullopt, kCharDecl, kCharValue, kTestUuid3);

  DescriptorData fake_desc1(kDesc1, types::kCharacteristicExtProperties);
  DescriptorData fake_desc2(kDesc2, kTestUuid4);

  SetCharacteristicsAndDescriptors({fake_chrc}, {fake_desc1, fake_desc2});

  // The callback should be triggered once to read the value of the descriptor containing
  // the ExtendedProperties bitfield.
  size_t read_cb_count = 0;
  auto extended_prop_read_cb = [&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDesc1, handle);
    callback(att::Status(), kExtendedPropValue);
    read_cb_count++;
  };
  fake_client()->set_read_request_callback(std::move(extended_prop_read_cb));

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto chrcs) {
    status = cb_status;
    EXPECT_EQ(1u, chrcs.size());

    CharacteristicData expected_chrc(props, kReliableWrite, kCharDecl, kCharValue, kTestUuid3);
    std::map<CharacteristicHandle,
             std::pair<CharacteristicData, std::map<DescriptorHandle, DescriptorData>>>
        expected = {{CharacteristicHandle(kCharValue),
                     {expected_chrc, {{kDesc1, fake_desc1}, {kDesc2, fake_desc2}}}}};
    EXPECT_EQ(expected, chrcs);

    // Validate that the ExtendedProperties have been written to the |chrcs| returned in the
    // callback.
    CharacteristicData chrc_data = chrcs.at(CharacteristicHandle(kCharValue)).first;
    EXPECT_TRUE(chrc_data.extended_properties.has_value());
    EXPECT_EQ(ExtendedProperty::kReliableWrite, chrc_data.extended_properties.value());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_EQ(1u, fake_client()->desc_discovery_count());
  EXPECT_EQ(1u, read_cb_count);
  EXPECT_TRUE(service->IsDiscovered());
  EXPECT_TRUE(status);
  EXPECT_EQ(kDesc1, fake_client()->last_desc_discovery_start_handle());
  EXPECT_EQ(kEnd, fake_client()->last_desc_discovery_end_handle());
}

// Discover descriptors of a service that doesn't contain the ExtendedProperties bit set,
// but with a descriptor containing an ExtendedProperty value. This is not invalid, as per
// the spec, and so discovery shouldn't fail.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsExtendedPropertiesNotSet) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  // The ExtendedProperties of the characteristic is not set.
  CharacteristicData fake_chrc(0, std::nullopt, kCharDecl, kCharValue, kTestUuid3);
  DescriptorData fake_desc1(kDesc1, types::kCharacteristicExtProperties);
  SetCharacteristicsAndDescriptors({fake_chrc}, {fake_desc1});

  // Callback should not be executed.
  size_t read_cb_count = 0;
  auto extended_prop_read_cb = [&](att::Handle handle, auto callback) {
    callback(att::Status(), kExtendedPropValue);
    read_cb_count++;
  };
  fake_client()->set_read_request_callback(std::move(extended_prop_read_cb));

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto chrcs) {
    status = cb_status;
    EXPECT_EQ(1u, chrcs.size());

    std::map<CharacteristicHandle,
             std::pair<CharacteristicData, std::map<DescriptorHandle, DescriptorData>>>
        expected = {{CharacteristicHandle(kCharValue), {fake_chrc, {{kDesc1, fake_desc1}}}}};

    EXPECT_EQ(expected, chrcs);

    // Validate that the ExtendedProperties has not been updated.
    CharacteristicData chrc_data = chrcs.at(CharacteristicHandle(kCharValue)).first;
    EXPECT_FALSE(chrc_data.extended_properties.has_value());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_EQ(1u, fake_client()->desc_discovery_count());
  EXPECT_EQ(0u, read_cb_count);
  EXPECT_TRUE(service->IsDiscovered());
  EXPECT_TRUE(status);
}

// Discover descriptors of a service with two descriptors containing ExtendedProperties.
// This is invalid, and discovery should fail.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsMultipleExtendedPropertiesError) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  // The ExtendedProperties of the characteristic is set.
  const Properties props = Property::kExtendedProperties;
  CharacteristicData fake_chrc(props, std::nullopt, kCharDecl, kCharValue, kTestUuid3);
  // Two descriptors with ExtProperties.
  DescriptorData fake_desc1(kDesc1, types::kCharacteristicExtProperties);
  DescriptorData fake_desc2(kDesc2, types::kCharacteristicExtProperties);
  SetCharacteristicsAndDescriptors({fake_chrc}, {fake_desc1, fake_desc2});

  size_t read_cb_count = 0;
  auto extended_prop_read_cb = [&](att::Handle handle, auto callback) {
    callback(att::Status(), kExtendedPropValue);
    read_cb_count++;
  };
  fake_client()->set_read_request_callback(std::move(extended_prop_read_cb));

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto chrcs) {
    status = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_EQ(1u, fake_client()->desc_discovery_count());
  EXPECT_EQ(0u, read_cb_count);
  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kFailed, status.error());
}

// Discover descriptors of a service with ExtendedProperties set, but with
// an error when reading the descriptor value. Discovery should fail.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsExtendedPropertiesReadDescValueError) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  // The ExtendedProperties of the characteristic is set.
  const Properties props = Property::kExtendedProperties;
  CharacteristicData fake_chrc(props, std::nullopt, kCharDecl, kCharValue, kTestUuid3);
  DescriptorData fake_desc1(kDesc1, types::kCharacteristicExtProperties);
  DescriptorData fake_desc2(kDesc2, kTestUuid4);
  SetCharacteristicsAndDescriptors({fake_chrc}, {fake_desc1, fake_desc2});

  // The callback should be triggered once to read the value of the descriptor containing
  // the ExtendedProperties bitfield.
  size_t read_cb_count = 0;
  auto extended_prop_read_cb = [&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDesc1, handle);
    callback(att::Status(att::ErrorCode::kReadNotPermitted), BufferView());
    read_cb_count++;
  };
  fake_client()->set_read_request_callback(std::move(extended_prop_read_cb));

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto chrcs) {
    status = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  RunLoopUntilIdle();

  EXPECT_EQ(1u, read_cb_count);
  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kProtocolError, status.error());
}

// Discover descriptors of a service with ExtendedProperties set, but with
// a malformed response when reading the descriptor value. Discovery should fail.
TEST_F(GATT_RemoteServiceManagerTest, DiscoverDescriptorsExtendedPropertiesReadDescInvalidValue) {
  ServiceData data(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  // The ExtendedProperties of the characteristic is set.
  const Properties props = Property::kExtendedProperties;
  CharacteristicData fake_chrc(props, std::nullopt, kCharDecl, kCharValue, kTestUuid3);
  DescriptorData fake_desc1(kDesc1, types::kCharacteristicExtProperties);
  DescriptorData fake_desc2(kDesc2, kTestUuid4);
  SetCharacteristicsAndDescriptors({fake_chrc}, {fake_desc1, fake_desc2});

  // The callback should be triggered once to read the value of the descriptor containing
  // the ExtendedProperties bitfield.
  size_t read_cb_count = 0;
  auto extended_prop_read_cb = [&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDesc1, handle);
    callback(att::Status(), BufferView());  // Invalid return buf
    read_cb_count++;
  };
  fake_client()->set_read_request_callback(std::move(extended_prop_read_cb));

  att::Status status(HostError::kFailed);
  service->DiscoverCharacteristics([&](att::Status cb_status, const auto chrcs) {
    status = cb_status;
    EXPECT_TRUE(chrcs.empty());
  });

  EXPECT_EQ(0u, fake_client()->chrc_discovery_count());
  RunLoopUntilIdle();

  EXPECT_EQ(1u, fake_client()->chrc_discovery_count());
  EXPECT_EQ(1u, fake_client()->desc_discovery_count());
  EXPECT_EQ(1u, read_cb_count);
  EXPECT_FALSE(service->IsDiscovered());
  EXPECT_FALSE(status);
  EXPECT_EQ(HostError::kPacketMalformed, status.error());
}

constexpr CharacteristicHandle kDefaultCharacteristic(3);
constexpr CharacteristicHandle kSecondCharacteristic(6);
constexpr CharacteristicHandle kInvalidCharacteristic(1);

constexpr att::Handle kDefaultChrcValueHandle = 3;

CharacteristicData UnreadableChrc() {
  return CharacteristicData(0, std::nullopt, 2, kDefaultChrcValueHandle, kTestUuid3);
}
CharacteristicData ReadableChrc() {
  return CharacteristicData(Property::kRead, std::nullopt, 2, kDefaultChrcValueHandle, kTestUuid3);
}
CharacteristicData WritableChrc() {
  return CharacteristicData(Property::kWrite, std::nullopt, 2, kDefaultChrcValueHandle, kTestUuid3);
}
CharacteristicData WriteableExtendedPropChrc() {
  auto props = Property::kWrite | Property::kExtendedProperties;
  return CharacteristicData(props, std::nullopt, 2, kDefaultChrcValueHandle, kTestUuid3);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadCharAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  service->ReadCharacteristic(kDefaultCharacteristic,
                              [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadCharWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->ReadCharacteristic(kDefaultCharacteristic,
                              [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadCharNotFound) {
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1), {});
  att::Status status;
  service->ReadCharacteristic(kDefaultCharacteristic,
                              [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadCharNotSupported) {
  auto service = SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 3, kTestServiceUuid1),
                                       {UnreadableChrc()});
  att::Status status;
  service->ReadCharacteristic(kDefaultCharacteristic,
                              [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_EQ(HostError::kNotSupported, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadCharSendsReadRequest) {
  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  const auto kValue = CreateStaticByteBuffer('t', 'e', 's', 't');

  fake_client()->set_read_request_callback([&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDefaultChrcValueHandle, handle);
    callback(att::Status(), kValue);
  });

  att::Status status(HostError::kFailed);
  service->ReadCharacteristic(kDefaultCharacteristic,
                              [&](att::Status cb_status, const auto& value) {
                                status = cb_status;
                                EXPECT_TRUE(ContainersEqual(kValue, value));
                              });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadCharSendsReadRequestWithDispatcher) {
  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  const auto kValue = CreateStaticByteBuffer('t', 'e', 's', 't');

  fake_client()->set_read_request_callback([&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDefaultChrcValueHandle, handle);
    callback(att::Status(), kValue);
  });

  att::Status status(HostError::kFailed);
  service->ReadCharacteristic(
      CharacteristicHandle(kDefaultChrcValueHandle),
      [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_TRUE(ContainersEqual(kValue, value));
      },
      dispatcher());

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  service->ReadLongCharacteristic(CharacteristicHandle(0), 0, 512,
                                  [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->ReadLongCharacteristic(CharacteristicHandle(0), 0, 512,
                                  [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongNotFound) {
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1), {});

  att::Status status;
  service->ReadLongCharacteristic(CharacteristicHandle(0), 0, 512,
                                  [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongNotSupported) {
  auto service = SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 3, kTestServiceUuid1),
                                       {UnreadableChrc()});

  att::Status status;
  service->ReadLongCharacteristic(kDefaultCharacteristic, 0, 512,
                                  [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotSupported, status.error());
}

// 0 is not a valid parameter for the |max_size| field of ReadLongCharacteristic
TEST_F(GATT_RemoteServiceManagerTest, ReadLongMaxSizeZero) {
  auto service = SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 3, kTestServiceUuid1),
                                       {ReadableChrc()});

  att::Status status;
  service->ReadLongCharacteristic(kDefaultCharacteristic, 0, 0,
                                  [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kInvalidParameters, status.error());
}

// The complete attribute value is read in a single request.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongSingleBlob) {
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 1000;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  const auto kValue = CreateStaticByteBuffer('t', 'e', 's', 't');

  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        EXPECT_EQ(kOffset, offset);
        callback(att::Status(), kValue);
      });

  att::Status status(HostError::kFailed);
  service->ReadLongCharacteristic(kDefaultCharacteristic, kOffset, kMaxBytes,
                                  [&](att::Status cb_status, const auto& value) {
                                    status = cb_status;
                                    EXPECT_TRUE(ContainersEqual(kValue, value));
                                  });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongMultipleBlobs) {
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 1000;
  constexpr int kExpectedBlobCount = 4;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  // Create a buffer that will take 4 requests to read. Since the default MTU is
  // 23:
  //   a. The size of |expected_value| is 69.
  //   b. We should read 22 + 22 + 22 + 3 bytes across 4 requests.
  StaticByteBuffer<att::kLEMinMTU * 3> expected_value;

  // Initialize the contents.
  for (size_t i = 0; i < expected_value.size(); ++i) {
    expected_value[i] = i;
  }

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;

        // Return a blob at the given offset with at most MTU - 1 bytes.
        auto blob = expected_value.view(offset, att::kLEMinMTU - 1);
        if (read_blob_count == kExpectedBlobCount) {
          // The final blob should contain 3 bytes.
          EXPECT_EQ(3u, blob.size());
        }

        callback(att::Status(), blob);
      });

  att::Status status(HostError::kFailed);
  service->ReadLongCharacteristic(kDefaultCharacteristic, kOffset, kMaxBytes,
                                  [&](att::Status cb_status, const auto& value) {
                                    status = cb_status;
                                    EXPECT_TRUE(ContainersEqual(expected_value, value));
                                  });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

// Same as ReadLongMultipleBlobs except the characteristic value has a size that
// is a multiple of (ATT_MTU - 1), so that the last read blob request returns 0
// bytes.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongValueExactMultipleOfMTU) {
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 1000;
  constexpr int kExpectedBlobCount = 4;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  // Create a buffer that will take 4 requests to read. Since the default MTU is
  // 23:
  //   a. The size of |expected_value| is 66.
  //   b. We should read 22 + 22 + 22 + 0 bytes across 4 requests.
  StaticByteBuffer<(att::kLEMinMTU - 1) * 3> expected_value;

  // Initialize the contents.
  for (size_t i = 0; i < expected_value.size(); ++i) {
    expected_value[i] = i;
  }

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;

        // Return a blob at the given offset with at most MTU - 1 bytes.
        auto blob = expected_value.view(offset, att::kLEMinMTU - 1);
        if (read_blob_count == kExpectedBlobCount) {
          // The final blob should be empty.
          EXPECT_EQ(0u, blob.size());
        }

        callback(att::Status(), blob);
      });

  att::Status status(HostError::kFailed);
  service->ReadLongCharacteristic(kDefaultCharacteristic, kOffset, kMaxBytes,
                                  [&](att::Status cb_status, const auto& value) {
                                    status = cb_status;
                                    EXPECT_TRUE(ContainersEqual(expected_value, value));
                                  });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

// Same as ReadLongMultipleBlobs but a maximum size is given that is smaller
// than the size of the attribute value.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongMultipleBlobsWithMaxSize) {
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 40;
  constexpr int kExpectedBlobCount = 2;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  StaticByteBuffer<att::kLEMinMTU * 3> expected_value;

  // Initialize the contents.
  for (size_t i = 0; i < expected_value.size(); ++i) {
    expected_value[i] = i;
  }

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;
        callback(att::Status(), expected_value.view(offset, att::kLEMinMTU - 1));
      });

  att::Status status(HostError::kFailed);
  service->ReadLongCharacteristic(
      kDefaultCharacteristic, kOffset, kMaxBytes, [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_TRUE(ContainersEqual(expected_value.view(0, kMaxBytes), value));
      });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

// Same as ReadLongMultipleBlobs but a non-zero offset is given.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongAtOffset) {
  constexpr uint16_t kOffset = 30;
  constexpr size_t kMaxBytes = 1000;
  constexpr int kExpectedBlobCount = 2;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  // Size: 69.
  // Reads starting at offset 30 will return 22 + 17 bytes across 2 requests.
  StaticByteBuffer<att::kLEMinMTU * 3> expected_value;

  // Initialize the contents.
  for (size_t i = 0; i < expected_value.size(); ++i) {
    expected_value[i] = i;
  }

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;
        callback(att::Status(), expected_value.view(offset, att::kLEMinMTU - 1));
      });

  att::Status status(HostError::kFailed);
  service->ReadLongCharacteristic(
      kDefaultCharacteristic, kOffset, kMaxBytes, [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_TRUE(ContainersEqual(expected_value.view(kOffset, kMaxBytes), value));
      });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

// Same as ReadLongAtOffset but a very small max size is given.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongAtOffsetWithMaxBytes) {
  constexpr uint16_t kOffset = 10;
  constexpr size_t kMaxBytes = 34;
  constexpr int kExpectedBlobCount = 2;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  // Size: 69. 4 bytes will be read in a single request starting at index 30.
  // Reads starting at offset 10 will return 12 + 22 bytes across 2 requests. A
  // third read blob should not be sent since this should satisfy |kMaxBytes|.
  StaticByteBuffer<att::kLEMinMTU * 3> expected_value;

  // Initialize the contents.
  for (size_t i = 0; i < expected_value.size(); ++i) {
    expected_value[i] = i;
  }

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;
        callback(att::Status(), expected_value.view(offset, att::kLEMinMTU - 1));
      });

  att::Status status(HostError::kFailed);
  service->ReadLongCharacteristic(
      kDefaultCharacteristic, kOffset, kMaxBytes, [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_TRUE(ContainersEqual(expected_value.view(kOffset, kMaxBytes), value));
      });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongError) {
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 1000;
  constexpr int kExpectedBlobCount = 2;  // The second request will fail.

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  // Make the first blob large enough that it will cause a second read blob
  // request.
  StaticByteBuffer<att::kLEMinMTU - 1> first_blob;

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;
        if (read_blob_count == kExpectedBlobCount) {
          callback(att::Status(att::ErrorCode::kInvalidOffset), BufferView());
        } else {
          callback(att::Status(), first_blob);
        }
      });

  att::Status status;
  service->ReadLongCharacteristic(
      kDefaultCharacteristic, kOffset, kMaxBytes, [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_EQ(0u, value.size());  // No value should be returned on error.
      });

  RunLoopUntilIdle();
  EXPECT_EQ(att::ErrorCode::kInvalidOffset, status.protocol_error());
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

// The service is shut down while before the first read blob response. The
// operation should get canceled.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongShutDownWhileInProgress) {
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 1000;
  constexpr int kExpectedBlobCount = 1;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {ReadableChrc()});

  StaticByteBuffer<att::kLEMinMTU - 1> first_blob;

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        read_blob_count++;

        service->ShutDown();
        callback(att::Status(), first_blob);
      });

  att::Status status;
  service->ReadLongCharacteristic(
      kDefaultCharacteristic, kOffset, kMaxBytes, [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_EQ(0u, value.size());  // No value should be returned on error.
      });

  RunLoopUntilIdle();
  EXPECT_EQ(HostError::kCanceled, status.error());
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeSendsReadRequestsUntilAttributeNotFound) {
  constexpr att::Handle kStartHandle = 1;
  constexpr att::Handle kEndHandle = 5;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  constexpr att::Handle kHandle0 = 2;
  const auto kValue0 = StaticByteBuffer(0x00, 0x01, 0x02);
  const std::vector<Client::ReadByTypeValue> kValues0 = {{kHandle0, kValue0.view()}};

  constexpr att::Handle kHandle1 = 3;
  const auto kValue1 = StaticByteBuffer(0x03, 0x04, 0x05);
  const std::vector<Client::ReadByTypeValue> kValues1 = {{kHandle1, kValue1.view()}};

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        switch (read_count++) {
          case 0:
            EXPECT_EQ(kStartHandle, start);
            callback(fit::ok(kValues0));
            break;
          case 1:
            EXPECT_EQ(kHandle0 + 1, start);
            callback(fit::ok(kValues1));
            break;
          case 2:
            EXPECT_EQ(kHandle1 + 1, start);
            callback(fit::error(
                Client::ReadByTypeError{att::Status(att::ErrorCode::kAttributeNotFound), start}));
            break;
          default:
            FAIL();
        }
      });

  std::optional<att::Status> status;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) {
    status = cb_status;
    ASSERT_EQ(2u, values.size());
    EXPECT_EQ(CharacteristicHandle(kHandle0), values[0].handle);
    ASSERT_TRUE(values[0].result.is_ok());
    EXPECT_TRUE(ContainersEqual(kValue0, *values[0].result.value()));
    EXPECT_EQ(CharacteristicHandle(kHandle1), values[1].handle);
    ASSERT_TRUE(values[1].result.is_ok());
    EXPECT_TRUE(ContainersEqual(kValue1, *values[1].result.value()));
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  // kAttributeNotFound error should be treated as success.
  EXPECT_TRUE(status->is_success()) << bt_str(status.value());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeSendsReadRequestsUntilServiceEndHandle) {
  constexpr att::Handle kStartHandle = 1;
  constexpr att::Handle kEndHandle = 2;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  constexpr att::Handle kHandle = kEndHandle;
  const auto kValue = StaticByteBuffer(0x00, 0x01, 0x02);
  const std::vector<Client::ReadByTypeValue> kValues = {{kHandle, kValue.view()}};

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        EXPECT_EQ(kStartHandle, start);
        EXPECT_EQ(0u, read_count++);
        callback(fit::ok(kValues));
      });

  std::optional<att::Status> status;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) {
    status = cb_status;
    ASSERT_EQ(1u, values.size());
    EXPECT_EQ(CharacteristicHandle(kHandle), values[0].handle);
    ASSERT_TRUE(values[0].result.is_ok());
    EXPECT_TRUE(ContainersEqual(kValue, *values[0].result.value()));
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success()) << bt_str(status.value());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeReturnsReadErrorsWithResults) {
  constexpr att::Handle kStartHandle = 1;
  constexpr att::Handle kEndHandle = 5;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  const std::array<att::ErrorCode, 5> errors = {
      att::ErrorCode::kInsufficientAuthorization, att::ErrorCode::kInsufficientAuthentication,
      att::ErrorCode::kInsufficientEncryptionKeySize, att::ErrorCode::kInsufficientEncryption,
      att::ErrorCode::kReadNotPermitted};

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        if (read_count < errors.size()) {
          EXPECT_EQ(kStartHandle + read_count, start);
          callback(fit::error(Client::ReadByTypeError{att::Status(errors[read_count++]), start}));
        } else {
          FAIL();
        }
      });

  std::optional<att::Status> status;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) {
    status = cb_status;
    ASSERT_EQ(errors.size(), values.size());
    for (size_t i = 0; i < values.size(); i++) {
      SCOPED_TRACE(fxl::StringPrintf("i: %zu", i));
      EXPECT_EQ(CharacteristicHandle(kStartHandle + i), values[i].handle);
      ASSERT_TRUE(values[i].result.is_error());
      EXPECT_EQ(errors[i], values[i].result.error());
    }
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  ASSERT_TRUE(status->is_success());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeReturnsProtocolErrorAfterRead) {
  constexpr att::Handle kStartHandle = 1;
  constexpr att::Handle kEndHandle = 5;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  constexpr att::Handle kHandle = kEndHandle;
  const auto kValue = StaticByteBuffer(0x00, 0x01, 0x02);
  const std::vector<Client::ReadByTypeValue> kValues = {{kHandle, kValue.view()}};

  const std::vector<std::pair<const char*, att::ErrorCode>> general_protocol_errors = {
      {"kRequestNotSupported", att::ErrorCode::kRequestNotSupported},
      {"kInsufficientResources", att::ErrorCode::kInsufficientResources},
      {"kInvalidPDU", att::ErrorCode::kInvalidPDU}};

  for (const auto& [name, code] : general_protocol_errors) {
    SCOPED_TRACE(fxl::StringPrintf("Error Code: %s", name));
    size_t read_count = 0;
    fake_client()->set_read_by_type_request_callback(
        [&, code = code](const UUID& type, att::Handle start, att::Handle end, auto callback) {
          ASSERT_EQ(0u, read_count++);
          switch (read_count++) {
            case 0:
              callback(fit::ok(kValues));
              break;
            case 1:
              callback(fit::error(Client::ReadByTypeError{att::Status(code), std::nullopt}));
              break;
            default:
              FAIL();
          }
        });

    std::optional<att::Status> status;
    service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) {
      status = cb_status;
      EXPECT_EQ(0u, values.size());
    });

    RunLoopUntilIdle();
    ASSERT_TRUE(status.has_value());
    ASSERT_TRUE(!status->is_success());
    ASSERT_TRUE(status->is_protocol_error());
    EXPECT_EQ(code, status->protocol_error());
  }
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeHandlesReadErrorWithMissingHandle) {
  constexpr att::Handle kStartHandle = 1;
  constexpr att::Handle kEndHandle = 5;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        ASSERT_EQ(0u, read_count++);
        callback(fit::error(
            Client::ReadByTypeError{att::Status(att::ErrorCode::kReadNotPermitted), std::nullopt}));
      });

  std::optional<att::Status> status;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  ASSERT_TRUE(!status->is_success());
  ASSERT_TRUE(status->is_protocol_error());
  EXPECT_EQ(att::ErrorCode::kReadNotPermitted, status->protocol_error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeHandlesReadErrorWithOutOfRangeHandle) {
  constexpr att::Handle kStartHandle = 1;
  constexpr att::Handle kEndHandle = 5;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        ASSERT_EQ(0u, read_count++);
        callback(fit::error(Client::ReadByTypeError{att::Status(att::ErrorCode::kReadNotPermitted),
                                                    kEndHandle + 1}));
      });

  std::optional<att::Status> status;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) { status = cb_status; });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  ASSERT_TRUE(!status->is_success());
  EXPECT_EQ(HostError::kPacketMalformed, status->error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeReturnsErrorIfUuidIsInternal) {
  const std::array<UUID, 10> kInternalUuids = {types::kPrimaryService,
                                               types::kSecondaryService,
                                               types::kIncludeDeclaration,
                                               types::kCharacteristicDeclaration,
                                               types::kCharacteristicExtProperties,
                                               types::kCharacteristicUserDescription,
                                               types::kClientCharacteristicConfig,
                                               types::kServerCharacteristicConfig,
                                               types::kCharacteristicFormat,
                                               types::kCharacteristicAggregateFormat};

  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1));

  fake_client()->set_read_by_type_request_callback([&](auto, auto, auto, auto) { ADD_FAILURE(); });

  for (const UUID& uuid : kInternalUuids) {
    std::optional<att::Status> status;
    service->ReadByType(uuid, [&](att::Status cb_status, auto values) {
      status = cb_status;
      EXPECT_EQ(0u, values.size());
    });

    RunLoopUntilIdle();
    ASSERT_TRUE(status.has_value()) << "UUID: " << bt_str(uuid);
    EXPECT_EQ(status->error(), HostError::kInvalidParameters);
  }
}

TEST_F(GATT_RemoteServiceManagerTest, WriteCharAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  service->WriteCharacteristic(kDefaultCharacteristic, std::vector<uint8_t>(),
                               [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteCharWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->WriteCharacteristic(kDefaultCharacteristic, std::vector<uint8_t>(),
                               [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteCharNotFound) {
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1), {});

  att::Status status;
  service->WriteCharacteristic(kDefaultCharacteristic, std::vector<uint8_t>(),
                               [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteCharNotSupported) {
  // No "write" property set.
  auto service = SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 3, kTestServiceUuid1),
                                       {ReadableChrc()});

  att::Status status;
  service->WriteCharacteristic(kDefaultCharacteristic, std::vector<uint8_t>(),
                               [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotSupported, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteCharSendsWriteRequest) {
  const std::vector<uint8_t> kValue{{'t', 'e', 's', 't'}};
  constexpr att::Status kStatus(att::ErrorCode::kWriteNotPermitted);

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {WritableChrc()});

  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kDefaultChrcValueHandle, handle);
        EXPECT_TRUE(std::equal(kValue.begin(), kValue.end(), value.begin(), value.end()));
        status_callback(kStatus);
      });

  att::Status status;
  service->WriteCharacteristic(kDefaultCharacteristic, kValue,
                               [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(kStatus, status);
}

// Tests that a long write is chunked up properly into a series of QueuedWrites
// that will be processed by the client. This tests a non-zero offset.
TEST_F(GATT_RemoteServiceManagerTest, WriteCharLongOffsetSuccess) {
  constexpr uint16_t kOffset = 5;
  constexpr uint16_t kExpectedQueueSize = 4;
  constexpr uint16_t kExpectedFullWriteSize = 18;
  constexpr uint16_t kExpectedFinalWriteSize = 15;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {WritableChrc()});

  // Create a vector that will take 4 requests to write. Since the default MTU
  // is 23:
  //   a. The size of |full_write_value| is 69.
  //   b. att:Handle, |kOffset|, and att::OpCode size is 5 bytes total.
  //   c. We should write 18 + 18 + 18 + 15 bytes across 4 requests.
  //   d. These bytes will be written with offset 5, from (5) to (5+69)
  std::vector<uint8_t> full_write_value(att::kLEMinMTU * 3);

  // Initialize the contents.
  for (size_t i = 0; i < full_write_value.size(); ++i) {
    full_write_value[i] = i;
  }

  uint8_t process_long_write_count = 0;
  fake_client()->set_execute_prepare_writes_callback(
      [&](att::PrepareWriteQueue write_queue, auto callback) {
        EXPECT_EQ(write_queue.size(), kExpectedQueueSize);

        for (int i = 0; i < kExpectedQueueSize; i++) {
          auto write = std::move(write_queue.front());
          write_queue.pop();

          EXPECT_EQ(write.handle(), kDefaultChrcValueHandle);
          EXPECT_EQ(write.offset(), kOffset + (i * kExpectedFullWriteSize));

          // All writes expect the final should be full, the final should be
          // the remainder.
          if (i < kExpectedQueueSize - 1) {
            EXPECT_EQ(write.value().size(), kExpectedFullWriteSize);
          } else {
            EXPECT_EQ(write.value().size(), kExpectedFinalWriteSize);
          }
        }

        process_long_write_count++;

        callback(att::Status());
      });

  ReliableMode mode = ReliableMode::kDisabled;
  att::Status status(HostError::kFailed);
  service->WriteLongCharacteristic(kDefaultCharacteristic, kOffset, full_write_value, mode,
                                   [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(1u, process_long_write_count);
}

TEST_F(GATT_RemoteServiceManagerTest, WriteCharLongAtExactMultipleOfMtu) {
  constexpr uint16_t kOffset = 0;
  constexpr uint16_t kExpectedQueueSize = 4;
  constexpr uint16_t kExpectedFullWriteSize = 18;

  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1),
      {WritableChrc()});

  // Create a vector that will take 4 requests to write. Since the default MTU
  // is 23:
  //   a. The size of |full_write_value| is 72.
  //   b. att:Handle, |kOffset|, and att::OpCode size is 5 bytes total.
  //   c. We should write 18 + 18 + 18 + 18 bytes across 4 requests.
  //   d. These bytes will be written with offset 0, from (0) to (72)
  std::vector<uint8_t> full_write_value((att::kLEMinMTU - 5) * 4);

  // Initialize the contents.
  for (size_t i = 0; i < full_write_value.size(); ++i) {
    full_write_value[i] = i;
  }

  uint8_t process_long_write_count = 0;
  fake_client()->set_execute_prepare_writes_callback(
      [&](att::PrepareWriteQueue write_queue, auto callback) {
        EXPECT_EQ(write_queue.size(), kExpectedQueueSize);

        for (int i = 0; i < kExpectedQueueSize; i++) {
          auto write = std::move(write_queue.front());
          write_queue.pop();

          EXPECT_EQ(write.handle(), kDefaultChrcValueHandle);
          EXPECT_EQ(write.offset(), kOffset + (i * kExpectedFullWriteSize));

          // All writes should be full
          EXPECT_EQ(write.value().size(), kExpectedFullWriteSize);
        }

        process_long_write_count++;

        callback(att::Status());
      });

  ReliableMode mode = ReliableMode::kDisabled;
  att::Status status(HostError::kFailed);
  service->WriteLongCharacteristic(kDefaultCharacteristic, kOffset, full_write_value, mode,
                                   [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(1u, process_long_write_count);
}

// Writing a long characteristic with ReliableMode::Enabled should succeed.
TEST_F(GATT_RemoteServiceManagerTest, WriteCharLongReliableWrite) {
  constexpr uint16_t kOffset = 0;
  constexpr uint16_t kExpectedQueueSize = 1;

  DescriptorData fake_desc1(kDesc1, types::kCharacteristicExtProperties);
  DescriptorData fake_desc2(kDesc2, kTestUuid4);

  // The callback should be triggered once to read the value of the descriptor containing
  // the ExtendedProperties bitfield.
  auto extended_prop_read_cb = [&](att::Handle handle, auto callback) {
    callback(att::Status(), kExtendedPropValue);
  };
  fake_client()->set_read_request_callback(std::move(extended_prop_read_cb));

  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, kStart, kEnd, kTestServiceUuid1),
                            {WriteableExtendedPropChrc()}, {fake_desc1, fake_desc2});

  // Create a vector that will take 1 request to write. Since the default MTU
  // is 23:
  //   a. The size of |full_write_value| is 18.
  //   b. att:Handle, |kOffset|, and att::OpCode size is 5 bytes total.
  //   c. We should write 18 bytes.
  std::vector<uint8_t> full_write_value((att::kLEMinMTU - 5));

  // Initialize the contents.
  for (size_t i = 0; i < full_write_value.size(); ++i) {
    full_write_value[i] = i;
  }

  uint8_t process_long_write_count = 0;
  fake_client()->set_execute_prepare_writes_callback(
      [&](att::PrepareWriteQueue write_queue, auto callback) {
        EXPECT_EQ(write_queue.size(), kExpectedQueueSize);
        process_long_write_count++;
        callback(att::Status());
      });

  ReliableMode mode = ReliableMode::kEnabled;
  att::Status status(HostError::kFailed);
  service->WriteLongCharacteristic(kDefaultCharacteristic, kOffset, full_write_value, mode,
                                   [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(1u, process_long_write_count);
}

TEST_F(GATT_RemoteServiceManagerTest, WriteWithoutResponseNotSupported) {
  ServiceData data(ServiceKind::PRIMARY, 1, 3, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  // No "write" or "write without response" property.
  CharacteristicData chr(0, std::nullopt, 2, 3, kTestUuid3);
  SetupCharacteristics(service, {{chr}});

  bool called = false;
  fake_client()->set_write_without_rsp_callback([&](auto, const auto&) { called = true; });

  service->WriteCharacteristicWithoutResponse(kDefaultCharacteristic, std::vector<uint8_t>());
  RunLoopUntilIdle();
  EXPECT_FALSE(called);
}

TEST_F(GATT_RemoteServiceManagerTest, WriteWithoutResponseSuccessWithWriteWithoutResponseProperty) {
  const std::vector<uint8_t> kValue{{'t', 'e', 's', 't'}};

  CharacteristicData chr(Property::kWriteWithoutResponse, std::nullopt, 2, kDefaultChrcValueHandle,
                         kTestUuid3);
  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1), {chr});

  bool called = false;
  fake_client()->set_write_without_rsp_callback([&](att::Handle handle, const auto& value) {
    EXPECT_EQ(kDefaultChrcValueHandle, handle);
    EXPECT_TRUE(std::equal(kValue.begin(), kValue.end(), value.begin(), value.end()));
    called = true;
  });

  service->WriteCharacteristicWithoutResponse(kDefaultCharacteristic, kValue);
  RunLoopUntilIdle();

  EXPECT_TRUE(called);
}

TEST_F(GATT_RemoteServiceManagerTest, WriteWithoutResponseSuccessWithWriteProperty) {
  const std::vector<uint8_t> kValue{{'t', 'e', 's', 't'}};

  CharacteristicData chr(Property::kWrite, std::nullopt, 2, kDefaultChrcValueHandle, kTestUuid3);
  auto service = SetupServiceWithChrcs(
      ServiceData(ServiceKind::PRIMARY, 1, kDefaultChrcValueHandle, kTestServiceUuid1), {chr});

  bool called = false;
  fake_client()->set_write_without_rsp_callback([&](att::Handle handle, const auto& value) {
    EXPECT_EQ(kDefaultChrcValueHandle, handle);
    EXPECT_TRUE(std::equal(kValue.begin(), kValue.end(), value.begin(), value.end()));
    called = true;
  });

  service->WriteCharacteristicWithoutResponse(kDefaultCharacteristic, kValue);
  RunLoopUntilIdle();

  EXPECT_TRUE(called);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadDescAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  service->ReadDescriptor(0, [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadDescWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->ReadDescriptor(0, [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadDescriptorNotFound) {
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1), {});

  att::Status status;
  service->ReadDescriptor(0, [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadDescSendsReadRequest) {
  // TODO(armansito): Some of the service set up and |status| verification
  // boilerplate could be reduced by factoring them out into helpers on the test
  // harness (also see code review comment in
  // https://fuchsia-review.googlesource.com/c/garnet/+/213794/6/drivers/bluetooth/lib/gatt/remote_service_manager_unittest.cc).
  constexpr att::Handle kValueHandle1 = 3;
  constexpr att::Handle kValueHandle2 = 5;
  constexpr att::Handle kDescrHandle = 6;

  ServiceData data(ServiceKind::PRIMARY, 1, kDescrHandle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData chr1(Property::kRead, std::nullopt, 2, kValueHandle1, kTestUuid3);
  CharacteristicData chr2(Property::kRead, std::nullopt, 4, kValueHandle2, kTestUuid3);
  DescriptorData desc(kDescrHandle, kTestUuid4);
  SetupCharacteristics(service, {{chr1, chr2}}, {{desc}});

  const auto kValue = CreateStaticByteBuffer('t', 'e', 's', 't');

  fake_client()->set_read_request_callback([&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDescrHandle, handle);
    callback(att::Status(), kValue);
  });

  att::Status status(HostError::kFailed);
  service->ReadDescriptor(DescriptorHandle(kDescrHandle),
                          [&](att::Status cb_status, const auto& value) {
                            status = cb_status;
                            EXPECT_TRUE(ContainersEqual(kValue, value));
                          });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadDescSendsReadRequestWithDispatcher) {
  constexpr att::Handle kValueHandle = 3;
  constexpr att::Handle kDescrHandle = 4;

  ServiceData data(ServiceKind::PRIMARY, 1, kDescrHandle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData chr(Property::kRead, std::nullopt, 2, kValueHandle, kTestUuid3);
  DescriptorData desc(kDescrHandle, kTestUuid4);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  const auto kValue = CreateStaticByteBuffer('t', 'e', 's', 't');

  fake_client()->set_read_request_callback([&](att::Handle handle, auto callback) {
    EXPECT_EQ(kDescrHandle, handle);
    callback(att::Status(), kValue);
  });

  att::Status status(HostError::kFailed);
  service->ReadDescriptor(
      DescriptorHandle(kDescrHandle),
      [&](att::Status cb_status, const auto& value) {
        status = cb_status;
        EXPECT_TRUE(ContainersEqual(kValue, value));
      },
      dispatcher());

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongDescWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->ReadLongDescriptor(0, 0, 512,
                              [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, ReadLongDescNotFound) {
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1), {});

  att::Status status;
  service->ReadLongDescriptor(0, 0, 512,
                              [&](att::Status cb_status, const auto&) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

// Tests that ReadLongDescriptor sends Read Blob requests. Other conditions
// around the long read procedure are already covered by the tests for
// ReadLongCharacteristic as the implementations are shared.
TEST_F(GATT_RemoteServiceManagerTest, ReadLongDescriptor) {
  constexpr att::Handle kValueHandle = 3;
  constexpr att::Handle kDescrHandle = 4;
  constexpr uint16_t kOffset = 0;
  constexpr size_t kMaxBytes = 1000;
  constexpr int kExpectedBlobCount = 4;

  ServiceData data(ServiceKind::PRIMARY, 1, kDescrHandle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData chr(Property::kRead, std::nullopt, 2, kValueHandle, kTestUuid3);
  DescriptorData desc(kDescrHandle, kTestUuid4);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  // Create a buffer that will take 4 requests to read. Since the default MTU is
  // 23:
  //   a. The size of |expected_value| is 69.
  //   b. We should read 22 + 22 + 22 + 3 bytes across 4 requests.
  StaticByteBuffer<att::kLEMinMTU * 3> expected_value;

  // Initialize the contents.
  for (size_t i = 0; i < expected_value.size(); ++i) {
    expected_value[i] = i;
  }

  int read_blob_count = 0;
  fake_client()->set_read_blob_request_callback(
      [&](att::Handle handle, uint16_t offset, auto callback) {
        EXPECT_EQ(kDescrHandle, handle);
        read_blob_count++;

        // Return a blob at the given offset with at most MTU - 1 bytes.
        auto blob = expected_value.view(offset, att::kLEMinMTU - 1);
        if (read_blob_count == kExpectedBlobCount) {
          // The final blob should contain 3 bytes.
          EXPECT_EQ(3u, blob.size());
        }

        callback(att::Status(), blob);
      });

  att::Status status(HostError::kFailed);
  service->ReadLongDescriptor(DescriptorHandle(kDescrHandle), kOffset, kMaxBytes,
                              [&](att::Status cb_status, const auto& value) {
                                status = cb_status;
                                EXPECT_TRUE(ContainersEqual(expected_value, value));
                              });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(kExpectedBlobCount, read_blob_count);
}

TEST_F(GATT_RemoteServiceManagerTest, WriteDescAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  service->WriteDescriptor(0, std::vector<uint8_t>(),
                           [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteDescWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->WriteDescriptor(0, std::vector<uint8_t>(),
                           [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteDescNotFound) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));
  SetupCharacteristics(service, std::vector<CharacteristicData>());

  att::Status status;
  service->WriteDescriptor(0, std::vector<uint8_t>(),
                           [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteDescNotAllowed) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 4, kTestServiceUuid1));

  // "CCC" characteristic cannot be written to.
  CharacteristicData chr(0, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc(4, types::kClientCharacteristicConfig);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  att::Status status;
  service->WriteDescriptor(4, std::vector<uint8_t>(),
                           [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotSupported, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, WriteDescSendsWriteRequest) {
  constexpr att::Handle kValueHandle = 3;
  constexpr att::Handle kDescrHandle = 4;
  const std::vector<uint8_t> kValue{{'t', 'e', 's', 't'}};
  const att::Status kStatus(HostError::kNotSupported);

  ServiceData data(ServiceKind::PRIMARY, 1, kDescrHandle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData chr(Property::kWrite, std::nullopt, 2, kValueHandle, kTestUuid3);
  DescriptorData desc(kDescrHandle, kTestUuid4);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kDescrHandle, handle);
        EXPECT_TRUE(std::equal(kValue.begin(), kValue.end(), value.begin(), value.end()));
        status_callback(kStatus);
      });

  att::Status status;
  service->WriteDescriptor(kDescrHandle, kValue,
                           [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_EQ(kStatus, status);
}

// Tests that WriteDescriptor with a long vector is prepared correctly.
// Other conditions around the long write procedure are already covered by the
// tests for WriteCharacteristic as the implementations are shared.
TEST_F(GATT_RemoteServiceManagerTest, WriteDescLongSuccess) {
  constexpr att::Handle kValueHandle = 3;
  constexpr att::Handle kDescrHandle = 4;
  constexpr uint16_t kOffset = 0;
  constexpr uint16_t kExpectedQueueSize = 4;
  constexpr uint16_t kExpectedFullWriteSize = 18;
  constexpr uint16_t kExpectedFinalWriteSize = 15;

  ServiceData data(ServiceKind::PRIMARY, 1, kDescrHandle, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  CharacteristicData chr(Property::kWrite, std::nullopt, 2, kValueHandle, kTestUuid3);
  DescriptorData desc(kDescrHandle, kTestUuid4);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  // Create a vector that will take 4 requests to write. Since the default MTU
  // is 23:
  //   a. The size of |full_write_value| is 69.
  //   b. att:Handle, |kOffset|, and att::OpCode size is 5 bytes total.
  //   c. We should write 18 + 18 + 18 + 15 bytes across 4 requests.
  //   d. These bytes will be written with offset 5, from (5) to (5+69)
  std::vector<uint8_t> full_write_value(att::kLEMinMTU * 3);

  // Initialize the contents.
  for (size_t i = 0; i < full_write_value.size(); ++i) {
    full_write_value[i] = i;
  }

  uint8_t process_long_write_count = 0;
  fake_client()->set_execute_prepare_writes_callback(
      [&](att::PrepareWriteQueue write_queue, auto callback) {
        EXPECT_EQ(write_queue.size(), kExpectedQueueSize);

        att::QueuedWrite prepare_write;
        for (int i = 0; i < kExpectedQueueSize; i++) {
          auto write = std::move(write_queue.front());
          write_queue.pop();

          EXPECT_EQ(write.handle(), kDescrHandle);
          EXPECT_EQ(write.offset(), kOffset + (i * kExpectedFullWriteSize));

          // All writes expect the final should be full, the final should be
          // the remainder.
          if (i < kExpectedQueueSize - 1) {
            EXPECT_EQ(write.value().size(), kExpectedFullWriteSize);
          } else {
            EXPECT_EQ(write.value().size(), kExpectedFinalWriteSize);
          }
        }

        process_long_write_count++;

        callback(att::Status());
      });

  att::Status status(HostError::kFailed);
  service->WriteLongDescriptor(DescriptorHandle(kDescrHandle), kOffset, full_write_value,
                               [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(1u, process_long_write_count);
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsAfterShutDown) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  service->ShutDown();

  att::Status status;
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsWhileNotReady) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1));

  att::Status status;
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsCharNotFound) {
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 2, kTestServiceUuid1), {});

  att::Status status;
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsNoProperties) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 4, kTestServiceUuid1));

  // Has neither the "notify" nor "indicate" property but has a CCC descriptor.
  CharacteristicData chr(Property::kRead, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc(4, types::kClientCharacteristicConfig);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  att::Status status;
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType) { status = cb_status; });
  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotSupported, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsSuccess) {
  constexpr att::Handle kCCCHandle = 4;
  auto service =
      SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, kCCCHandle, kTestServiceUuid1));

  CharacteristicData chr(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc(kCCCHandle, types::kClientCharacteristicConfig);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kCCCHandle, handle);
        EXPECT_TRUE(ContainersEqual(kCCCNotifyValue, value));
        status_callback(att::Status());
      });

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType cb_id) {
                                 status = cb_status;
                                 id = cb_id;
                               });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_NE(kInvalidId, id);
}

TEST_F(GATT_RemoteServiceManagerTest, EnableIndications) {
  constexpr att::Handle kCCCHandle = 4;
  auto service =
      SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, kCCCHandle, kTestServiceUuid1));

  CharacteristicData chr(Property::kIndicate, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc(kCCCHandle, types::kClientCharacteristicConfig);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kCCCHandle, handle);
        EXPECT_TRUE(ContainersEqual(kCCCIndicateValue, value));
        status_callback(att::Status());
      });

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType cb_id) {
                                 status = cb_status;
                                 id = cb_id;
                               });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_NE(kInvalidId, id);
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsError) {
  constexpr att::Handle kCCCHandle = 4;

  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 4, kTestServiceUuid1));

  CharacteristicData chr(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc(kCCCHandle, types::kClientCharacteristicConfig);
  SetupCharacteristics(service, {{chr}}, {{desc}});

  // Should enable notifications
  const auto kExpectedValue = CreateStaticByteBuffer(0x01, 0x00);

  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kCCCHandle, handle);
        EXPECT_TRUE(ContainersEqual(kExpectedValue, value));
        status_callback(att::Status(att::ErrorCode::kUnlikelyError));
      });

  IdType id = kInvalidId;
  att::Status status;
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status cb_status, IdType cb_id) {
                                 status = cb_status;
                                 id = cb_id;
                               });

  RunLoopUntilIdle();

  EXPECT_TRUE(status.is_protocol_error());
  EXPECT_EQ(att::ErrorCode::kUnlikelyError, status.protocol_error());
  EXPECT_EQ(kInvalidId, id);
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsRequestMany) {
  constexpr att::Handle kCCCHandle1 = 4;
  constexpr att::Handle kCCCHandle2 = 7;

  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 7, kTestServiceUuid1));

  // Set up two characteristics
  CharacteristicData chr1(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc1(kCCCHandle1, types::kClientCharacteristicConfig);

  CharacteristicData chr2(Property::kIndicate, std::nullopt, 5, 6, kTestUuid3);
  DescriptorData desc2(kCCCHandle2, types::kClientCharacteristicConfig);

  SetupCharacteristics(service, {{chr1, chr2}}, {{desc1, desc2}});

  int ccc_write_count = 0;
  att::StatusCallback status_callback1, status_callback2;
  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_cb) {
        if (handle == kCCCHandle1) {
          EXPECT_TRUE(ContainersEqual(kCCCNotifyValue, value));
          status_callback1 = std::move(status_cb);
        } else if (handle == kCCCHandle2) {
          EXPECT_TRUE(ContainersEqual(kCCCIndicateValue, value));
          status_callback2 = std::move(status_cb);
        } else {
          ADD_FAILURE() << "Unexpected handle: " << handle;
        }
        ccc_write_count++;
      });

  size_t cb_count = 0u;

  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status status, IdType id) {
                                 cb_count++;
                                 EXPECT_EQ(1u, id);
                                 EXPECT_TRUE(status);
                               });
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status status, IdType id) {
                                 cb_count++;
                                 EXPECT_EQ(2u, id);
                                 EXPECT_TRUE(status);
                               });
  service->EnableNotifications(kSecondCharacteristic, NopValueCallback,
                               [&](att::Status status, IdType id) {
                                 cb_count++;
                                 EXPECT_EQ(1u, id);
                                 EXPECT_TRUE(status);
                               });
  service->EnableNotifications(kSecondCharacteristic, NopValueCallback,
                               [&](att::Status status, IdType id) {
                                 cb_count++;
                                 EXPECT_EQ(2u, id);
                                 EXPECT_TRUE(status);
                               });
  service->EnableNotifications(kSecondCharacteristic, NopValueCallback,
                               [&](att::Status status, IdType id) {
                                 cb_count++;
                                 EXPECT_EQ(3u, id);
                                 EXPECT_TRUE(status);
                               });

  RunLoopUntilIdle();

  // ATT write requests should be sent but none of the notification requests
  // should be resolved.
  EXPECT_EQ(2, ccc_write_count);
  EXPECT_EQ(0u, cb_count);

  // An ATT response should resolve all pending requests for the right
  // characteristic.
  status_callback1(att::Status());
  EXPECT_EQ(2u, cb_count);
  status_callback2(att::Status());
  EXPECT_EQ(5u, cb_count);

  // An extra request should succeed without sending any PDUs.
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback,
                               [&](att::Status status, IdType) {
                                 cb_count++;
                                 EXPECT_TRUE(status);
                               });

  RunLoopUntilIdle();

  EXPECT_EQ(2, ccc_write_count);
  EXPECT_EQ(6u, cb_count);
}

TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsRequestManyError) {
  constexpr att::Handle kCCCHandle = 4;

  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 4, kTestServiceUuid1));

  // Set up two characteristics
  CharacteristicData chr(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc(kCCCHandle, types::kClientCharacteristicConfig);

  SetupCharacteristics(service, {{chr}}, {{desc}});

  int ccc_write_count = 0;
  att::StatusCallback status_callback;
  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_cb) {
        EXPECT_EQ(kCCCHandle, handle);
        EXPECT_TRUE(ContainersEqual(kCCCNotifyValue, value));

        ccc_write_count++;
        status_callback = std::move(status_cb);
      });

  int cb_count = 0;
  att::Status status;
  auto cb = [&](att::Status cb_status, IdType id) {
    status = cb_status;
    cb_count++;
  };

  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback, std::move(cb));
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback, std::move(cb));
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback, std::move(cb));

  RunLoopUntilIdle();

  // Requests should be buffered and only one ATT request should have been sent
  // out.
  EXPECT_EQ(1, ccc_write_count);
  EXPECT_EQ(0, cb_count);

  status_callback(att::Status(HostError::kNotSupported));
  EXPECT_EQ(3, cb_count);
  EXPECT_EQ(HostError::kNotSupported, status.error());

  // A new request should write to the descriptor again.
  service->EnableNotifications(kDefaultCharacteristic, NopValueCallback, std::move(cb));

  RunLoopUntilIdle();

  EXPECT_EQ(2, ccc_write_count);
  EXPECT_EQ(3, cb_count);

  status_callback(att::Status());
  EXPECT_EQ(2, ccc_write_count);
  EXPECT_EQ(4, cb_count);
  EXPECT_TRUE(status);
}

// Enabling notifications should succeed without a descriptor write.
TEST_F(GATT_RemoteServiceManagerTest, EnableNotificationsWithoutCCC) {
  // Has the "notify" property but no CCC descriptor.
  CharacteristicData chr(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
  auto service =
      SetupServiceWithChrcs(ServiceData(ServiceKind::PRIMARY, 1, 3, kTestServiceUuid1), {chr});

  bool write_requested = false;
  fake_client()->set_write_request_callback([&](auto, auto&, auto) { write_requested = true; });

  int notify_count = 0;
  auto notify_cb = [&](const auto& value) { notify_count++; };

  att::Status status;
  IdType id;
  service->EnableNotifications(kDefaultCharacteristic, std::move(notify_cb),
                               [&](att::Status _status, IdType _id) {
                                 status = _status;
                                 id = _id;
                               });
  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_FALSE(write_requested);

  fake_client()->SendNotification(false, 3, StaticByteBuffer('y', 'e'));
  EXPECT_EQ(1, notify_count);

  // Disabling notifications should not result in a write request.
  service->DisableNotifications(kDefaultCharacteristic, id,
                                [&](auto _status) { status = _status; });
  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_FALSE(write_requested);

  // The handler should no longer receive notifications.
  fake_client()->SendNotification(false, 3, StaticByteBuffer('o', 'y', 'e'));
  EXPECT_EQ(1, notify_count);
}

// Notifications received when the remote service database is empty should be
// dropped and not cause a crash.
TEST_F(GATT_RemoteServiceManagerTest, NotificationWithoutServices) {
  for (att::Handle i = 0; i < 10; ++i) {
    fake_client()->SendNotification(false, i, CreateStaticByteBuffer('n', 'o', 't', 'i', 'f', 'y'));
  }
  RunLoopUntilIdle();
}

TEST_F(GATT_RemoteServiceManagerTest, NotificationCallback) {
  auto service = SetUpFakeService(ServiceData(ServiceKind::PRIMARY, 1, 7, kTestServiceUuid1));

  // Set up two characteristics
  CharacteristicData chr1(Property::kNotify, std::nullopt, 2, 3, kTestUuid3);
  DescriptorData desc1(4, types::kClientCharacteristicConfig);

  CharacteristicData chr2(Property::kIndicate, std::nullopt, 5, 6, kTestUuid3);
  DescriptorData desc2(7, types::kClientCharacteristicConfig);

  SetupCharacteristics(service, {{chr1, chr2}}, {{desc1, desc2}});

  fake_client()->set_write_request_callback(
      [&](att::Handle, const auto&, auto status_callback) { status_callback(att::Status()); });

  IdType handler_id = kInvalidId;
  att::Status status(HostError::kFailed);

  int chr1_count = 0;
  auto chr1_cb = [&](const ByteBuffer& value) {
    chr1_count++;
    EXPECT_EQ("notify", value.AsString());
  };

  int chr2_count = 0;
  auto chr2_cb = [&](const ByteBuffer& value) {
    chr2_count++;
    EXPECT_EQ("indicate", value.AsString());
  };

  // Notify both characteristics which should get dropped.
  fake_client()->SendNotification(false, 3, CreateStaticByteBuffer('n', 'o', 't', 'i', 'f', 'y'));
  fake_client()->SendNotification(true, 6,
                                  CreateStaticByteBuffer('i', 'n', 'd', 'i', 'c', 'a', 't', 'e'));

  EnableNotifications(service, kDefaultCharacteristic, &status, &handler_id, std::move(chr1_cb));
  ASSERT_TRUE(status);
  EnableNotifications(service, kSecondCharacteristic, &status, &handler_id, std::move(chr2_cb));
  ASSERT_TRUE(status);

  // Notify characteristic 1.
  fake_client()->SendNotification(false, 3, CreateStaticByteBuffer('n', 'o', 't', 'i', 'f', 'y'));
  EXPECT_EQ(1, chr1_count);
  EXPECT_EQ(0, chr2_count);

  // Notify characteristic 2.
  fake_client()->SendNotification(true, 6,
                                  CreateStaticByteBuffer('i', 'n', 'd', 'i', 'c', 'a', 't', 'e'));
  EXPECT_EQ(1, chr1_count);
  EXPECT_EQ(1, chr2_count);

  // Disable notifications from characteristic 1.
  status = att::Status(HostError::kFailed);
  service->DisableNotifications(kDefaultCharacteristic, handler_id,
                                [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();
  EXPECT_TRUE(status);

  // Notifications for characteristic 1 should get dropped.
  fake_client()->SendNotification(false, 3, CreateStaticByteBuffer('n', 'o', 't', 'i', 'f', 'y'));
  fake_client()->SendNotification(true, 6,
                                  CreateStaticByteBuffer('i', 'n', 'd', 'i', 'c', 'a', 't', 'e'));
  EXPECT_EQ(1, chr1_count);
  EXPECT_EQ(2, chr2_count);
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsAfterShutDown) {
  auto service = SetupNotifiableService();

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  EnableNotifications(service, kDefaultCharacteristic, &status, &id);

  EXPECT_TRUE(status);
  EXPECT_NE(kInvalidId, id);

  service->ShutDown();

  service->DisableNotifications(kDefaultCharacteristic, id,
                                [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kFailed, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsWhileNotReady) {
  ServiceData data(ServiceKind::PRIMARY, 1, 4, kTestServiceUuid1);
  auto service = SetUpFakeService(data);

  att::Status status;
  service->DisableNotifications(kDefaultCharacteristic, 1,
                                [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotReady, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsCharNotFound) {
  auto service = SetupNotifiableService();

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  EnableNotifications(service, kDefaultCharacteristic, &status, &id);

  // "1" is an invalid characteristic ID.
  service->DisableNotifications(kInvalidCharacteristic, id,
                                [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsIdNotFound) {
  auto service = SetupNotifiableService();

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  EnableNotifications(service, kDefaultCharacteristic, &status, &id);

  // Valid characteristic ID but invalid notification handler ID.
  service->DisableNotifications(kDefaultCharacteristic, id + 1,
                                [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_EQ(HostError::kNotFound, status.error());
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsSingleHandler) {
  constexpr att::Handle kCCCHandle = 4;
  auto service = SetupNotifiableService();

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  EnableNotifications(service, kDefaultCharacteristic, &status, &id);

  // Should disable notifications
  const auto kExpectedValue = CreateStaticByteBuffer(0x00, 0x00);

  int ccc_write_count = 0;
  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kCCCHandle, handle);
        EXPECT_TRUE(ContainersEqual(kExpectedValue, value));
        ccc_write_count++;
        status_callback(att::Status());
      });

  status = att::Status(HostError::kFailed);
  service->DisableNotifications(kDefaultCharacteristic, id,
                                [&](att::Status cb_status) { status = cb_status; });

  RunLoopUntilIdle();

  EXPECT_TRUE(status);
  EXPECT_EQ(1, ccc_write_count);
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsDuringShutDown) {
  constexpr att::Handle kCCCHandle = 4;
  auto service = SetupNotifiableService();

  IdType id = kInvalidId;
  att::Status status(HostError::kFailed);
  EnableNotifications(service, kDefaultCharacteristic, &status, &id);
  ASSERT_TRUE(status);

  // Should disable notifications
  const auto kExpectedValue = CreateStaticByteBuffer(0x00, 0x00);

  int ccc_write_count = 0;
  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        EXPECT_EQ(kCCCHandle, handle);
        EXPECT_TRUE(ContainersEqual(kExpectedValue, value));
        ccc_write_count++;
        status_callback(att::Status());
      });

  // Shutting down the service should clear the CCC.
  service->ShutDown();
  RunLoopUntilIdle();

  EXPECT_EQ(1, ccc_write_count);
}

TEST_F(GATT_RemoteServiceManagerTest, DisableNotificationsManyHandlers) {
  auto service = SetupNotifiableService();

  IdType id = kInvalidId;
  std::vector<IdType> handler_ids;

  for (int i = 0; i < 2; i++) {
    att::Status status(HostError::kFailed);
    EnableNotifications(service, kDefaultCharacteristic, &status, &id);
    ASSERT_TRUE(status);
    handler_ids.push_back(id);
  }

  int ccc_write_count = 0;
  fake_client()->set_write_request_callback(
      [&](att::Handle handle, const auto& value, auto status_callback) {
        ccc_write_count++;
        status_callback(att::Status());
      });

  // Disabling should succeed without an ATT transaction.
  att::Status status(HostError::kFailed);
  service->DisableNotifications(kDefaultCharacteristic, handler_ids.back(),
                                [&](att::Status cb_status) { status = cb_status; });
  handler_ids.pop_back();
  RunLoopUntilIdle();
  EXPECT_TRUE(status);
  EXPECT_EQ(0, ccc_write_count);

  // Enabling should succeed without an ATT transaction.
  status = att::Status(HostError::kFailed);
  EnableNotifications(service, kDefaultCharacteristic, &status, &id);
  EXPECT_TRUE(status);
  EXPECT_EQ(0, ccc_write_count);
  handler_ids.push_back(id);

  // Disabling all should send out an ATT transaction.
  while (!handler_ids.empty()) {
    att::Status status(HostError::kFailed);
    service->DisableNotifications(kDefaultCharacteristic, handler_ids.back(),
                                  [&](att::Status cb_status) { status = cb_status; });
    handler_ids.pop_back();
    RunLoopUntilIdle();
    EXPECT_TRUE(status);
  }

  EXPECT_EQ(1, ccc_write_count);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeErrorOnLastHandleDoesNotOverflowHandle) {
  constexpr att::Handle kStartHandle = 0xFFFE;
  constexpr att::Handle kEndHandle = 0xFFFF;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));
  constexpr UUID kCharUuid(uint16_t{0xfefe});

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        ASSERT_EQ(0u, read_count++);
        EXPECT_EQ(kStartHandle, start);
        callback(fit::error(
            Client::ReadByTypeError{att::Status(att::ErrorCode::kReadNotPermitted), kEndHandle}));
      });

  std::optional<att::Status> status;
  std::vector<RemoteService::ReadByTypeResult> results;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto cb_results) {
    status = cb_status;
    results = std::move(cb_results);
  });
  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success());
  ASSERT_EQ(1u, results.size());
  EXPECT_EQ(CharacteristicHandle(kEndHandle), results[0].handle);
  ASSERT_TRUE(results[0].result.is_error());
  EXPECT_EQ(results[0].result.error(), att::ErrorCode::kReadNotPermitted);
}

TEST_F(GATT_RemoteServiceManagerTest, ReadByTypeResultOnLastHandleDoesNotOverflowHandle) {
  constexpr att::Handle kStartHandle = 0xFFFE;
  constexpr att::Handle kEndHandle = 0xFFFF;
  auto service = SetUpFakeService(
      ServiceData(ServiceKind::PRIMARY, kStartHandle, kEndHandle, kTestServiceUuid1));

  constexpr UUID kCharUuid(uint16_t{0xfefe});

  constexpr att::Handle kHandle = kEndHandle;
  const auto kValue = StaticByteBuffer(0x00, 0x01, 0x02);
  const std::vector<Client::ReadByTypeValue> kValues = {{kHandle, kValue.view()}};

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const UUID& type, att::Handle start, att::Handle end, auto callback) {
        ASSERT_EQ(0u, read_count++);
        EXPECT_EQ(kStartHandle, start);
        callback(fit::ok(kValues));
      });

  std::optional<att::Status> status;
  service->ReadByType(kCharUuid, [&](att::Status cb_status, auto values) {
    status = cb_status;
    ASSERT_EQ(1u, values.size());
    EXPECT_EQ(CharacteristicHandle(kHandle), values[0].handle);
    ASSERT_TRUE(values[0].result.is_ok());
    EXPECT_TRUE(ContainersEqual(kValue, *values[0].result.value()));
  });

  RunLoopUntilIdle();
  ASSERT_TRUE(status.has_value());
  EXPECT_TRUE(status->is_success()) << bt_str(status.value());
}

}  // namespace
}  // namespace bt::gatt::internal
