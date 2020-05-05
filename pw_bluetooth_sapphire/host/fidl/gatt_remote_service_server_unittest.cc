#include "gatt_remote_service_server.h"

#include "gtest/gtest.h"
#include "helpers.h"
#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_client.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/fake_layer.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service.h"
#include "src/connectivity/bluetooth/core/bt-host/gatt/remote_service_manager.h"

namespace bthost {

namespace {

namespace fbgatt = fuchsia::bluetooth::gatt;

constexpr bt::att::Handle kServiceStartHandle = 0x0021;
constexpr bt::att::Handle kServiceEndHandle = 0x002C;
const bt::UUID kServiceUuid((uint16_t)0x180D);

void NopStatusCallback(bt::att::Status) {}

};  // namespace

using TestingBase = ::gtest::TestLoopFixture;
class FIDL_GattRemoteServiceServerTest : public TestingBase {
 public:
  void SetUp() override {
    auto client = std::make_unique<bt::gatt::testing::FakeClient>(dispatcher());
    fake_client_ = client.get();

    mgr_ =
        std::make_unique<bt::gatt::internal::RemoteServiceManager>(std::move(client), dispatcher());

    service_ = SetUpFakeService(
        bt::gatt::ServiceData(kServiceStartHandle, kServiceEndHandle, kServiceUuid));

    fidl::InterfaceHandle<fbgatt::RemoteService> handle;
    server_ = std::make_unique<GattRemoteServiceServer>(
        service_, /*gatt=*/bt::gatt::testing::FakeLayer::Create(), handle.NewRequest());
    proxy_.Bind(std::move(handle));
  }

  void TearDown() override {
    // Clear any previous expectations that are based on the ATT Write Request,
    // so that write requests sent during RemoteService::ShutDown() are ignored.
    fake_client()->set_write_request_callback({});
  }

 protected:
  // Initializes a RemoteService based on |data|.
  fbl::RefPtr<bt::gatt::RemoteService> SetUpFakeService(const bt::gatt::ServiceData& data) {
    std::vector<bt::gatt::ServiceData> fake_services{{data}};
    fake_client()->set_primary_services(std::move(fake_services));

    mgr_->Initialize(NopStatusCallback);

    bt::gatt::ServiceList services;
    mgr_->ListServices(std::vector<bt::UUID>(),
                       [&services](auto status, bt::gatt::ServiceList cb_services) {
                         services = std::move(cb_services);
                       });

    RunLoopUntilIdle();

    ZX_DEBUG_ASSERT(services.size() == 1u);
    return services[0];
  }

  bt::gatt::testing::FakeClient* fake_client() const { return fake_client_; }
  fbgatt::RemoteServicePtr& service_proxy() { return proxy_; }

 private:
  std::unique_ptr<GattRemoteServiceServer> server_;
  fbgatt::RemoteServicePtr proxy_;

  std::unique_ptr<bt::gatt::internal::RemoteServiceManager> mgr_;

  // The memory is owned by |mgr_|.
  bt::gatt::testing::FakeClient* fake_client_;

  fbl::RefPtr<bt::gatt::RemoteService> service_;
};

TEST_F(FIDL_GattRemoteServiceServerTest, ReadByTypeSuccess) {
  constexpr bt::UUID kCharUuid((uint16_t)0xfefe);

  constexpr bt::att::Handle kHandle = kServiceStartHandle;
  const auto kValue = bt::StaticByteBuffer(0x00, 0x01, 0x02);
  const std::vector<bt::gatt::Client::ReadByTypeValue> kValues = {{kHandle, kValue.view()}};

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const bt::UUID& type, bt::att::Handle start, bt::att::Handle end, auto callback) {
        switch (read_count++) {
          case 0:
            callback(fit::ok(kValues));
            break;
          case 1:
            callback(fit::error(bt::gatt::Client::ReadByTypeError{
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
  constexpr bt::UUID kCharUuid((uint16_t)0xfefe);

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const bt::UUID& type, bt::att::Handle start, bt::att::Handle end, auto callback) {
        ASSERT_EQ(0u, read_count++);
        callback(fit::error(bt::gatt::Client::ReadByTypeError{
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
  constexpr bt::UUID kCharUuid((uint16_t)0xfefe);

  size_t read_count = 0;
  fake_client()->set_read_by_type_request_callback(
      [&](const bt::UUID& type, bt::att::Handle start, bt::att::Handle end, auto callback) {
        switch (read_count++) {
          case 0:
            callback(fit::error(bt::gatt::Client::ReadByTypeError{
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

};  // namespace bthost
