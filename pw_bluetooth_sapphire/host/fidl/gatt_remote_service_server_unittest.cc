// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gatt_remote_service_server.h"

#include "gtest/gtest.h"
#include "helpers.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer_test.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service_manager.h"

namespace bthost {
namespace {

namespace fbgatt = fuchsia::bluetooth::gatt;

constexpr bt::PeerId kPeerId(1);

constexpr bt::att::Handle kServiceStartHandle = 0x0021;
constexpr bt::att::Handle kServiceEndHandle = 0x002C;
const bt::UUID kServiceUuid(uint16_t{0x180D});

class FIDL_GattRemoteServiceServerTest : public bt::gatt::testing::FakeLayerTest {
 public:
  FIDL_GattRemoteServiceServerTest() = default;
  ~FIDL_GattRemoteServiceServerTest() override = default;

  void SetUp() override {
    {
      auto [svc, client] = gatt()->AddPeerService(
          kPeerId, bt::gatt::ServiceData(bt::gatt::ServiceKind::PRIMARY, kServiceStartHandle,
                                         kServiceEndHandle, kServiceUuid));
      service_ = std::move(svc);
      fake_client_ = std::move(client);
    }

    fidl::InterfaceHandle<fbgatt::RemoteService> handle;
    server_ = std::make_unique<GattRemoteServiceServer>(service_, gatt()->AsWeakPtr(), kPeerId,
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

  fbgatt::RemoteServicePtr& service_proxy() { return proxy_; }

 private:
  std::unique_ptr<GattRemoteServiceServer> server_;

  fbgatt::RemoteServicePtr proxy_;
  fbl::RefPtr<bt::gatt::RemoteService> service_;
  fxl::WeakPtr<bt::gatt::testing::FakeClient> fake_client_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(FIDL_GattRemoteServiceServerTest);
};

TEST_F(FIDL_GattRemoteServiceServerTest, ReadByTypeSuccess) {
  constexpr bt::UUID kCharUuid(uint16_t{0xfefe});

  constexpr bt::att::Handle kHandle = kServiceStartHandle;
  const auto kValue = bt::StaticByteBuffer(0x00, 0x01, 0x02);
  const std::vector<bt::gatt::Client::ReadByTypeValue> kValues = {
      {kHandle, kValue.view(), /*maybe_truncated=*/false}};

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const bt::UUID& type, bt::att::Handle start, bt::att::Handle end, auto callback) {
        switch (read_count++) {
          case 0:
            callback(fpromise::ok(kValues));
            break;
          case 1:
            callback(fpromise::error(bt::gatt::Client::ReadByTypeError{
                bt::att::Status(bt::att::ErrorCode::kAttributeNotFound), start}));
            break;
          default:
            FAIL();
        }
      });

  std::optional<fbgatt::RemoteService_ReadByType_Result> fidl_result;
  service_proxy()->ReadByType(fidl_helpers::UuidToFidl(kCharUuid),
                              [&](auto cb_result) { fidl_result = std::move(cb_result); });

  RunLoopUntilIdle();
  ASSERT_TRUE(fidl_result.has_value());
  ASSERT_TRUE(fidl_result->is_response());
  const auto& response = fidl_result->response();
  ASSERT_EQ(1u, response.results.size());
  const fbgatt::ReadByTypeResult& result0 = response.results[0];
  ASSERT_TRUE(result0.has_id());
  EXPECT_EQ(result0.id(), static_cast<uint64_t>(kHandle));
  ASSERT_TRUE(result0.has_value());
  EXPECT_TRUE(
      ContainersEqual(bt::BufferView(result0.value().data(), result0.value().size()), kValue));
  EXPECT_FALSE(result0.has_error());
}

TEST_F(FIDL_GattRemoteServiceServerTest, ReadByTypeResultWithError) {
  constexpr bt::UUID kCharUuid(uint16_t{0xfefe});

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const bt::UUID& type, bt::att::Handle start, bt::att::Handle end, auto callback) {
        ASSERT_EQ(0u, read_count++);
        callback(fpromise::error(bt::gatt::Client::ReadByTypeError{
            bt::att::Status(bt::att::ErrorCode::kInsufficientAuthorization), kServiceEndHandle}));
      });

  std::optional<fbgatt::RemoteService_ReadByType_Result> fidl_result;
  service_proxy()->ReadByType(fidl_helpers::UuidToFidl(kCharUuid),
                              [&](auto cb_result) { fidl_result = std::move(cb_result); });

  RunLoopUntilIdle();
  ASSERT_TRUE(fidl_result.has_value());
  ASSERT_TRUE(fidl_result->is_response());
  const auto& response = fidl_result->response();
  ASSERT_EQ(1u, response.results.size());
  const fbgatt::ReadByTypeResult& result0 = response.results[0];
  ASSERT_TRUE(result0.has_id());
  EXPECT_EQ(result0.id(), static_cast<uint64_t>(kServiceEndHandle));
  EXPECT_FALSE(result0.has_value());
  ASSERT_TRUE(result0.has_error());
  EXPECT_EQ(fbgatt::Error::INSUFFICIENT_AUTHORIZATION, result0.error());
}

TEST_F(FIDL_GattRemoteServiceServerTest, ReadByTypeError) {
  constexpr bt::UUID kCharUuid(uint16_t{0xfefe});

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const bt::UUID& type, bt::att::Handle start, bt::att::Handle end, auto callback) {
        switch (read_count++) {
          case 0:
            callback(fpromise::error(bt::gatt::Client::ReadByTypeError{
                bt::att::Status(bt::HostError::kPacketMalformed), std::nullopt}));
            break;
          default:
            FAIL();
        }
      });

  std::optional<fbgatt::RemoteService_ReadByType_Result> fidl_result;
  service_proxy()->ReadByType(fidl_helpers::UuidToFidl(kCharUuid),
                              [&](auto cb_result) { fidl_result = std::move(cb_result); });

  RunLoopUntilIdle();
  ASSERT_TRUE(fidl_result.has_value());
  ASSERT_TRUE(fidl_result->is_err());
  const auto& err = fidl_result->err();
  EXPECT_EQ(fbgatt::Error::INVALID_RESPONSE, err);
}

TEST_F(FIDL_GattRemoteServiceServerTest, ReadByTypeInvalidParametersErrorClosesChannel) {
  constexpr bt::UUID kCharUuid = bt::gatt::types::kCharacteristicDeclaration;

  std::optional<zx_status_t> error_status;
  service_proxy().set_error_handler([&](zx_status_t status) { error_status = status; });

  std::optional<fbgatt::RemoteService_ReadByType_Result> fidl_result;
  service_proxy()->ReadByType(fidl_helpers::UuidToFidl(kCharUuid),
                              [&](auto cb_result) { fidl_result = std::move(cb_result); });

  RunLoopUntilIdle();
  EXPECT_FALSE(fidl_result.has_value());
  EXPECT_FALSE(service_proxy().is_bound());
  EXPECT_TRUE(error_status.has_value());
  EXPECT_EQ(ZX_ERR_INVALID_ARGS, error_status.value());
}

}  // namespace
}  // namespace bthost
