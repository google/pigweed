// Copyright 2022 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_profiles/device_info_service.h"

#include <string_view>

#include "gtest/gtest.h"
#include "pw_bluetooth/gatt/server.h"

using namespace std::string_view_literals;

namespace pw::bluetooth_profiles {
namespace {

class FakeGattServer final : public bluetooth::gatt::Server {
 public:
  // Server overrides:
  void PublishService(
      const bluetooth::gatt::LocalServiceInfo& info,
      bluetooth::gatt::LocalServiceDelegate* delegate,
      Function<void(PublishServiceResult)>&& result_callback) override {
    ASSERT_EQ(delegate_, nullptr);
    delegate_ = delegate;
    ASSERT_EQ(published_info_, nullptr);
    published_info_ = &info;
    local_service_.emplace(this);
    result_callback(
        PublishServiceResult(std::in_place, &local_service_.value()));
  }

  // PublishService call argument getters:
  const bluetooth::gatt::LocalServiceInfo* published_info() const {
    return published_info_;
  }
  bluetooth::gatt::LocalServiceDelegate* delegate() const { return delegate_; }

 private:
  class FakeLocalService final : public bluetooth::gatt::LocalService {
   public:
    explicit FakeLocalService(FakeGattServer* fake_server)
        : fake_server_(fake_server) {}

    // LocalService overrides:
    void NotifyValue(
        const ValueChangedParameters& /* parameters */,
        ValueChangedCallback&& /* completion_callback */) override {
      FAIL();  // Unimplemented
    }
    void IndicateValue(
        const ValueChangedParameters& /* parameters */,
        Function<void(
            bluetooth::Result<bluetooth::gatt::Error>)>&& /* confirmation */)
        override {
      FAIL();  // Unimplemented
    }

   private:
    void UnpublishService() override { fake_server_->local_service_.reset(); }

    FakeGattServer* fake_server_;
  };

  // The LocalServiceInfo passed when PublishService was called.
  const bluetooth::gatt::LocalServiceInfo* published_info_ = nullptr;

  bluetooth::gatt::LocalServiceDelegate* delegate_ = nullptr;

  std::optional<FakeLocalService> local_service_;
};

TEST(DeviceInfoServiceTest, PublishAndReadTest) {
  FakeGattServer fake_server;

  constexpr auto kUsedFields = DeviceInfo::Field::kModelNumber |
                               DeviceInfo::Field::kSerialNumber |
                               DeviceInfo::Field::kSoftwareRevision;
  DeviceInfo device_info = {};
  const auto kModelNumber = "model"sv;
  device_info.model_number = as_bytes(span{kModelNumber});
  device_info.serial_number = as_bytes(span{"parallel_number"sv});
  device_info.software_revision = as_bytes(span{"rev123"sv});

  DeviceInfoService<kUsedFields, bluetooth::gatt::Handle{123}>
      device_info_service(device_info);

  bool called = false;
  device_info_service.PublishService(
      &fake_server,
      [&called](
          bluetooth::Result<bluetooth::gatt::Server::PublishServiceError> res) {
        EXPECT_TRUE(res.ok());
        called = true;
      });
  // The FakeGattServer calls the PublishService callback right away so our
  // callback should have been called already.
  EXPECT_TRUE(called);

  ASSERT_NE(fake_server.delegate(), nullptr);
  ASSERT_NE(fake_server.published_info(), nullptr);

  // Test that the published info looks correct.
  EXPECT_EQ(3u, fake_server.published_info()->characteristics.size());

  // Test that we can read the characteristics.
  for (auto& characteristic : fake_server.published_info()->characteristics) {
    bool read_callback_called = false;
    fake_server.delegate()->ReadValue(
        bluetooth::PeerId{1234},
        characteristic.handle,
        /*offset=*/0,
        [&read_callback_called](bluetooth::Result<bluetooth::gatt::Error,
                                                  span<const std::byte>> res) {
          EXPECT_TRUE(res.ok());
          EXPECT_NE(0u, res.value().size());
          read_callback_called = true;
        });
    // The DeviceInfoService always calls the callback from within ReadValue().
    EXPECT_TRUE(read_callback_called);
  }

  // Check the actual values.
  // The order of the characteristics in the LocalServiceInfo must be the order
  // in which the fields are listed in kCharacteristicFields, so the first
  // characteristic is the Model Number.
  span<const std::byte> read_value;
  fake_server.delegate()->ReadValue(
      bluetooth::PeerId{1234},
      fake_server.published_info()->characteristics[0].handle,
      /*offset=*/0,
      [&read_value](bluetooth::Result<bluetooth::gatt::Error,
                                      span<const std::byte>> res) {
        EXPECT_TRUE(res.ok());
        read_value = res.value();
      });
  EXPECT_EQ(read_value.size(), kModelNumber.size());  // "model" string.
  // DeviceInfoService keeps references to the values provides in the
  // DeviceInfo struct, not copies.
  EXPECT_EQ(read_value.data(),
            reinterpret_cast<const std::byte*>(kModelNumber.data()));

  // Read with an offset.
  const size_t kReadOffset = 3;
  fake_server.delegate()->ReadValue(
      bluetooth::PeerId{1234},
      fake_server.published_info()->characteristics[0].handle,
      kReadOffset,
      [&read_value](bluetooth::Result<bluetooth::gatt::Error,
                                      span<const std::byte>> res) {
        EXPECT_TRUE(res.ok());
        read_value = res.value();
      });
  EXPECT_EQ(read_value.size(), kModelNumber.size() - kReadOffset);
  EXPECT_EQ(
      read_value.data(),
      reinterpret_cast<const std::byte*>(kModelNumber.data()) + kReadOffset);
}

}  // namespace
}  // namespace pw::bluetooth_profiles
