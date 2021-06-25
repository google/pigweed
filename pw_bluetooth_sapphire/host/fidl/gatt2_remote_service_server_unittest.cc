// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt2_remote_service_server.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer_test.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"

namespace bthost {
namespace {

namespace fbg = fuchsia::bluetooth::gatt2;

constexpr bt::PeerId kPeerId(1);

constexpr bt::att::Handle kServiceStartHandle = 0x0001;
constexpr bt::att::Handle kServiceEndHandle = 0x000F;
const bt::UUID kServiceUuid(uint16_t{0x180D});

class FIDL_Gatt2RemoteServiceServerTest : public bt::gatt::testing::FakeLayerTest {
 public:
  FIDL_Gatt2RemoteServiceServerTest() = default;
  ~FIDL_Gatt2RemoteServiceServerTest() override = default;

  void SetUp() override {
    {
      auto [svc, client] = gatt()->AddPeerService(
          kPeerId, bt::gatt::ServiceData(bt::gatt::ServiceKind::PRIMARY, kServiceStartHandle,
                                         kServiceEndHandle, kServiceUuid));
      service_ = std::move(svc);
      fake_client_ = std::move(client);
    }

    fidl::InterfaceHandle<fbg::RemoteService> handle;
    server_ = std::make_unique<Gatt2RemoteServiceServer>(service_, gatt()->AsWeakPtr(), kPeerId,
                                                         handle.NewRequest());
    proxy_.Bind(std::move(handle));
  }

  void TearDown() override {
    // Clear any previous expectations that are based on the ATT Write Request,
    // so that write requests sent during RemoteService::ShutDown() are ignored.
    fake_client()->set_write_request_callback({});

    bt::gatt::testing::FakeLayerTest::TearDown();
  }

 protected:
  bt::gatt::testing::FakeClient* fake_client() const {
    ZX_ASSERT(fake_client_);
    return fake_client_.get();
  }

  fbg::RemoteServicePtr& service_proxy() { return proxy_; }

 private:
  std::unique_ptr<Gatt2RemoteServiceServer> server_;

  fbg::RemoteServicePtr proxy_;
  fbl::RefPtr<bt::gatt::RemoteService> service_;
  fxl::WeakPtr<bt::gatt::testing::FakeClient> fake_client_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(FIDL_Gatt2RemoteServiceServerTest);
};

TEST_F(FIDL_Gatt2RemoteServiceServerTest, DiscoverCharacteristics) {
  bt::gatt::Properties properties =
      static_cast<bt::gatt::Properties>(bt::gatt::Property::kAuthenticatedSignedWrites) |
      static_cast<bt::gatt::Properties>(bt::gatt::Property::kExtendedProperties);
  bt::gatt::ExtendedProperties ext_properties =
      static_cast<bt::gatt::ExtendedProperties>(bt::gatt::ExtendedProperty::kReliableWrite) |
      static_cast<bt::gatt::ExtendedProperties>(bt::gatt::ExtendedProperty::kWritableAuxiliaries);
  constexpr bt::att::Handle kCharacteristicHandle(kServiceStartHandle + 1);
  constexpr bt::att::Handle kCharacteristicValueHandle(kCharacteristicHandle + 1);
  const bt::UUID kCharacteristicUuid(uint16_t{0x0000});
  bt::gatt::CharacteristicData characteristic(properties, ext_properties, kCharacteristicHandle,
                                              kCharacteristicValueHandle, kCharacteristicUuid);
  fake_client()->set_characteristics({characteristic});

  constexpr bt::att::Handle kDescriptorHandle(kCharacteristicValueHandle + 1);
  const bt::UUID kDescriptorUuid(uint16_t{0x0001});
  bt::gatt::DescriptorData descriptor(kDescriptorHandle, kDescriptorUuid);
  fake_client()->set_descriptors({descriptor});

  std::optional<std::vector<fbg::Characteristic>> fidl_characteristics;
  service_proxy()->DiscoverCharacteristics(
      [&](std::vector<fbg::Characteristic> chars) { fidl_characteristics = std::move(chars); });
  RunLoopUntilIdle();
  ASSERT_TRUE(fidl_characteristics.has_value());
  ASSERT_EQ(fidl_characteristics->size(), 1u);
  const fbg::Characteristic& fidl_characteristic = fidl_characteristics->front();

  ASSERT_TRUE(fidl_characteristic.has_handle());
  EXPECT_EQ(fidl_characteristic.handle().value, static_cast<uint64_t>(kCharacteristicHandle));

  ASSERT_TRUE(fidl_characteristic.has_type());
  EXPECT_EQ(fidl_characteristic.type().value, kCharacteristicUuid.value());

  ASSERT_TRUE(fidl_characteristic.has_properties());
  EXPECT_EQ(fidl_characteristic.properties(),
            static_cast<uint32_t>(fbg::CharacteristicPropertyBits::AUTHENTICATED_SIGNED_WRITES) |
                static_cast<uint32_t>(fbg::CharacteristicPropertyBits::RELIABLE_WRITE) |
                static_cast<uint32_t>(fbg::CharacteristicPropertyBits::WRITABLE_AUXILIARIES));

  EXPECT_FALSE(fidl_characteristic.has_permissions());

  ASSERT_TRUE(fidl_characteristic.has_descriptors());
  ASSERT_EQ(fidl_characteristic.descriptors().size(), 1u);
  const fbg::Descriptor& fidl_descriptor = fidl_characteristic.descriptors().front();

  ASSERT_TRUE(fidl_descriptor.has_handle());
  EXPECT_EQ(fidl_descriptor.handle().value, static_cast<uint64_t>(kDescriptorHandle));

  ASSERT_TRUE(fidl_descriptor.has_type());
  EXPECT_EQ(fidl_descriptor.type().value, kDescriptorUuid.value());

  EXPECT_FALSE(fidl_descriptor.has_permissions());
}

TEST_F(FIDL_Gatt2RemoteServiceServerTest, DiscoverCharacteristicsWithNoDescriptors) {
  bt::gatt::Properties properties = 0;
  bt::gatt::ExtendedProperties ext_properties = 0;
  constexpr bt::att::Handle kCharacteristicHandle(kServiceStartHandle + 1);
  constexpr bt::att::Handle kCharacteristicValueHandle(kCharacteristicHandle + 1);
  const bt::UUID kCharacteristicUuid(uint16_t{0x0000});
  bt::gatt::CharacteristicData characteristic(properties, ext_properties, kCharacteristicHandle,
                                              kCharacteristicValueHandle, kCharacteristicUuid);
  fake_client()->set_characteristics({characteristic});

  std::optional<std::vector<fbg::Characteristic>> fidl_characteristics;
  service_proxy()->DiscoverCharacteristics(
      [&](std::vector<fbg::Characteristic> chars) { fidl_characteristics = std::move(chars); });
  RunLoopUntilIdle();
  ASSERT_TRUE(fidl_characteristics.has_value());
  ASSERT_EQ(fidl_characteristics->size(), 1u);
  const fbg::Characteristic& fidl_characteristic = fidl_characteristics->front();
  EXPECT_FALSE(fidl_characteristic.has_descriptors());
}

}  // namespace
}  // namespace bthost
